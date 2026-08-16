/*
decalmaterialparams.h - per-decal material params lookup
Copyright (C) 2026

Decal material params are stored in a uniform buffer object (std140),
two vec4 per material, indexed by the per-vertex attr_MaterialIndex
attribute. MAX_MATERIALS is defined from the shader options.

Layout:
  vec4[0] = (detailScale.x, detailScale.y, reflectScale, refractScale)
  vec4[1] = (smoothness, aberrationScale, reliefScale, 0)
*/

#ifndef DECALMATERIALPARAMS_H
#define DECALMATERIALPARAMS_H

layout(std140) uniform DecalMaterialParams
{
	vec4 u_DecalMaterialParams[MAX_MATERIALS * 2];
};

vec4 GetDecalMaterialParams( const float index )
{
	return u_DecalMaterialParams[int( index ) * 2 + 0];
}

vec4 GetDecalMaterialParams2( const float index )
{
	return u_DecalMaterialParams[int( index ) * 2 + 1];
}

#endif//DECALMATERIALPARAMS_H
