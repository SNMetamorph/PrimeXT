/*
depth_studio_vp.glsl - shadow-pass fragment shader
Copyright (C) 2019 Uncle Mike

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
#include "skinning.h"

attribute vec3		attr_Position;
attribute vec2		attr_TexCoord0;

uniform float	u_SwayHeight;
uniform float	u_RealTime;

varying vec2		var_TexDiffuse;

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );

	if( bool( u_SwayHeight != 0.0 ) && position.z > u_SwayHeight )
    {
        position.x += position.z * 0.025 * sin( position.z + u_RealTime * 0.25 );
        position.y += position.z * 0.025 * cos( position.z + u_RealTime * 0.25 );
    }

	mat4 boneMatrix = ComputeSkinningMatrix();
	vec4 worldpos = boneMatrix * position;

	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	var_TexDiffuse = attr_TexCoord0;
}