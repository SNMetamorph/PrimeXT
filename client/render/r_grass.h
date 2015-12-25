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
#define GRASS_DENSITY( size, density )	( sqrtf( size.x * size.x + size.y * size.y ) / 64.0f * density )

#define ParceGrassFirstPass()\
pfile = COM_ParseFile( pfile, token );\
if( !pfile )\
{\
	ALERT( at_error, "R_ParseGrass: unexpected end of the file %s\n", file );\
	break;\
}\

typedef enum
{
	GRASS_PASS_NORMAL = 0,
	GRASS_PASS_SHADOW,
	GRASS_PASS_AMBIENT,
	GRASS_PASS_LIGHT,
	GRASS_PASS_DIFFUSE,
	GRASS_PASS_FOG,
} GrassPassMode;

// 20 bytes here
typedef struct grassvert_s
{
	float	v[3];
	byte	c[4];
	short	t[2];
} grassvert_t;

typedef struct grass_s
{
	Vector		pos;
	float		size;
	byte		color[3];
	byte		texture;	// not a real texture just index into array
	grassvert_t	*mesh;
} grass_t;

typedef struct grasshdr_s
{
	int		renderframe, cullframe;
	int		cached_light[MAXLIGHTMAPS];
	Vector		mins, maxs;
	float		animtime;
	float		radius;	// for cull on rotatable bmodels
	float		scale;
	int		count;	// total bush count for this poly
	float		dist;	// dist to player

	struct grasshdr_s	*chain[3];
	grass_t		g[1];	// variable sized
} grasshdr_t;

typedef struct grasstex_s
{
	char		name[256];
	int		gl_texturenum;
} grasstex_t;

typedef struct grasspinfo_s
{
	char		stex[16];
	char		gtex[256];
	byte		texture;
	float		density;
	float		min;
	float		max;
	int		seed;
} grasspinfo_t;

extern void R_DrawGrass( int pass );
extern void R_ParseGrassFile( void );
extern void R_ReLightGrass( msurface_t *surf, bool force = false );
extern void R_AddToGrassChain( msurface_t *surf, const mplane_t frustum[6], unsigned int clipflags, qboolean lightpass );

#endif//R_GRASS_H
