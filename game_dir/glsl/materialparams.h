/*
materialparams.h - per-material params lookup
Copyright (C) 2026

Per-material scalar parameters are stored in a uniform buffer object
(std140), one vec4 per material, indexed by the per-vertex
attr_MaterialIndex attribute. MAX_MATERIALS is defined from the shader
options (per map).

Layout: x = detailScale.x, y = detailScale.y, z = reflectScale, w = refractScale
*/

#ifndef MATERIALPARAMS_H
#define MATERIALPARAMS_H

layout(std140) uniform MaterialParams
{
	vec4 u_MaterialParams[MAX_MATERIALS];
};

varying float	var_MaterialIndex;

vec4 GetMaterialParams( const float index )
{
	return u_MaterialParams[int( index )];
}

#endif//MATERIALPARAMS_H
