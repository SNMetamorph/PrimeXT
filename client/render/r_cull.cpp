/*
r_cull.cpp - render culling routines
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "entity_types.h"
#include "mathlib.h"

#define BACKFACE_EPSILON	0.01f

/*
=============================================================

FRUSTUM AND PVS CULLING

=============================================================
*/
/*
=================
R_CullBoxExt

Returns true if the box is completely outside the frustum
=================
*/
bool R_CullBoxExt( const mplane_t frustum[6], const Vector &mins, const Vector &maxs, unsigned int clipflags )
{
	unsigned int	i, bit;
	const mplane_t	*p;

	// client.dll may use additional passes for render custom mirrors etc
	if( r_nocull->value )
		return false;

	for( i = 6, bit = 1; i > 0; i--, bit <<= 1 )
	{
		p = &frustum[6 - i];

		if( !( clipflags & bit ))
			continue;

		switch( p->signbits )
		{
		case 0:
			if( p->normal.x * maxs.x + p->normal.y * maxs.y + p->normal.z * maxs.z < p->dist )
				return true;
			break;
		case 1:
			if( p->normal.x * mins.x + p->normal.y * maxs.y + p->normal.z * maxs.z < p->dist )
				return true;
			break;
		case 2:
			if( p->normal.x * maxs.x + p->normal.y * mins.y + p->normal.z * maxs.z < p->dist )
				return true;
			break;
		case 3:
			if( p->normal.x * mins.x + p->normal.y * mins.y + p->normal.z * maxs.z < p->dist )
				return true;
			break;
		case 4:
			if( p->normal.x * maxs.x + p->normal.y * maxs.y + p->normal.z * mins.z < p->dist )
				return true;
			break;
		case 5:
			if( p->normal.x * mins.x + p->normal.y * maxs.y + p->normal.z * mins.z < p->dist )
				return true;
			break;
		case 6:
			if( p->normal.x * maxs.x + p->normal.y * mins.y + p->normal.z * mins.z < p->dist )
				return true;
			break;
		case 7:
			if( p->normal.x * mins.x + p->normal.y * mins.y + p->normal.z * mins.z < p->dist )
				return true;
			break;
		default:
			return false;
		}
	}
	return false;
}

/*
=================
R_CullSphereExt

Returns true if the sphere is completely outside the frustum
=================
*/
bool R_CullSphereExt( const mplane_t frustum[6], const Vector &centre, float radius, unsigned int clipflags )
{
	unsigned int i, bit;
	const mplane_t *p;

	// client.dll may use additional passes for render custom mirrors etc
	if( r_nocull->value )
		return false;

	for( i = 6, bit = 1; i > 0; i--, bit <<= 1 )
	{
		p = &frustum[6 - i];

		if(!( clipflags & bit )) continue;
		if( DotProduct( centre, p->normal ) - p->dist <= -radius )
			return true;
	}
	return false;
}

/*
===================
R_VisCullBox
===================
*/
bool R_VisCullBox( const Vector &mins, const Vector &maxs )
{
	Vector extmins, extmaxs;
	mnode_t *localstack[2048];
	int stackdepth = 0;

	if( !worldmodel || !RI.drawWorld )
		return false;

	if( r_novis->value )
		return false;

	for( int i = 0; i < 3; i++ )
	{
		extmins[i] = mins[i] - 4;
		extmaxs[i] = maxs[i] + 4;
	}

	for( mnode_t *node = worldmodel->nodes; node; )
	{
		if( node->visframe != tr.visframecount )
		{
			if( !stackdepth )
				return true;
			node = localstack[--stackdepth];
			continue;
		}

		if( node->contents < 0 )
			return false;

		int s = BoxOnPlaneSide( extmins, extmaxs, node->plane ) - 1;

		if( s < 2 )
		{
			node = node->children[s];
			continue;
		}

		// go down both sides
		if( stackdepth < ARRAYSIZE( localstack ))
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}

	return true;
}

