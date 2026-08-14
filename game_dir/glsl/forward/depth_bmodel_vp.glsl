/*
depth_bmodel_vp.glsl - bmodel shadow pass
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

#include "modelmatrix.h"

attribute vec3		attr_Position;
attribute float		attr_MatrixIndex;
attribute vec2		attr_TexCoord0;

uniform vec2		u_TexOffset;

varying vec2		var_TexCoord;	// for alpha-testing

void main( void )
{
	mat4 modelMatrix = GetModelMatrix( attr_MatrixIndex );
	vec4 position = vec4( attr_Position, 1.0 );
	vec4 worldpos = modelMatrix * position;

	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	var_TexCoord = attr_TexCoord0 + u_TexOffset;
}