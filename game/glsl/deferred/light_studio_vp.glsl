/*
light_studio_vp.glsl - g-buffer rendering studio triangles
Copyright (C) 2018 Uncle Mike

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
#include "matrix.h"
#include "mathlib.h"
#include "lightmodel.h"
#include "skinning.h"
#include "tnbasis.h"

attribute vec3		attr_Position;
attribute vec2		attr_TexCoord0;

varying vec2		var_TexDiffuse;
varying vec3		var_Normal;

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );
	mat4 boneMatrix = ComputeSkinningMatrix();
	vec4 worldpos = boneMatrix * position;

	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	mat3 tbn = ComputeTBN( boneMatrix );

	var_TexDiffuse = attr_TexCoord0;
	var_Normal = tbn[2];
}