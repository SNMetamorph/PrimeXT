/*
material.h - material description for both client and server
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#ifndef MATERIAL_H
#define MATERIAL_H

#include "com_model.h"
#include "vector.h"

#define MAX_MAT_SOUNDS		8
#define IMPACT_NONE			0
#define IMPACT_BODY			1
#define IMPACT_MATERIAL		2
#define DEFAULT_SMOOTHNESS	0.0f

#define COM_CopyString( s )	_COM_CopyString( s, __FILE__, __LINE__ )

struct matdef_t
{
	char		name[32];	// material name
	const char	*impact_decal;
	const char	*impact_parts[MAX_MAT_SOUNDS+1];	// for terminator
	const char	*impact_sounds[MAX_MAT_SOUNDS+1];
	const char	*step_sounds[MAX_MAT_SOUNDS+1];
};

// NOTE: if this is changed it must be changed in studio.h and com_model.h too!!!
struct matdesc_t
{
	float		smoothness;			// smoothness factor
	vec2_t		detailScale;		// detail texture scales x, y
	float		reflectScale;		// reflection scale for translucent water
	float		refractScale;		// refraction scale for mirrors, windows, water
	float		aberrationScale;	// chromatic abberation
	float		reliefScale;		// relief-mapping
	float       swayHeight;    // height from model origin for swaying, 0 - disabled
	matdef_t	*effects;			// hit, impact, particle effects etc
	char		name[64];			// just a name of material
	uint32_t	hash;

	char		diffusemap[64];
	char		normalmap[64];
	char		glossmap[64];
	char		detailmap[64];
};

// matdef is physical properties description
matdef_t *COM_MatDefFromSurface(msurface_t *surf, const Vector &pointOnSurf);
matdef_t *COM_FindMatdef(const char *name);
matdef_t *COM_DefaultMatdef();
void COM_PrecacheMaterialsSounds();
void COM_InitMatdef();

// matdesc is visual properties description
matdesc_t *COM_DefaultMatdesc();
void COM_LoadMaterials(const char *path);
matdesc_t *COM_FindMaterial(const char *texName);
matdesc_t *COM_FindMaterial(uint32_t matHash);
void COM_InitMaterials(matdesc_t *&matlist, int &matcount);
uint32_t COM_GetMaterialHash(matdesc_t *mat);

#endif//MATERIAL_H
