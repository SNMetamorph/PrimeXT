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

#define GRASS_VERTS			65536
#define GRASS_TEXTURES		256	// unique textures for grass
#define GRASS_ANIM_TIME		0.05f
#define GRASS_ANIM_DIST		512.0f
#define GRASS_SCALE_STEP		16.0f

// 20 bytes here
typedef struct grassvert_s
{
	float		v[3];
	byte		c[4];
	short		t[2];
} grassvert_t;

// all the grassdata for one polygon and specified texture
// stored into single vbo
typedef struct grass_s
{
	Vector		pos;
	float		size;
	byte		color[4];
	byte		texture;	// not a real texture just index into array
	grassvert_t	*mesh;
	struct grass_s	*chain;
} grass_t;

typedef struct grasshdr_s
{
	int		cached_light[MAXLIGHTMAPS];
	Vector		mins, maxs;
	float		animtime;
	float		radius;	// for cull on rotatable bmodels
	float		scale;
	int		count;	// total bush count for this poly
	float		dist;	// dist to player
	grass_t		g[1];	// variable sized
} grasshdr_t;

typedef struct grasstex_s
{
	char		name[256];
	int		gl_texturenum;
	grass_t		*grasschain;	// for sequentially draw
} grasstex_t;

typedef struct grassentry_s
{
	char		name[16];		// name of level texture
	byte		texture;		// number in array of grass textures
	float		density;		// grass density (0 - 100)
	float		min;		// min grass scale
	float		max;		// max grass scale
	int		seed;		// seed for predictable random (auto-filled)
} grassentry_t;

extern void R_GrassInit( void );
extern void R_GrassShutdown( void );
extern void R_GrassInitForSurface( msurface_t *surf );
extern void R_DrawGrass( qboolean lightpass = false );
extern void R_ReLightGrass( msurface_t *surf, bool force = false );
extern bool R_AddGrassToChain( msurface_t *s, CFrustum *frustum, bool lightpass = false, struct mworldleaf_s *leaf = NULL );
extern void R_DrawGrassLight( struct plight_s *pl );
extern void R_UnloadFarGrass( void );

#endif//R_GRASS_H
