/*
materialparams.h - per-material params lookup
Copyright (C) 2026

Per-material scalar parameters are stored in a uniform buffer object
(std140), two vec4 per material, indexed by the per-vertex
attr_MaterialIndex attribute. MAX_MATERIALS is defined from the shader
options (per map).

Layout:
  vec4[0] = (detailScale.x, detailScale.y, reflectScale, refractScale)
  vec4[1] = (smoothness, aberrationScale, reliefScale, 0)
*/

#ifndef MATERIALPARAMS_H
#define MATERIALPARAMS_H

layout(std140) uniform MaterialParams
{
	vec4 u_MaterialParams[MAX_MATERIALS * 2];
};

varying float	var_MaterialIndex;

vec4 GetMaterialParams( const float index )
{
	return u_MaterialParams[int( index ) * 2 + 0];
}

vec4 GetMaterialParams2( const float index )
{
	return u_MaterialParams[int( index ) * 2 + 1];
}

#endif//MATERIALPARAMS_H
