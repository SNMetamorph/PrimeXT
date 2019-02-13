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
#include "r_world.h"
#include "mathlib.h"

/*
=============================================================

FRUSTUM AND PVS CULLING

=============================================================
*/
/*
=============
R_CullModel
=============
*/
bool R_CullModel( cl_entity_t *e, const Vector &absmin, const Vector &absmax )
{
	if( e == GET_VIEWMODEL( ))
	{
		if( FBitSet( RI->params, RP_NONVIEWERREF ))
			return true;
		return false;
	}

	// don't reflect this entity in mirrors
	if( FBitSet( e->curstate.effects, EF_NOREFLECT ) && FBitSet( RI->params, RP_MIRRORVIEW ))
		return true;

	// draw only in mirrors
	if( FBitSet( e->curstate.effects, EF_REFLECTONLY ) && !FBitSet( RI->params, RP_MIRRORVIEW ))
		return true;

	// never draw playermodel for himself flashlight while shadowpass is active
	if( FBitSet( RI->params, RP_SHADOWVIEW ) && RI->currentlight != NULL )
	{
		if( UTIL_IsLocal( e->index ) && UTIL_IsLocal( RI->currentlight->key ))
			return true;
          }

	if( RP_LOCALCLIENT( e ))
	{
		if( FBitSet( RI->params, RP_FORCE_NOPLAYER ))
			return true;

		if( !FBitSet( RI->params, RP_THIRDPERSON ) && UTIL_IsLocal( RI->viewentity ))
		{
			// player can view himself from the portal camera
			if(!( RI->params & ( RP_MIRRORVIEW|RP_PORTALVIEW|RP_SCREENVIEW|RP_SHADOWVIEW )))
				return true;
		}
	}

	return R_CullBox( absmin, absmax );
}

/*
=================
R_CullSurface

cull invisible surfaces
=================
*/
int R_CullSurfaceExt( msurface_t *surf, CFrustum *frustum )
{
	cl_entity_t	*e = RI->currententity;

	if( !surf || !surf->texinfo || !surf->texinfo->texture )
		return CULL_OTHER;

	if( CVAR_TO_BOOL( r_nocull ))
		return CULL_VISIBLE;

	// only static ents can be culled by frustum
	if( !R_StaticEntity( e )) frustum = NULL;

	if( surf->plane->normal != g_vecZero )
	{
		float	dist;

		if( FBitSet( RI->params, RP_OVERVIEW ))
		{
			Vector	orthonormal;

			if( e == GET_ENTITY( 0 )) orthonormal[2] = surf->plane->normal[2];
			else orthonormal = RI->objectMatrix.VectorRotate( surf->plane->normal );

			dist = orthonormal.z;
		}
		else dist = PlaneDiff( tr.modelorg, surf->plane );

		if( glState.faceCull == GL_FRONT )
		{
			if( surf->flags & SURF_PLANEBACK )
			{
				if( dist >= -BACKFACE_EPSILON )
					return CULL_BACKSIDE; // wrong side
			}
			else
			{
				if( dist <= BACKFACE_EPSILON )
					return CULL_BACKSIDE; // wrong side
			}
		}
		else if( glState.faceCull == GL_BACK )
		{
			if( surf->flags & SURF_PLANEBACK )
			{
				if( dist <= BACKFACE_EPSILON )
					return CULL_BACKSIDE; // wrong side
			}
			else
			{
				if( dist >= -BACKFACE_EPSILON )
					return CULL_BACKSIDE; // wrong side
			}
		}
	}

	if( frustum && frustum->CullBox( surf->info->mins, surf->info->maxs ))
		return CULL_FRUSTUM;

	return CULL_VISIBLE;
}

/*
================
R_CullNodeTopView

cull node by user rectangle (simple scissor)
================
*/
bool R_CullNodeTopView( const mworldnode_t *node )
{
	Vector2D	delta, size;
	Vector	center, half;

	// build the node center and half-diagonal
	center = (node->mins + node->maxs) * 0.5f;
	half = node->maxs - center;

	// cull against the screen frustum or the appropriate area's frustum.
	delta.x = center.x - world->orthocenter.x;
	delta.y = center.y - world->orthocenter.y;
	size.x = half.x + world->orthohalf.x;
	size.y = half.y + world->orthohalf.y;

	return ( fabs( delta.x ) > size.x ) || ( fabs( delta.y ) > size.y );
}
