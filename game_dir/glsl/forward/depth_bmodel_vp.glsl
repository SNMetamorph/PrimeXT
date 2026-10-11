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

attribute vec3		attr_Position;
attribute vec2		attr_TexCoord0;

uniform mat4		u_ModelViewMatrix;
uniform mat4		u_ModelViewProjectionMatrix;
uniform mat4		u_ModelMatrix;
uniform vec4		u_ClipPlane;
uniform vec2		u_TexOffset;


varying vec2		var_TexCoord;	// for alpha-testing

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );
	vec4 worldpos = u_ModelMatrix * position;

	// transform vertex position into homogenous clip-space
	gl_Position = u_ModelViewProjectionMatrix * worldpos;
	gl_ClipDistance[0] = dot( worldpos, u_ClipPlane );

	var_TexCoord = attr_TexCoord0 + u_TexOffset;
}