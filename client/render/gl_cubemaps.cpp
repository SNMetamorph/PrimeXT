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
#include "gl_cvars.h"

static Vector env_dir[] =
{
	Vector(1.0f,  0.0f,  0.0f),
	Vector(-1.0f,  0.0f,  0.0f),
	Vector(0.0f,  1.0f,  0.0f),
	Vector(0.0f, -1.0f,  0.0f),
	Vector(0.0f,  0.0f,  1.0f),
	Vector(0.0f,  0.0f, -1.0f)
};

void R_InitCubemaps()
{
	gEngfuncs.pfnAddCommand("buildcubemaps", CL_BuildCubemaps_f);
}


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

/*
==================
Mod_FreeCubemap

unload a given cubemap
==================
*/
static void Mod_FreeCubemap(mcubemap_t *m)
{
	if (m->valid && m->texture && m->texture != tr.whiteCubeTexture)
		FREE_TEXTURE(m->texture);

	memset(m, 0, sizeof(*m));
}

/*
==================
Mod_FreeCubemaps

purge all the cubemaps
from current level
==================
*/
void Mod_FreeCubemaps()
{
	for (int i = 0; i < world->num_cubemaps; i++)
		Mod_FreeCubemap(&world->cubemaps[i]);
	Mod_FreeCubemap(&world->defaultCubemap);

	world->rebuilding_cubemaps = CMREBUILD_INACTIVE;
	world->build_default_cubemap = false;
	world->loading_cubemaps = false;
	world->cubemap_build_number = 0;
	world->num_cubemaps = 0;
}

/*
==================
Mod_CheckCubemap

check cubemap sides for valid
==================
*/
static bool Mod_CheckCubemap(const char *name)
{
	const char *suf[6] = { "px", "nx", "py", "ny", "pz", "nz" };
	int	valid_sides = 0;
	char	sidename[64];
	int	iCompare;

	// FIXME: potentially unsafe checking: looking for DDS_CUBEMAP bit?
	if (FILE_EXISTS(va("maps/env/%s/%s.dds", world->name, name)))
		return true;

	for (int i = 0; i < 6; i++)
	{
		Q_snprintf(sidename, sizeof(sidename), "maps/env/%s/%s%s.tga", world->name, name, suf[i]);

		if (COMPARE_FILE_TIME(worldmodel->name, sidename, &iCompare) && iCompare <= 0)
			valid_sides++;
	}

	return (valid_sides == 6) ? true : false;
}

/*
==================
Mod_DeleteCubemap

remove cubemap images from HDD
==================
*/
static void Mod_DeleteCubemap(const char *name)
{
	const char *suf[6] = { "px", "nx", "py", "ny", "pz", "nz" };
	char	sidename[64];

	for (int i = 0; i < 6; i++)
	{
		Q_snprintf(sidename, sizeof(sidename), "maps/env/%s/%s%s.tga", world->name, name, suf[i]);

		if (FILE_EXISTS(sidename))
			Sys_RemoveFile(sidename);
	}
}

/*
==================
Mod_LoadCubemap

load cubemap into
video memory
==================
*/
static bool Mod_LoadCubemap(mcubemap_t *m)
{
	int flags = 0;

	if (m->valid && !m->texture)
	{
		if (GL_Support(R_SEAMLESS_CUBEMAP))
			SetBits(flags, TF_BORDER);	// seamless cubemaps have support for border
		else SetBits(flags, TF_CLAMP); // default method
		m->texture = LOAD_TEXTURE(m->name, NULL, 0, flags);

		// make sure what is really cubemap
		if (RENDER_GET_PARM(PARM_TEX_TARGET, m->texture) == GL_TEXTURE_CUBE_MAP_ARB)
			m->valid = true;
		else m->valid = false;

		// NOTE: old DDS cubemaps has no mip-levels
		m->numMips = RENDER_GET_PARM(PARM_TEX_MIPCOUNT, m->texture);
	}

	return m->valid;
}

