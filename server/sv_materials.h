/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#pragma once
#include "const.h"
#include "com_model.h"
#include "material.h"

typedef struct
{
	char		name[64];
	unsigned short	width, height;
	byte		maxHeight;
	byte		numLayers;	// layers that specified on heightmap
	byte		*pixels;		// pixels are immediately goes here
} sv_indexMap_t;

typedef struct
{
	char		names[MAX_LANDSCAPE_LAYERS][64];	// basenames
	matdef_t	*effects[MAX_LANDSCAPE_LAYERS];	// layer settings
} sv_layerMap_t;

typedef struct sv_terrain_s
{
	char		name[16];
	sv_indexMap_t	indexmap;
	sv_layerMap_t	layermap;
	int		numLayers;	// count of array textures
	bool		valid;		// if heightmap was actual
} sv_terrain_t;

void SV_InitMaterials();
void COM_InitMatdef();
sv_terrain_t *SV_FindTerrain(const char *texname);
void SV_ProcessWorldData(model_t *mod, qboolean create, const byte *buffer);
