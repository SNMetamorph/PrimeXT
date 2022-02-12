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

#include "gl_cubemap.h"
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
#include "gl_debug.h"
#include "gl_viewport.h"

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

	float maxDist1 = 999999.0f;
	float maxDist2 = 999999.0f;
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
CL_GetCubemapSideViewangles

returns viewangles which needed to render desired cubemap side
=================
*/
Vector CL_GetCubemapSideViewangles(int side)
{
	static Vector cubemapSideAngles[] =
	{
		Vector(0.0f,   0.0f,  90.0f),	// GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB 
		Vector(0.0f, 180.0f, -90.0f),	// GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB 
		Vector(0.0f,  90.0f,   0.0f),	// GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB
		Vector(0.0f, 270.0f, 180.0f),	// GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB 
		Vector(-90.0f, 180.0f, -90.0f),	// GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB
		Vector(90.0f,   0.0f,  90.0f),	// GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB
	};
	return cubemapSideAngles[side];
}

/*
==================
Mod_AllocateCubemap

allocate cubemap texture and FBO
==================
*/
static bool Mod_AllocateCubemap(mcubemap_t *cubemap)
{
	int flags = TF_CUBEMAP;
	if (!cubemap->texture)
	{
		if (GL_Support(R_SEAMLESS_CUBEMAP)) {
			SetBits(flags, TF_BORDER);	// seamless cubemaps have support for border
		}
		else {
			SetBits(flags, TF_CLAMP); // default method
		}

		if (CVAR_TO_BOOL(gl_hdr))
		{
			// set FP16 format for cubemaps (it's important for PBR and reflections)
			SetBits(flags, TF_ARB_16BIT);
			SetBits(flags, TF_ARB_FLOAT);
		}

		cubemap->texture = CREATE_TEXTURE(cubemap->name, cubemap->size, cubemap->size, NULL, flags);
		cubemap->numMips = RENDER_GET_PARM(PARM_TEX_MIPCOUNT, cubemap->texture); // NOTE: old DDS cubemaps has no mip-levels
	}

	cubemap->framebuffer.Init(FBO_CUBE, cubemap->size, cubemap->size, FBO_NOTEXTURE);
	bool textureTargetValid = RENDER_GET_PARM(PARM_TEX_TARGET, cubemap->texture) == GL_TEXTURE_CUBE_MAP_ARB;
	return textureTargetValid && cubemap->framebuffer.ValidateFBO();
}

/*
==================
Mod_FreeCubemap

unload a given cubemap
==================
*/
static void Mod_FreeCubemap(mcubemap_t *cubemap)
{
	if (cubemap->valid && cubemap->texture && cubemap->texture != tr.whiteCubeTexture) {
		FREE_TEXTURE(cubemap->texture);
	}

	cubemap->texture = 0;
	cubemap->valid = false;
	cubemap->numMips = 0;
	ClearBounds(cubemap->mins, cubemap->maxs);
	cubemap->framebuffer.Free();
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

	world->build_default_cubemap = false;
	world->loading_cubemaps = false;
	world->num_cubemaps = 0;
}

/*
==================
Mod_CheckCubemap

checks for cubemap sides existance and validates it
==================
*/
static bool Mod_CheckCubemap(const char *name)
{
	const char *suf[6] = { "px", "nx", "py", "ny", "pz", "nz" };
	int	valid_sides = 0;
	char sidename[64];
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
	char sidename[64];

	for (int i = 0; i < 6; i++)
	{
		Q_snprintf(sidename, sizeof(sidename), "maps/env/%s/%s%s.tga", world->name, name, suf[i]);

		if (FILE_EXISTS(sidename))
			Sys_RemoveFile(sidename);
	}
}

/*
=================
CL_BuildCubemaps_f

force to rebuilds all the cubemaps
in the scene
=================
*/
void CL_BuildCubemaps_f()
{
	Mod_FreeCubemap(&world->defaultCubemap);

	for (int i = 0; i < world->num_cubemaps; i++)
	{
		mcubemap_t *m = &world->cubemaps[i];
		Mod_FreeCubemap(m);
	}

	if (FBitSet(world->features, WORLD_HAS_SKYBOX)) {
		world->build_default_cubemap = true;
	}

	world->loading_cubemaps = true;
}

