/*
modelmatrix.h - per-submodel model matrix lookup
Copyright (C) 2026

The entity model matrices are stored in a uniform buffer object (std140),
one mat4 per submodel, indexed by the per-vertex attr_MatrixIndex attribute.
MAX_MODEL_MATRICES is defined from the shader options (per map).
*/

#ifndef MODELMATRIX_H
#define MODELMATRIX_H

layout(std140) uniform ModelMatrices
{
	mat4 u_ModelMatrices[MAX_MODEL_MATRICES];
};

mat4 GetModelMatrix( const float index )
{
	return u_ModelMatrices[int( index )];
}

#endif//MODELMATRIX_H