/*
==================
GL_LoadAndRebuildCubemaps

rebuild cubemaps that older than bspfile
loading actual cubemaps into videomemory
==================
*/
void GL_LoadAndRebuildCubemaps(RefParams refParams)
{
	if (!world->loading_cubemaps && world->rebuilding_cubemaps == CMREBUILD_INACTIVE)
		return; // job is done

	// we are in cubemap-rendering mode
	if (FBitSet(refParams, RP_ENVVIEW | RP_SKYVIEW))
		return;

	if (world->rebuilding_cubemaps != CMREBUILD_INACTIVE)
	{
		if (world->build_default_cubemap)
		{
			mcubemap_t *cm = &world->defaultCubemap;

			if (world->rebuilding_cubemaps == CMREBUILD_WAITING)
			{
				cm->valid = Mod_CheckCubemap("default"); // need for rebuild?
				world->rebuilding_cubemaps = CMREBUILD_CHECKING;

				if (!cm->valid)
					ALERT(at_error, "GL_RebuildCubemaps: can't build default cubemap\n");
				world->build_default_cubemap = false; // done
			}

			if (world->build_default_cubemap)
			{
				Mod_DeleteCubemap("default");

				// NOTE: engine function EnvShot not makes shots immediately
				// but create a queue. So we will wait for a next frame
				ENV_SHOT(cm->origin, va("%s.tga", cm->name), false, cm->size);
				world->rebuilding_cubemaps = CMREBUILD_WAITING;
				return;
			}
		}

		for (; world->cubemap_build_number < world->num_cubemaps; world->cubemap_build_number++)
		{
			mcubemap_t *cm = &world->cubemaps[world->cubemap_build_number];

			if (world->rebuilding_cubemaps == CMREBUILD_WAITING)
			{
				cm->valid = Mod_CheckCubemap(va("cube#%i", world->cubemap_build_number)); // need for rebuild?
				world->rebuilding_cubemaps = CMREBUILD_CHECKING;

				if (!cm->valid)
				{
					ALERT(at_error, "GL_RebuildCubemaps: can't build cube#%i\n", world->cubemap_build_number);
					continue; // to avoid infinity cycle
				}
			}

			if (cm->valid) continue; // it's valid cubemap

			Mod_DeleteCubemap(va("cube#%i", world->cubemap_build_number));

			// NOTE: engine function EnvShot not makes shots immediately
			// but create a queue. So we will wait for a next frame
			ENV_SHOT(cm->origin, va("%s.tga", cm->name), false, cm->size);
			world->rebuilding_cubemaps = CMREBUILD_WAITING;
			return;
		}

		// we reached the end of list
		// next frame will be restored gamma
		SetBits(cv_brightness->flags, FCVAR_CHANGED);
		SetBits(cv_gamma->flags, FCVAR_CHANGED);
		world->rebuilding_cubemaps = CMREBUILD_INACTIVE;
		world->cubemap_build_number = 0;
		tr.params_changed = true;
		tr.glsl_valid_sequence++;
		tr.fClearScreen = false;
	}

	// now all the cubemaps are recreated, so we can starts to upload them
	if (world->loading_cubemaps)
	{
		int i;
		Mod_LoadCubemap(&world->defaultCubemap);

		for (i = 0; i < world->num_cubemaps; i++)
		{
			mcubemap_t *cm = &world->cubemaps[i];
			Vector vecStart, vecEnd;
			pmtrace_t pmtrace;

			Mod_LoadCubemap(cm);

			// compute viewbox size
			for (int j = 0; j < 6; j++)
			{
				vecStart = cm->origin;
				vecEnd = vecStart + env_dir[j] * 4096.0f;
				gEngfuncs.pEventAPI->EV_SetTraceHull(2);
				gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecStart + vecEnd, PM_WORLD_ONLY, -1, &pmtrace);
				AddPointToBounds(pmtrace.endpos, cm->mins, cm->maxs);
			}
		}

		// bind cubemaps onto world surfaces
		// so we don't need to search them again
		for (int i = 0; i < worldmodel->numsurfaces; i++)
		{
			msurface_t *surf = &worldmodel->surfaces[i];
			mextrasurf_t *es = surf->info;
			CL_FindTwoNearestCubeMapForSurface(es->origin, surf, &es->cubemap[0], &es->cubemap[1]);

			// compute lerp factor
			float dist0 = (es->cubemap[0]->origin - es->origin).Length();
			float dist1 = (es->cubemap[1]->origin - es->origin).Length();
			es->lerpFactor = dist0 / (dist0 + dist1);
		}

		world->loading_cubemaps = false;
	}
}

/*
=================
Mod_LoadCubemaps
=================
*/
void Mod_LoadCubemaps(const byte *base, const dlump_t *l)
{
	dcubemap_t *in;
	mcubemap_t *out;
	int count;

	in = (dcubemap_t *)(base + l->fileofs);
	if (l->filelen % sizeof(*in))
		HOST_ERROR("Mod_LoadCubemaps: funny lump size\n");
	count = l->filelen / sizeof(*in);

	if (count >= MAX_MAP_CUBEMAPS)
	{
		ALERT(at_error, "Mod_LoadCubemaps: map contain too many cubemaps. Will handle only first %i items\n", MAX_MAP_CUBEMAPS);
		count = MAX_MAP_CUBEMAPS;
	}

	out = world->cubemaps;
	world->num_cubemaps = count;
	world->loading_cubemaps = (count > 0) ? true : false;

	// makes an default cubemap from skybox
	if (FBitSet(world->features, WORLD_HAS_SKYBOX) && (count > 0))
	{
		mcubemap_t *cm = &world->defaultCubemap;
		Q_snprintf(cm->name, sizeof(cm->name), "maps/env/%s/default", world->name);
		cm->origin = (worldmodel->mins + worldmodel->maxs) * 0.5f;
		cm->size = 256; // default cubemap larger than others
		cm->valid = Mod_CheckCubemap("default"); // need for rebuild?
		world->loading_cubemaps = true;

		if (!cm->valid)
		{
			world->rebuilding_cubemaps = CMREBUILD_CHECKING;
			world->build_default_cubemap = true;
		}
	}
	else
	{
		// using stub as default cubemap
		mcubemap_t *cm = &world->defaultCubemap;
		Q_snprintf(cm->name, sizeof(cm->name), "*whiteCube");
		cm->origin = (worldmodel->mins + worldmodel->maxs) * 0.5f;
		cm->texture = tr.whiteCubeTexture;
		cm->valid = true;
		cm->size = 4;
	}

	for (int i = 0; i < count; i++, in++, out++)
	{
		// build a cubemap name like enum
		Q_snprintf(out->name, sizeof(out->name), "maps/env/%s/cube#%i", world->name, i);
		out->valid = Mod_CheckCubemap(va("cube#%i", i)); // need for rebuild?
		if (!out->valid) world->rebuilding_cubemaps = CMREBUILD_CHECKING;
		VectorCopy(in->origin, out->origin);
		ClearBounds(out->mins, out->maxs);
		out->size = in->size;

		if (out->size <= 0)
			out->size = DEFAULT_CUBEMAP_SIZE;
		out->size = NearestPOW(out->size, false);
		out->size = bound(1, out->size, 512);
	}

	// user request for disable autorebuild
	if (gEngfuncs.CheckParm("-noautorebuildcubemaps", NULL))
	{
		world->rebuilding_cubemaps = CMREBUILD_INACTIVE;
		world->build_default_cubemap = false;
	}
}
