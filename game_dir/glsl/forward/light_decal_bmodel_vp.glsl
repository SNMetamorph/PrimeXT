/*
light_decal_bmodel_vp.glsl - vertex uber shader for all dlight types for bmodel decals
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

#include "const.h"
#include "mathlib.h"
#include "matrix.h"
#include "tnbasis.h"
#include "modelmatrix.h"

attribute vec3	attr_Position;
attribute float	attr_MatrixIndex;
attribute vec4	attr_TexCoord0;

uniform mat4	u_LightViewProjMatrix;
uniform vec4	u_LightOrigin;

varying vec4	var_TexDiffuse;	// xy = decal coords, zw = surface coords
varying vec3	var_LightVec;

#if defined( HAS_NORMALMAP )
varying mat3	var_MatrixTBN;
#else
varying vec3	var_Normal;
#endif

#if defined( LIGHT_SPOT )
varying vec4	var_ProjCoord;
#endif

void main( void )
{
	mat4 modelMatrix = GetModelMatrix( attr_MatrixIndex );
	vec4 position = vec4( attr_Position, 1.0 );
	vec4 worldpos = modelMatrix * position;

	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	mat3 tbn = ComputeTBN( modelMatrix );

	// decal & surface scissor coords
	var_TexDiffuse = attr_TexCoord0;

#if defined( LIGHT_SPOT )
	var_ProjCoord = ( Mat4Texture( -0.5 ) * u_LightViewProjMatrix ) * worldpos;
#endif

	// these things are in world space and not normalized
	var_LightVec = ( u_LightOrigin.xyz - worldpos.xyz );

#if defined( HAS_NORMALMAP )
	var_MatrixTBN = tbn;
#else
	var_Normal = tbn[2];
#endif
}