/*
==================
CL_LinkCubemapsWithSurfaces

assing cubemaps onto world surfaces
so we don't need to search them again
==================
*/
static void CL_LinkCubemapsWithSurfaces()
{
	for (int i = 0; i < worldmodel->numsurfaces; ++i)
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		mextrasurf_t *es = surf->info;
		CL_FindTwoNearestCubeMapForSurface(es->origin, surf, &es->cubemap[0], &es->cubemap[1]);

		// compute lerp factor
		float dist0 = (es->cubemap[0]->origin - es->origin).Length();
		float dist1 = (es->cubemap[1]->origin - es->origin).Length();
		es->lerpFactor = dist0 / (dist0 + dist1);
	}
}

static void GL_CreateCubemap(dcubemap_t *src, mcubemap_t *dest, int index)
{
	// build a cubemap name like enum
	Q_snprintf(dest->name, sizeof(dest->name), "cubemap_%i", index);
	VectorCopy(src->origin, dest->origin);
	ClearBounds(dest->mins, dest->maxs);

	dest->size = src->size;
	if (dest->size <= 0)
		dest->size = DEFAULT_CUBEMAP_SIZE;

	dest->texture = 0;
	dest->valid = false;
	dest->size = bound(1, dest->size, 512);
	dest->size = NearestPOW(dest->size, false);
}

static void GL_CreateSkyboxCubemap(mcubemap_t *cubemap)
{
	Q_snprintf(cubemap->name, sizeof(cubemap->name), "cubemap_%s", world->name);
	cubemap->origin = (worldmodel->mins + worldmodel->maxs) * 0.5f;
	cubemap->texture = 0;
	cubemap->valid = false;
	cubemap->size = 256; // default cubemap larger than others
}

static void GL_CreateStubCubemap(mcubemap_t *cubemap)
{
	Q_snprintf(cubemap->name, sizeof(cubemap->name), "*whiteCube");
	cubemap->origin = (worldmodel->mins + worldmodel->maxs) * 0.5f;
	cubemap->texture = tr.whiteCubeTexture;
	cubemap->valid = true;
	cubemap->size = 4;
}

static void GL_ComputeCubemapViewBoxSize(mcubemap_t *cubemap)
{
	pmtrace_t pmtrace;
	Vector vecStart, vecEnd;
	const float distance = 65536.f;
	static Vector env_dir[] =
	{
		Vector(1.0f,  0.0f,  0.0f),
		Vector(-1.0f,  0.0f,  0.0f),
		Vector(0.0f,  1.0f,  0.0f),
		Vector(0.0f, -1.0f,  0.0f),
		Vector(0.0f,  0.0f,  1.0f),
		Vector(0.0f,  0.0f, -1.0f)
	};

	for (int j = 0; j < 6; j++)
	{
		vecStart = cubemap->origin;
		vecEnd = vecStart + env_dir[j] * distance;
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecStart + vecEnd, PM_WORLD_ONLY, -1, &pmtrace);
		AddPointToBounds(pmtrace.endpos, cubemap->mins, cubemap->maxs);
	}
}

static void GL_SetupCubemapSideView(mcubemap_t *cubemap, ref_viewpass_t &rvp, int side)
{
	CViewport cubeSideViewport;
	cubeSideViewport.SetX(0);
	cubeSideViewport.SetY(0);
	cubeSideViewport.SetWidth(cubemap->size);
	cubeSideViewport.SetHeight(cubemap->size);
	cubeSideViewport.WriteToArray(rvp.viewport);

	rvp.vieworigin = cubemap->origin;
	rvp.viewangles = CL_GetCubemapSideViewangles(side);
	rvp.viewentity = 0;
	rvp.fov_x = rvp.fov_y = 90.0f;
	rvp.flags = RP_DRAW_WORLD;
	
	if (world->build_default_cubemap) {
		SetBits(rvp.flags, RP_SKYVIEW);
	}
	else {
		SetBits(rvp.flags, RP_ENVVIEW);
	}
}

