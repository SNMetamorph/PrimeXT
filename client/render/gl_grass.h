/*
gl_grass.h - grass construct & rendering
this code written for Paranoia 2: Savior modification
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef GL_GRASS_H
#define GL_GRASS_H
#include "texture_handle.h"

#define GRASS_TEXTURES	256			// unique textures for grass (a byte limit, don't change)
#define GRASS_ANIM_DIST	512.0f			// if this is changed it must be changed in glsl too!
#define MAX_GRASS_ELEMS	65536			// unsigned short limit
#define MAX_GRASS_VERTS	( MAX_GRASS_ELEMS / 2 )	// ( numelems / 1.5 ) actually
#define MAX_GRASS_BUSHES	( MAX_GRASS_VERTS / 16 )	// one bush contain 4 poly, so we have 2048 bushes max per one surface
#define GRASS_SKY_DIST	BOGUS_RANGE		// in-world grass never reach this value

typedef struct grassentry_s
{
	char	name[16];		// name of level texture
	byte	texture;		// number in array of grass textures
	float	density;		// grass density (0 - 100)
	float	min;		// min grass scale
	float	max;		// max grass scale
	int	seed;		// seed for predictable random (auto-filled)
} grassentry_t;

typedef struct grasstexture_s
{
	char	name[256];	// path to grass texture
	TextureHandle	gl_texturenum;	// gl-texture
} grasstexture_t;

typedef struct gvert_s
{
	float		center[4];		// used for rescale
	float		normal[4];		// center + vertex[2] * vertex[3];
	float		light[MAXLIGHTMAPS];	// packed color + unused entry
	float		delux[MAXLIGHTMAPS];	// packed lightdir + unused entry
	byte		styles[MAXLIGHTMAPS];	// styles on surface
} gvert_t;

#define FGRASS_NODRAW	BIT( 0 )		// grass shader is failed to build
#define FGRASS_NODLIGHT	BIT( 1 )		// grass dlight shader is failed to build
#define FGRASS_NOSUNLIGHT	BIT( 2 )		// grass dlight shader is failed to build

// all the grassdata for one polygon and specified texture
// stored into single vbo
typedef struct grass_s
{
	// shader cache
	shader_t		forwardScene;
	shader_t		forwardLightSpot;
	shader_t		forwardLightOmni;
	shader_t		forwardLightProj;
	shader_t		forwardDepth;

	byte		texture;		// not a real texture just index into array
	byte		flags;		// state flags
	unsigned short	numVerts;		// for glDrawRangeElementsEXT
	unsigned short	numElems;		// for glDrawElements
	unsigned int	vbo, vao, ibo;	// buffer objects
	unsigned short	hCachedMatrix;	// HACKHACK: get matrices
	byte		lights[MAXDYNLIGHTS];/// light numbers
	unsigned int	cacheSize;	// debug info: uploaded cache size for this buffer
} grass_t;

typedef void (*pfnCreateGrassBuffer)( grass_t *pOut, gvert_t *arrayxvert );
typedef void (*pfnBindGrassBuffer)( grass_t *pOut, int attrFlags );

enum
{
	GRASSLOADER_BASE = 0,
	GRASSLOADER_BASEBUMP,
	GRASSLOADER_COUNT,
};

typedef struct
{
	pfnCreateGrassBuffer	CreateBuffer;
	pfnBindGrassBuffer		BindBuffer;
	const char*		BufferName;	// debug
} grass_loader_t;

typedef struct grasshdr_s
{
	Vector		mins, maxs;	// per-poly culling
	int		count;		// total bush count for this poly
	grass_t		g[1];		// variable sized
} grasshdr_t;

extern void R_GrassInit( void );
extern void R_GrassShutdown( void );
extern void R_GrassInitForSurface( msurface_t *surf );
extern void R_RenderGrassOnList( void );
extern void R_GrassSetupFrame( void );
extern void R_RenderShadowGrassOnList( void );
extern void R_DrawLightForGrass( CDynLight *pl );
extern void R_AddGrassToDrawList( msurface_t *s, drawlist_t type );
extern void R_PrecacheGrass( msurface_t *s, mextraleaf_t *leaf );
extern void R_RemoveGrassForSurface( mextrasurf_t *es );
extern void R_UnloadFarGrass( void );

#endif//GL_GRASS_H