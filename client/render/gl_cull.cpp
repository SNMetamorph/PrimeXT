/*
gl_cull.cpp - render culling routines
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
#include "gl_local.h"
#include "entity_types.h"
#include "mathlib.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_cvars.h"

/*
=============
R_CullModel
=============
*/
bool R_CullModel( cl_entity_t *e, const Vector &absmin, const Vector &absmax )
{
	if (e == GET_VIEWMODEL())
	{
		if( RI->params & RP_NONVIEWERREF )
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
		if( UTIL_IsLocal( e->index ) && RI->currentlight->key == FLASHLIGHT_KEY )
			return true;
	}
	
	if (RP_LOCALCLIENT(e))
	{
		if (FBitSet(RI->params, RP_FORCE_NOPLAYER))
			return true;

		if (!FBitSet(RI->params, RP_THIRDPERSON) && UTIL_IsLocal(RI->view.entity))
		{
			// player can view himself from the portal camera
			if (!FBitSet(RI->params, RP_MIRRORVIEW | RP_SHADOWVIEW | RP_SCREENVIEW | RP_PORTALVIEW))
				return true;
		}
	}

	return R_CullBox( absmin, absmax );
}

/*
================
R_CullBrushModel

Cull brush model by bbox
================
*/
bool R_CullBrushModel( cl_entity_t *e )
{
	gl_state_t *glm = GL_GetCache( e->hCachedMatrix );
	model_t	*clmodel = e->model;
	Vector	absmin, absmax;

	if( !e || !e->model )
		return true;

	if( e->angles != g_vecZero )
	{
		absmin = e->origin - clmodel->radius;
		absmax = e->origin + clmodel->radius;
	}
	else
	{
		absmin = e->origin + clmodel->mins;
		absmax = e->origin + clmodel->maxs;
	}
	tr.modelorg = glm->GetModelOrigin();

	return R_CullModel( e, absmin, absmax );
}

static bool R_SunLightShadow( void )
{
	if( RI->currentlight && RI->currentlight->type == LIGHT_DIRECTIONAL && FBitSet( RI->params, RP_SHADOWVIEW ))
		return true;
	return false;
}

/*
=================
R_AllowFacePlaneCulling

allow to culling backfaces
=================
*/
static bool R_AllowFacePlaneCulling( msurface_t *surf )
{
	if( !glState.faceCull )
		return false;

	if( FBitSet( surf->flags, SURF_TWOSIDE ))
		return false;

	if( !FBitSet( RI->params, RP_SHADOWVIEW ) && RI->currentlight && RI->currentlight->type == LIGHT_DIRECTIONAL )
		return false;

	// don't cull transparent surfaces because we should be draw decals on them
	if( FBitSet( surf->flags, SURF_HAS_DECALS ) && !R_OpaqueEntity( RI->currententity ))
		return false;

	return true;
}

/*
=================
R_CullSurface

cull invisible surfaces
=================
*/
int R_CullSurface( msurface_t *surf, const Vector &vieworg, CFrustum *frustum, int clipFlags )
{
	cl_entity_t *e = RI->currententity;

	if( !surf || !surf->texinfo || !surf->texinfo->texture )
		return CULL_OTHER;

	if( FBitSet( RI->params, RP_WATERPASS ) && R_WaterEntity( e->model ))
		return CULL_OTHER; // don't render water from waterplane reflection

	if( CVAR_TO_BOOL( r_nocull ))
		return CULL_VISIBLE;

	if( R_SunLightShadow( ))
		return CULL_VISIBLE;

	// because light or shadow passes required both sides for right self-shadowing
	if( R_AllowFacePlaneCulling( surf ))
	{
		float	dist;

		// can use normal.z for world (optimisation)
		if( FBitSet( RI->params, RP_DRAW_OVERVIEW ))
		{
			Vector	orthonormal;

			if( e == GET_ENTITY( 0 ) || R_StaticEntity( e ))
			{
				orthonormal.z = surf->plane->normal.z;
			}
			else
			{
				matrix4x4	m = matrix4x4( g_vecZero, e->angles );
				orthonormal = m.VectorRotate( surf->plane->normal );
            }

			dist = orthonormal.z;
		}
		else dist = PlaneDiff( vieworg, surf->plane );

		if( glState.faceCull == GL_FRONT || (RI->params & (RP_MIRRORVIEW | RP_PORTALVIEW)))
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

	if( frustum && frustum->CullBoxFast( surf->info->mins, surf->info->maxs, clipFlags ))
		return CULL_FRUSTUM;
	return CULL_VISIBLE;
}

/*
================
R_CullNodeTopView

cull node by user rectangle (simple scissor)
================
*/
bool R_CullNodeTopView( mnode_t *node )
{
	Vector2D	delta, size;
	Vector	center, half, mins, maxs;

	// build the node center and half-diagonal
	mins = node->minmaxs, maxs = node->minmaxs + 3;
	center = (mins + maxs) * 0.5f;
	half = maxs - center;

	// cull against the screen frustum or the appropriate area's frustum.
	delta.x = center.x - world->orthocenter.x;
	delta.y = center.y - world->orthocenter.y;
	size.x = half.x + world->orthohalf.x;
	size.y = half.y + world->orthohalf.y;

	return ( fabs( delta.x ) > size.x ) || ( fabs( delta.y ) > size.y );
}
