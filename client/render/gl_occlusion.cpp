/*
gl_occlusion.cpp - occlusion query implementation class
this code written for Paranoia 2: Savior modification
Copyright (C) 2015 Uncle Mike

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
#include "const.h"
#include "gl_local.h"
#include <utlarray.h>
#include "gl_occlusion.h"
#include "gl_world.h"
#include "gl_cvars.h"
#include "gl_debug.h"

/*
===============
GL_AllocOcclusionQuery

===============
*/
void GL_AllocOcclusionQuery( msurface_t *surf )
{
	if( !GL_Support( R_OCCLUSION_QUERIES_EXT ) || surf->info->query )
		return;

	pglGenQueriesARB( 1, &surf->info->query );
}

/*
===============
GL_DeleteOcclusionQuery

===============
*/
void GL_DeleteOcclusionQuery( msurface_t *surf )
{
	if( !GL_Support( R_OCCLUSION_QUERIES_EXT ) || !surf->info->query )
		return;

	pglDeleteQueriesARB( 1, &surf->info->query );
}

/*
===============
GL_DrawOcclusionCube

===============
*/
static void GL_DrawOcclusionCube( const Vector &absmin, const Vector &absmax )
{
	vec3_t	bbox[8];
	int	i;

	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
  		bbox[i][0] = ( i & 1 ) ? absmin[0] : absmax[0];
  		bbox[i][1] = ( i & 2 ) ? absmin[1] : absmax[1];
  		bbox[i][2] = ( i & 4 ) ? absmin[2] : absmax[2];
	}

	pglBegin( GL_QUADS );

	for( i = 0; i < 6; i++ )
	{
		pglVertex3fv( bbox[g_boxpnt[i][0]] );
		pglVertex3fv( bbox[g_boxpnt[i][1]] );
		pglVertex3fv( bbox[g_boxpnt[i][2]] );
		pglVertex3fv( bbox[g_boxpnt[i][3]] );
	}
	pglEnd();
}

/*
===============
GL_TestSurfaceOcclusion

===============
*/
void GL_TestSurfaceOcclusion( msurface_t *surf )
{
	mextrasurf_t	*es = surf->info;
	Vector		absmin, absmax;
	word		cached_matrix;
	Vector		normal;

	if( !es->query || FBitSet( surf->flags, SURF_QUEUED ))
		return; // we already have the query

	if( !es->parent ) cached_matrix = WORLD_MATRIX;
	else cached_matrix = es->parent->hCachedMatrix;
	gl_state_t *glm = GL_GetCache( cached_matrix );

	if( FBitSet( surf->flags, SURF_PLANEBACK ))
		normal = -surf->plane->normal;
	else normal = surf->plane->normal;

	// place above surface
	absmin = es->mins + normal * 5.0f;
	absmax = es->maxs + normal * 5.0f;
	ExpandBounds( absmin, absmax, 2.0f );

	if( cached_matrix != WORLD_MATRIX )
		TransformAABB( glm->transform, es->mins, es->maxs, absmin, absmax );
	pglBeginQueryARB( GL_SAMPLES_PASSED_ARB, es->query );
	GL_DrawOcclusionCube( absmin, absmax );
	pglEndQueryARB( GL_SAMPLES_PASSED_ARB );

	// now we have a valid query
	SetBits( surf->flags, SURF_QUEUED );
}

/*
===============
GL_TestSurfaceOcclusion

===============
*/
void GL_DebugSurfaceOcclusion( msurface_t *surf )
{
	mextrasurf_t	*es = surf->info;
	Vector		absmin, absmax;
	word		cached_matrix;
	Vector		normal;

	if( !FBitSet( surf->flags, SURF_QUEUED ))
		return; // draw only queue

	if( !es->parent ) cached_matrix = WORLD_MATRIX;
	else cached_matrix = es->parent->hCachedMatrix;
	gl_state_t *glm = GL_GetCache( cached_matrix );

	if( FBitSet( surf->flags, SURF_PLANEBACK ))
		normal = -surf->plane->normal;
	else normal = surf->plane->normal;

	// place above surface
	absmin = es->mins + normal * 5.0f;
	absmax = es->maxs + normal * 5.0f;
	ExpandBounds( absmin, absmax, 2.0f );

	if( cached_matrix != WORLD_MATRIX )
		TransformAABB( glm->transform, es->mins, es->maxs, absmin, absmax );
	GL_DrawOcclusionCube( absmin, absmax );
}

/*
================
R_RenderOcclusionList
================
*/
void R_RenderSurfOcclusionList( void )
{
	ZoneScoped;

	int	i;

	if( !RP_NORMALPASS() || !CVAR_TO_BOOL( r_occlusion_culling ))
		return;

	if( !RI->frame.num_subview_faces )
		return;

	GL_DEBUG_SCOPE();
	pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	GL_DepthMask( GL_FALSE );
	GL_AlphaTest( GL_FALSE );
	GL_BindShader( NULL );

	for( i = 0; i < RI->frame.num_subview_faces; i++ )
		GL_TestSurfaceOcclusion( RI->frame.subview_faces[i] );

	pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	GL_DepthMask( GL_TRUE );
	pglFlush();

	if( r_occlusion_culling->value < 2.0f )
		return;

	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	GL_Blend( GL_FALSE );

	for( i = 0; i < RI->frame.num_subview_faces; i++ )
		GL_DebugSurfaceOcclusion( RI->frame.subview_faces[i] );
}

/*
================
GL_SurfaceOccluded
================
*/
bool GL_SurfaceOccluded( msurface_t *surf )
{
	mextrasurf_t	*es = surf->info;
	GLuint		sampleCount = 0;
	GLint		available = false;

	if( !RP_NORMALPASS() || !CVAR_TO_BOOL( r_occlusion_culling ))
		return false;

	if( !es->query ) return false;

	if( !FBitSet( surf->flags, SURF_QUEUED ))
	{
		// occlusion is no more actual
		ClearBits( surf->flags, SURF_OCCLUDED );
		return false;
	}

	// i hope results will be arrived on a next frame...
	pglGetQueryObjectivARB( es->query, GL_QUERY_RESULT_AVAILABLE_ARB, &available );

	// NOTE: if we can't get actual information about query results
	// assume that object was visible and cull him with default methods: frustum, pvs etc
	if( !available ) return false;

	pglGetQueryObjectuivARB( es->query, GL_QUERY_RESULT_ARB, &sampleCount );
	ClearBits( surf->flags, SURF_QUEUED ); // we catch the results, so query is outdated
	if( sampleCount == 0 ) SetBits( surf->flags, SURF_OCCLUDED );
	else ClearBits( surf->flags, SURF_OCCLUDED );
	if( !sampleCount ) r_stats.c_occlusion_culled++;

	return (FBitSet( surf->flags, SURF_OCCLUDED ) != 0);
}