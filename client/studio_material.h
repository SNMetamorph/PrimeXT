/*
studio_material.h - studio material declaration for internal render usage
Copyright (C) 2023 SNMetamorph

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
#include "studio.h"
#include "material.h"
#include "shader.h"
#include "texture_handle.h"

// each mstudiotexture_t has a material
typedef struct mstudiomat_s
{
	mstudiotexture_t	*pSource;			// pointer to original texture

	TextureHandle	gl_diffuse_id;		// diffuse texture
	TextureHandle	gl_detailmap_id;	// detail texture
	TextureHandle	gl_normalmap_id;	// normalmap
	TextureHandle	gl_specular_id;		// specular
	TextureHandle	gl_glowmap_id;		// self-illuminate parts
	TextureHandle	gl_heightmap_id;	// parallax stuff

	// this part is shared with matdesc_t
	float		smoothness;		// smoothness factor
	float		detailScale[2];		// detail texture scales x, y
	float		reflectScale;		// reflection scale for translucent water
	float		refractScale;		// refraction scale for mirrors, windows, water
	float		aberrationScale;		// chromatic abberation
	float		reliefScale;		// relief-mapping
	float       swayHeight;    // height from model origin for swaying, 0 - disabled
	struct matdef_t	*effects;			// hit, impact, particle effects etc
	int		flags;			// mstudiotexture_t->flags

	// cached shadernums
	shader_t		forwardScene;
	shader_t		forwardLightSpot[2];
	shader_t		forwardLightOmni[2];
	shader_t		forwardLightProj;
	shader_t		forwardDepth;

	unsigned short	lastRenderMode;		// for catch change render modes
} mstudiomaterial_t;
