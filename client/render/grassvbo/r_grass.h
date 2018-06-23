/*
r_garss.h - grass rendering
Copyright (C) 2013 SovietCoder

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef R_GRASS_H
#define R_GRASS_H

#define GRASS_TEXTURES	256			// unique textures for grass (a byte limit, don't change)
#define GRASS_ANIM_DIST	512.0f			// if this is changed it must be changed in glsl too!
#define MAX_GRASS_ELEMS	65536			// unsigned short limit
#define MAX_GRASS_VERTS	( MAX_GRASS_ELEMS / 2 )	// ( numelems / 1.5 ) actually
#define MAX_GRASS_BUSHES	( MAX_GRASS_VERTS / 16 )	// one bush contain 4 poly, so we have 2048 bushes max per one surface
#define GRASS_PACKED_VERTEX

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
	int	gl_texturenum;	// gl-texture
} grasstexture_t;

// 52 bytes here
#pragma pack(1)
typedef struct gvert_s
{
#ifdef GRASS_PACKED_VERTEX
	short	center[4];		// used for rescale
	short	normal[4];		// center + vertex[2] * vertex[3];
#else
	Vector4D	center;			// used for rescale
	Vector4D	normal;			// center + vertex[2] * vertex[3];
#endif
	float	light[MAXLIGHTMAPS];	// packed color + unused entry
	byte	styles[MAXLIGHTMAPS];	// styles on surface
} gvert_t;
#pragma pack()

#define FGRASS_NODRAW	BIT( 0 )		// grass shader is failed to build
#define FGRASS_NODLIGHT	BIT( 1 )		// grass dlight shader is failed to build

// all the grassdata for one polygon and specified texture
// stored into single vbo
typedef struct grass_s
{
	byte		texture;		// not a real texture just index into array
	word		shaderNum;	// may be changed while grass link into chain
	word		hLightShader;	// temp light shader
	word		hSunLightShader;	// sun light shader
	word		flags;		// state flags
	unsigned short	numVerts;		// for glDrawRangeElementsEXT
	unsigned short	numElems;		// for glDrawElements
	unsigned int	vbo, vao, ibo;	// buffer objects
	struct grass_s	*chain[MAX_REF_STACK];// for sequentially drawing
	struct grass_s	*lightchain;	// for sequentially drawing light
	unsigned short	hCachedMatrix;	// HACKHACK: get matrices
	int		glsl_sequence;	// for catch global changes
} grass_t;

typedef struct grasshdr_s
{
	Vector		mins, maxs;	// per-poly culling
	float		animtime;
	float		radius;	// for cull on rotatable bmodels
	float		scale;
	int		count;	// total bush count for this poly
	float		dist;	// dist to player
	grass_t		g[1];	// variable sized
} grasshdr_t;

extern void R_GrassInit( void );
extern void R_GrassShutdown( void );
extern void R_GrassInitForSurface( msurface_t *surf );
extern void R_RenderGrassOnList( void );
extern void R_GrassPrepareFrame( void );
extern void R_GrassPrepareLight( void );
extern void R_RenderShadowGrassOnList( void );
extern void R_DrawLightForGrass( struct plight_s *pl );
extern bool R_AddGrassToChain( struct mextraleaf_s *leaf, msurface_t *s, cl_entity_t *e, CFrustum *frustum, int clipFlags, bool lightpass = false );
extern void R_RemoveGrassForSurface( mextrasurf_t *es );
extern void R_UnloadFarGrass( void );

#endif//R_GRASS_H
