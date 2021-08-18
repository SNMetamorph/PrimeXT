/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "polylib.h"
#include "threads.h"
#include "stringlib.h"
#include "filesystem.h"

#define DEFAULT_TEXSCALE		true
#define DEFAULT_DLIGHT_SCALE		1.0
#define DEFAULT_WADTEXTURES		false
#define DEFAULT_TEXREFLECTGAMMA	(1.0 / 2.2)	// turn back to linear space
#define DEFAULT_TEXREFLECTSCALE	1.0
#define DEFAULT_LIGHTCLIP		255
#define DEFAULT_GAMMA		1.8

#define MAX_SINGLEMAP		((MAX_CUSTOM_SURFACE_EXTENT+1) * (MAX_CUSTOM_SURFACE_EXTENT+1) * 3)
#define MAX_SUBDIVIDE		16384
#define MAX_TEXLIGHTS		1024

typedef struct
{
	char		name[256];
	const char	*filename;
	vec3_t		value;
} texlight_t;

typedef struct 
{
	vec3_t		facenormal;	// face normal
	short		texmins[2];	// also used for face testing
	short		texsize[2];
} faceinfo_t;

typedef struct
{
	vec3_t		light[MAXLIGHTMAPS];	// total lightvalue pex luxel
} sample_t;

typedef struct
{
	int		numsamples;
	sample_t		*samples;
} facelight_t;

//
// textures.c
//
void TEX_LoadTextures( void );
miptex_t *GetTextureByMiptex( int miptex );
void TEX_FreeTextures( void );