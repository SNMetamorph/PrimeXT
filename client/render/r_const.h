/*
r_const.h - renderer constants (shared with engine)
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

#ifndef R_CONST_H
#define R_CONST_H

#define BLOCK_SIZE		glConfig.block_size	// lightmap blocksize
#define BLOCK_SIZE_DEFAULT	128		// for keep backward compatibility
#define BLOCK_SIZE_MAX	1024

#define MAX_SIDE_VERTS	512	// per one polygon (engine constant, do not increase)
#define MAX_VISIBLE_PACKET	1024	// 10 bits
#define MAX_TEXTURES	4096	// engine limit
#define MAX_MODELS		2048	// engine limit
#define MAX_LIGHTSTYLES	256	// a byte limit, don't modify
#define MAX_LIGHTMAPS	256	// 8 bits
#define MAX_CLIP_VERTS	64	// skybox clipping vertexes
#define SUBDIVIDE_SIZE	64
#define MAX_MIRRORS		32	// per one frame!
#define MAX_DLIGHTS		32	// engine limit
#define MAX_ELIGHTS		64	// engine limit (entity only point lights)
#define MAX_MOVIES		16	// max various movies per level
#define MAX_MOVIE_TEXTURES	64	// max # of unique video textures per level
#define MAX_PLIGHTS		96	// 32 for players, 32 for other ents and 32 for replace cl_dlights
#define MAX_USER_PLIGHTS	64	// after 64 comes dlights
#define MAX_SHADOWS		64	// BUGBUG: not included pointlights only directional lights
#define MAX_FRAMEBUFFERS	64
#define MAXARRAYVERTS	8192
#define TURBSCALE		( 256.0f / M_PI2 )
#define AMBIENT_EPSILON	0.001f

#define CALC_TEXCOORD_S( v, tex )	(( DotProductFast(( v ), (tex)->vecs[0] ) + (tex)->vecs[0][3] ) / (float)(tex)->texture->width )
#define CALC_TEXCOORD_T( v, tex )	(( DotProductFast(( v ), (tex)->vecs[1] ) + (tex)->vecs[1][3] ) / (float)(tex)->texture->height )

enum
{
	FBO_MAIN = -1,		// main buffer
	FBO_MIRRORS,		// used for mirrors
	FBO_SCREENS,		// used for screens
	FBO_PORTALS,		// used for portals
	FBO_NUM_TYPES,
};

#endif//R_CONST_H
