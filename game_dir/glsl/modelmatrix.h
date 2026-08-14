/*
modelmatrix.h - per-submodel model matrix lookup
Copyright (C) 2026

The entity model matrices are stored in a RGBA32F float texture, one
submodel per column (4 texels, one per matrix column). The per-vertex
attr_MatrixIndex attribute selects the submodel.
*/

#ifndef MODELMATRIX_H
#define MODELMATRIX_H

uniform sampler2D	u_ModelMatrices;

mat4 GetModelMatrix( const float index )
{
	int i = int( index );

	return mat4(
		texelFetch( u_ModelMatrices, ivec2( i, 0 ), 0 ),
		texelFetch( u_ModelMatrices, ivec2( i, 1 ), 0 ),
		texelFetch( u_ModelMatrices, ivec2( i, 2 ), 0 ),
		texelFetch( u_ModelMatrices, ivec2( i, 3 ), 0 ));
}

#endif//MODELMATRIX_H
