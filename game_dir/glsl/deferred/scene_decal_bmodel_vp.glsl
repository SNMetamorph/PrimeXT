/*
scene_decal_bmodel_vp.glsl - vertex uber shader for bmodel decals
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

attribute vec3	attr_Position;
attribute vec4	attr_TexCoord0;	// diffuse\decal
attribute vec4	attr_LightStyles;

uniform vec3	u_ViewOrigin;	// already in modelspace
uniform mat4	u_ModelMatrix;

varying vec4	var_TexDiffuse;
varying mat3	var_WorldMat;
varying vec3	var_ViewDir;
varying vec3	var_Position;

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );
	vec4 worldpos = u_ModelMatrix * position;

	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	var_WorldMat = ComputeTBN( u_ModelMatrix );

	// decal & surface scissor coords
	var_TexDiffuse = attr_TexCoord0;

	vec3 V = ( u_ViewOrigin - worldpos.xyz );
	// transform viewdir into tangent space
	var_ViewDir = V * var_WorldMat;
	var_Position = worldpos.xyz;
}