/*
material.h - texture materials code
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef MATERIAL_H
#define MATERIAL_H

#include "texfetch.h"

struct MaterialData
{
	float smoothness;
	float metalness;
	float ambientOcclusion;
};

MaterialData MaterialFetchTexture(vec4 texel)
{
	MaterialData result;
	result.smoothness = texel.r;
	result.metalness = texel.g;
	result.ambientOcclusion = texel.b;
	return result;
}

#endif // MATERIAL_H
