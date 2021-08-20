/*
gl_cubemaps.cpp - tools for cubemaps search & handling
Copyright (C) 2016 Uncle Mike

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
#include "com_model.h"
#include "ref_params.h"
#include "gl_local.h"
#include "gl_decals.h"
#include <mathlib.h>
#include "gl_world.h"

/*
=================
CL_FindNearestCubeMap

find the nearest cubemap for a given point
=================
*/
void CL_FindNearestCubeMap( const Vector &pos, mcubemap_t **result )
{
	if( !result ) return;

	float maxDist = 99999.0f;
	*result = NULL;

	for( int i = 0; i < world->num_cubemaps; i++ )
	{
		mcubemap_t *check = &world->cubemaps[i];
		float dist = VectorDistance( check->origin, pos );

		if( dist < maxDist )
		{
			*result = check;
			maxDist = dist;
		}
	}

	if( !*result )
	{
		// this may happens if map
		// doesn't have any cubemaps
		*result = &world->defaultCubemap;
	}
}

/*
=================
CL_FindNearestCubeMapForSurface

find the nearest cubemap on front of plane
=================
*/
void CL_FindNearestCubeMapForSurface( const Vector &pos, const msurface_t *surf, mcubemap_t **result )
{
	if( !result ) return;

	float maxDist = 99999.0f;
	mplane_t plane;
	*result = NULL;

	plane = *surf->plane;

	if( FBitSet( surf->flags, SURF_PLANEBACK ))
	{
		plane.normal = -plane.normal;
		plane.dist = -plane.dist;
	}

	for( int i = 0; i < world->num_cubemaps; i++ )
	{
		mcubemap_t *check = &world->cubemaps[i];
		float dist = VectorDistance( check->origin, pos );

		if( dist < maxDist && PlaneDiff( check->origin, &plane ) >= 0.0f )
		{
			*result = check;
			maxDist = dist;
		}
	}

	if( *result ) return;

	// fallback to default method
	CL_FindNearestCubeMap( pos, result );
}

/*
=================
CL_FindTwoNearestCubeMap

find the two nearest cubemaps for a given point
=================
*/
void CL_FindTwoNearestCubeMap( const Vector &pos, mcubemap_t **result1, mcubemap_t **result2 )
{
	if( !result1 || !result2 )
		return;

	float maxDist1 = 99999.0f;
	float maxDist2 = 99999.0f;
	*result1 = *result2 = NULL;

	for( int i = 0; i < world->num_cubemaps; i++ )
	{
		mcubemap_t *check = &world->cubemaps[i];
		float dist = VectorDistance( check->origin, pos );

		if( dist < maxDist1 )
		{
			*result1 = check;
			maxDist1 = dist;
		}
		else if( dist < maxDist2 && dist > maxDist1 )
		{
			*result2 = check;
			maxDist2 = dist;
		}
	}

	if( !*result1 )
	{
		// this may happens if map
		// doesn't have any cubemaps
		*result1 = &world->defaultCubemap;
	}

	if( !*result2 )
	{
		// this may happens if map
		// doesn't have any cubemaps
		*result2 = *result1;
	}
}

/*
=================
CL_FindTwoNearestCubeMapForSurface

find the two nearest cubemaps on front of plane
=================
*/
void CL_FindTwoNearestCubeMapForSurface( const Vector &pos, const msurface_t *surf, mcubemap_t **result1, mcubemap_t **result2 )
{
	if( !result1 || !result2 ) return;

	float maxDist1 = 99999.0f;
	float maxDist2 = 99999.0f;
	mplane_t plane;
	*result1 = NULL;
	*result2 = NULL;

	plane = *surf->plane;

	if( FBitSet( surf->flags, SURF_PLANEBACK ))
	{
		plane.normal = -plane.normal;
		plane.dist = -plane.dist;
	}

	for( int i = 0; i < world->num_cubemaps; i++ )
	{
		mcubemap_t *check = &world->cubemaps[i];
		float dist = VectorDistance( check->origin, pos );

		if( dist < maxDist1 && PlaneDiff( check->origin, &plane ) >= 0.0f )
		{
			*result1 = check;
			maxDist1 = dist;
		}
		else if( dist < maxDist2 && dist > maxDist1 )
		{
			*result2 = check;
			maxDist2 = dist;
		}
	}

	if( *result1 )
	{
		if( !*result2 )
			*result2 = *result1;
		return;
	}

	// fallback to default method
	CL_FindTwoNearestCubeMap( pos, result1, result2 );
}

/*
=================
CL_BuildCubemaps_f

force to rebuilds all the cubemaps
in the scene
=================
*/
void CL_BuildCubemaps_f( void )
{
	mcubemap_t *m = &world->defaultCubemap;
	FREE_TEXTURE( m->texture );
	m->valid = ( m->texture = false );

	for( int i = 0; i < world->num_cubemaps; i++ )
	{
		mcubemap_t *m = &world->cubemaps[i];
		FREE_TEXTURE( m->texture );
		m->valid = ( m->texture = false );
	}

	if( FBitSet( world->features, WORLD_HAS_SKYBOX ))
		world->build_default_cubemap = true;

	world->rebuilding_cubemaps = CMREBUILD_CHECKING;
	world->loading_cubemaps = true;
}

void R_InitCubemaps()
{
	gEngfuncs.pfnAddCommand("buildcubemaps", CL_BuildCubemaps_f);
}