/*
===================
R_VisCullSphere
===================
*/
bool R_VisCullSphere( const Vector &origin, float radius )
{
	mnode_t *localstack[2048];
	int stackdepth = 0;

	if( !worldmodel || !RI.drawWorld )
		return false;

	if( r_novis->value )
		return false;

	radius += 4;

	for( mnode_t *node = worldmodel->nodes; ; )
	{
		if( node->visframe != tr.visframecount )
		{
			if( !stackdepth )
				return true;
			node = localstack[--stackdepth];
			continue;
		}

		if( node->contents < 0 )
			return false;

		float dist = PlaneDiff( origin, node->plane );

		if( dist > radius )
		{
			node = node->children[0];
			continue;
		}
		else if( dist < -radius )
		{
			node = node->children[1];
			continue;
		}

		// go down both sides
		if( stackdepth < ARRAYSIZE( localstack ))
			localstack[stackdepth++] = node->children[0];
		node = node->children[1];
	}

	return true;
}

/*
=============
R_CullModel
=============
*/
bool R_CullModel( cl_entity_t *e, const Vector &origin, const Vector &mins, const Vector &maxs, float radius )
{
	if( e == GET_VIEWMODEL( ))
	{
		if( RI.params & RP_NONVIEWERREF )
			return true;
		return false;
	}

	// don't reflect this entity in mirrors
	if( e->curstate.effects & EF_NOREFLECT && RI.params & RP_MIRRORVIEW )
		return true;

	// draw only in mirrors
	if( e->curstate.effects & EF_REFLECTONLY && !( RI.params & RP_MIRRORVIEW ))
		return true;

	// never draw playermodel for himself flashlight while shadowpass is active
	if( RI.params & RP_SHADOWVIEW && RI.currentlight != NULL )
	{
		if( UTIL_IsLocal( e->index ) && UTIL_IsLocal( RI.currentlight->key ))
			return true;
          }

	if( RP_LOCALCLIENT( e ))
	{
		if( RI.params & RP_FORCE_NOPLAYER )
			return true;

		if( !RI.thirdPerson && UTIL_IsLocal( RI.refdef.viewentity ))
		{
			// player can view himself from the portal camera
			if(!( RI.params & ( RP_MIRRORVIEW|RP_PORTALVIEW|RP_SCREENVIEW|RP_SHADOWVIEW )))
				return true;
		}
	}

	if( R_CullSphere( origin, radius, RI.clipFlags ))
		return true;

	if( RI.params & ( RP_SKYPORTALVIEW|RP_PORTALVIEW ))
	{
		if( R_VisCullSphere( e->origin, radius ))
			return true;
	}

	return false;
}

/*
=================
R_CullSurface

cull invisible surfaces
=================
*/
bool R_CullSurfaceExt( msurface_t *surf, const mplane_t frustum[6], unsigned int clipflags )
{
	mextrasurf_t	*info;
	cl_entity_t	*e = RI.currententity;

	if( !surf || !surf->texinfo || !surf->texinfo->texture )
		return true;

	if( surf->flags & SURF_WATERCSG && !( RI.currententity->curstate.effects & EF_NOWATERCSG ))
		return true;

	if( surf->flags & SURF_NOCULL )
		return false;

	// don't cull transparent surfaces because we should be draw decals on them
	if( surf->pdecals && ( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd ))
		return false;

	if( r_nocull->value )
		return false;

	// world surfaces can be culled by vis frame too
	if( RI.currententity == GET_ENTITY( 0 ) && surf->visframe != tr.framecount )
		return true;

	if( r_faceplanecull->value && glState.faceCull != 0 )
	{
		if(!(surf->flags & SURF_DRAWTURB) || !RI.currentWaveHeight )
		{
			if( surf->plane->normal != g_vecZero )
			{
				float	dist;

				if( RI.drawOrtho ) dist = surf->plane->normal[2];
				else dist = PlaneDiff( tr.modelorg, surf->plane );

				if( glState.faceCull == GL_FRONT || ( RI.params & RP_MIRRORVIEW ))
				{
					if( surf->flags & SURF_PLANEBACK )
					{
						if( dist >= -BACKFACE_EPSILON )
							return true; // wrong side
					}
					else
					{
						if( dist <= BACKFACE_EPSILON )
							return true; // wrong side
					}
				}
				else if( glState.faceCull == GL_BACK )
				{
					if( surf->flags & SURF_PLANEBACK )
					{
						if( dist <= BACKFACE_EPSILON )
							return true; // wrong side
					}
					else
					{
						if( dist >= -BACKFACE_EPSILON )
							return true; // wrong side
					}
				}
			}
		}
	}

	info = SURF_INFO( surf, RI.currentmodel );

	return ( clipflags && R_CullBoxExt( frustum, info->mins, info->maxs, clipflags ));
}
