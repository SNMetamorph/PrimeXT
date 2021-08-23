/*
gl_world.h - local world data for rendering
this code written for Paranoia 2: Savior modification
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

#ifndef GL_WORLD_H
#define GL_WORLD_H

#include "cubemap.h"
#include "vertex_fmt.h"
#include "matrix.h"
#include "gl_export.h"
#include "gl_local.h"

// world features
#define WORLD_HAS_MOVIES	BIT( 0 )

#define WORLD_HAS_GRASS	BIT( 2 )
#define WORLD_HAS_DELUXEMAP	BIT( 3 )
#define WORLD_HAS_SKYBOX	BIT( 4 )
#define WORLD_WATERALPHA	BIT( 5 )

#define MAX_MAP_ELEMS	MAX_MAP_VERTS * 5	// should be enough
#define SHADOW_ZBUF_RES	8		// 6 * 8 * 8 * 2 * 4 = 3k bytes per light

// rebuilding cubemap states
#define CMREBUILD_INACTIVE	0
#define CMREBUILD_CHECKING	1
#define CMREBUILD_WAITING	2

typedef struct bvert_s
{
	Vector		vertex;			// position
	Vector		tangent;			// tangent
	Vector		binormal;			// binormal
	Vector		normal;			// normal
	float		stcoord0[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords for styles 0-1
	float		lmcoord1[4];		// LM texture coords for styles 2-3
	byte		styles[MAXLIGHTMAPS];	// light styles
	byte		lights0[4];		// packed light numbers
	byte		lights1[4];		// packed light numbers
} bvert_t;

typedef struct
{
	dlightcube_t	cube;
	vec3_t		origin;
	mleaf_t		*leaf;		// ambient linked into this leaf
} mlightprobe_t;

typedef struct mworldlight_s
{
	emittype_t	emittype;
	int		style;
	byte		*pvs;		// accumulated domain of the light
	vec3_t		origin;		// light abs origin
	vec3_t		intensity;	// RGB
	vec3_t		normal;		// for surfaces and spotlights
	float		stopdot;		// for spotlights
	float		stopdot2;		// for spotlights
	float		fade;		// falloff scaling for linear and inverse square falloff 1.0 = normal, 0.5 = farther, 2.0 = shorter
	float		radius;		// light radius
	mleaf_t		*leaf;		// light linked into this leaf
	byte		falloff;		// falloff style 0 = default (inverse square), 1 = inverse falloff, 2 = inverse square
	msurface_t	*surface;		// surf pointer for emit_surface
	int		lightnum;		// important stuff!
	int		modelnum;		// unused
	unsigned short	shadow_x;		// offset by X in atlas
	unsigned short	shadow_y;		// offset by Y in atlas
	unsigned short	shadow_w;		// 0 is uninitialized
	unsigned short	shadow_h;		// 0 is uninitialized
} mworldlight_t;

// leaf extradata
typedef struct mextraleaf_s
{
// leaf specific
	vec3_t		mins, maxs;	// leaf size that updated with grass bounds

	mlightprobe_t	*ambient_light;
	byte		num_lightprobes;

	mworldlight_t	*direct_lights;
	int		num_directlights;
} mextraleaf_t;

struct BmodelInstance_t
{
	cl_entity_t	*m_pEntity;

	// bounds info
	Vector		bbox[8];
	Vector		absmin;
	Vector		absmax;
	float		radius;
	byte		lights[MAXDYNLIGHTS];

	matrix4x4		m_transform;
	GLfloat		m_gltransform;
};

struct LightShadowZBufferSample_t
{
	float		m_flTraceDistance;	// how far we traced. 0 = invalid
	float		m_flHitDistance;	// where we hit
};

typedef CCubeMap< LightShadowZBufferSample_t, SHADOW_ZBUF_RES> lightzbuffer_t;
	
typedef struct
{
	char		name[64];		// to avoid reloading on same

	word		features;		// world features

	int		num_visible_models;	// visible models

	mextraleaf_t	*leafs;		// [worldmodel->numleafs]
	int		totalleafs;	// full leaf counting
	int		numleafs;		// [submodels[0].visleafs + 1]

	material_t	*materials;	// [worldmodel->numtextures]

	int		numleaflights;
	mlightprobe_t	*leaflights;

	int		numworldlights;
	mworldlight_t	*worldlights;

	dvertnorm_t	*surfnormals;	// is not NULL here a indexed normals
	dnormal_t		*normals;
	int		numnormals;

	mcubemap_t	cubemaps[MAX_MAP_CUBEMAPS];
	mcubemap_t	defaultCubemap;
	int		num_cubemaps;

	terrain_t		*terrains;
	unsigned int	num_terrains;

	bvert_t		*vertexes;
	int		numvertexes;

	dvlightlump_t	*vertex_lighting;	// used for env_statics
	dvlightlump_t	*surface_lighting;	// used for env_statics

	byte		*vislightdata;
	color24		*deluxedata;
	byte		*shadowdata;

	lightzbuffer_t	*shadowzbuffers;

	// single buffer for all the models
	uint		vertex_buffer_object;
	uint		vertex_array_object;
	uint		cacheSize;

	unsigned short	*sortedfaces;	// surfaces sorted through all models
	unsigned short	numsortedfaces;

	// misc info
	Vector2D		orthocenter;	// overview stuff
	Vector2D		orthohalf;

	// cubemap builder internal state
	bool	loading_cubemaps;
	bool	build_default_cubemap;
	int		rebuilding_cubemaps;
	int		cubemap_build_number;
	bool	ignore_restart_check; // to prevent bug with invalid restart check in GL_InitModelLightCache
} gl_world_t;

extern gl_world_t	*world;

#endif//GL_WORLD_H