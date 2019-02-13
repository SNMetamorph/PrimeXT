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

#define INVALID_HANDLE	0xFFFF

#define Z_NEAR		4.0f
#define Z_NEAR_LIGHT	0.1f

#define BLOCK_SIZE		512	// lightmap blocksize
#define NORMAL_FLATSHADE	0.7f	// same as Valve 'flatshade'
#define BACKFACE_EPSILON	0.01f
#define SHADE_LAMBERT	1.495f
#define DEFAULT_ALPHATEST	0.0f	// alpha ref default value
#define MAX_REF_STACK	8	// renderpass stack depth
#define MAX_VISIBLE_PACKET	1024	// 10 bits
#define MAX_MODELS		1024	// engine limit
#define MAX_LIGHTSTYLES	64	// original quake limit
#define MAX_LIGHTMAPS	256	// 8 bits
#define MAX_CLIP_VERTS	64	// skybox clipping vertexes
#define SUBDIVIDE_SIZE	64
#define MAX_LANDSCAPE_LAYERS	16	// fundamental limit, don't modify
#define MAX_SUBVIEWS	256	// per one frame!
#define MAX_SUBVIEW_TEXTURES	64	// total depth
#define MAX_DLIGHTS		32	// engine limit
#define MAX_ELIGHTS		64	// engine limit (entity only point lights)
#define MAX_MOVIES		16	// max various movies per level
#define MAX_MOVIE_TEXTURES	64	// max # of unique video textures per level
#define MAX_PLIGHTS		96	// 32 for players, 32 for other ents and 32 for replace cl_dlights
#define MAX_USER_PLIGHTS	64	// after 64 comes dlights
#define MAX_SHADOWS		64	// BUGBUG: not included pointlights only directional lights
#define MAX_CACHED_STATES	512	// 32 kb cache (this needs only for bmodels)
#define MAX_LIGHTCACHE	2048	// unique models with instanced vertex lighting
#define MAX_DECAL_SURFS	4096
#define MAX_FRAMEBUFFERS	MAX_SUBVIEW_TEXTURES
#define TURBSCALE		( 256.0f / M_PI2 )
#define REFPVS_RADIUS	2.0f
#define MOD_FRAMES		20
#define STAIR_INTERP_TIME	100.0f
#define GLOWSHELL_FACTOR	(1.0f / 128.0f)
#define WORLD_MATRIX	0	// must be 0 always
#define FBO_MAIN		-1
	
#define CHECKVISBIT( vis, b )		((b) >= 0 ? (byte)((vis)[(b) >> 3] & (1 << ((b) & 7))) : (byte)false )
#define SETVISBIT( vis, b )( void )	((b) >= 0 ? (byte)((vis)[(b) >> 3] |= (1 << ((b) & 7))) : (byte)false )
#define CLEARVISBIT( vis, b )( void )	((b) >= 0 ? (byte)((vis)[(b) >> 3] &= ~(1 << ((b) & 7))) : (byte)false )

#endif//R_CONST_H