static void GL_RenderCubemapSide(mcubemap_t *cubemap, int side)
{
	ref_viewpass_t rvp;
	GL_DebugGroupPush(__FUNCTION__);
	cubemap->framebuffer.Bind(cubemap->texture, side);
	GL_SetupCubemapSideView(cubemap, rvp, side);
	R_RenderScene(&rvp, static_cast<RefParams>(rvp.flags));
	GL_DebugGroupPop();
}

static void GL_RenderCubemap(mcubemap_t *cubemap)
{
	GL_DebugGroupPush(__FUNCTION__);
	if (Mod_AllocateCubemap(cubemap)) 
	{
		GL_ComputeCubemapViewBoxSize(cubemap);
		for (int i = 0; i < 6; ++i)
		{
			GL_RenderCubemapSide(cubemap, i);
			R_ResetRefState();
		}
		cubemap->valid = true;
	}
	else {
		ALERT(at_warning, "GL_RenderCubemap: failed to allocate cubemap \"%s\"\n", cubemap->name);
	}
	GL_DebugGroupPop();
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
	bool realtimeBaking = CVAR_TO_BOOL(r_cubemap_realtime);
	if (!world->loading_cubemaps && !realtimeBaking)
		return; // job is done

	if (RP_CUBEPASS())
		return; // already in cubemap-rendering mode

	int oldFBO = glState.frameBuffer;
	GL_DebugGroupPush(__FUNCTION__);
	R_PushRefState();
	
	if (world->build_default_cubemap)
	{
		GL_RenderCubemap(&world->defaultCubemap);
		world->build_default_cubemap = false; // done
	}

	for (int i = 0; i < world->num_cubemaps; i++)
	{
		mcubemap_t *cm = &world->cubemaps[i];
		if (!cm->valid) {
			GL_RenderCubemap(cm);
		}
	}

	if (realtimeBaking)
	{
		mcubemap_t *cubemapFirst, *cubemapSecond;
		CL_FindTwoNearestCubeMap(RI->view.origin, &cubemapFirst, &cubemapSecond);
		GL_RenderCubemap(cubemapFirst);
		if (cubemapFirst != cubemapSecond) {
			GL_RenderCubemap(cubemapSecond);
		}
	}

	// we reached the end of list
	// next frame will be restored gamma
	SetBits(cv_brightness->flags, FCVAR_CHANGED);
	SetBits(cv_gamma->flags, FCVAR_CHANGED);
	tr.params_changed = true;
	tr.glsl_valid_sequence++;
	tr.fClearScreen = false;
	world->loading_cubemaps = false;

	R_PopRefState();
	GL_BindFBO(oldFBO);
	GL_BindShader(NULL);
	GL_DebugGroupPop();
	CL_LinkCubemapsWithSurfaces();
}

/*
=================
Mod_LoadCubemaps
=================
*/
void Mod_LoadCubemaps(const byte *base, const dlump_t *l)
{
	int count;
	dcubemap_t *in = (dcubemap_t *)(base + l->fileofs);
	mcubemap_t *out = world->cubemaps;
	
	if (l->filelen % sizeof(*in))
		HOST_ERROR("Mod_LoadCubemaps: funny lump size\n");

	count = l->filelen / sizeof(*in);
	if (count >= MAX_MAP_CUBEMAPS)
	{
		ALERT(at_error, "Mod_LoadCubemaps: map contain too many cubemaps. Will handle only first %i items\n", MAX_MAP_CUBEMAPS);
		count = MAX_MAP_CUBEMAPS;
	}

	world->num_cubemaps = count;
	world->loading_cubemaps = (count > 0) ? true : false;

	// makes an default cubemap from skybox
	if (FBitSet(world->features, WORLD_HAS_SKYBOX) && world->loading_cubemaps)
	{
		GL_CreateSkyboxCubemap(&world->defaultCubemap);
		world->build_default_cubemap = true;
	}
	else
	{
		// using stub as default cubemap
		GL_CreateStubCubemap(&world->defaultCubemap);
	}

	for (int i = 0; i < count; i++, in++, out++) {
		GL_CreateCubemap(in, out, i);
	}

	// user request for disable autorebuild
	//if (gEngfuncs.CheckParm("-noautorebuildcubemaps", NULL))
	//{
	//	world->build_default_cubemap = false;
	//}
}
