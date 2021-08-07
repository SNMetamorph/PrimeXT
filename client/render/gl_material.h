/*
gl_material.h - visible material settings
this code written for Paranoia 2: Savior modification
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef GL_MATERIAL_H
#define GL_MATERIAL_H

typedef enum
{
	PHYSMODEL_BRDF = 0,		// BRDF as default
	PHYSMODEL_DOOM3,		// obsolete lighting model like Doom3
	PHYSMODEL_FOLIAGE,		// two-side rendering, vegetation lighting model
	PHYSMODEL_GLASS,		// translucent surface
	PHYSMODEL_SKIN,		// human skin lighting model
	PHYSMODEL_HAIR,		// anisotropic lighting model for hairs
	PHYSMODEL_EYES,		// eyes physical model
} physmodel_t;

typedef struct
{
	physmodel_t	physModel;
	struct matdef_s	*effects;	// material common effects (decals, particles, etc)
} gl_material_t;

#endif//GL_MATERIAL_H