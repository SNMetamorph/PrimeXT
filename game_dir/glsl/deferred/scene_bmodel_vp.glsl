/*
scene_bmodel_vp.glsl - g-buffer rendering brush surfaces
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
#include "mathlib.h"
#include "tnbasis.h"

attribute vec3		attr_Position;
attribute vec4		attr_TexCoord0;	// diffuse\terrain
attribute vec4		attr_TexCoord1;	// lightmap 0-1
attribute vec4		attr_TexCoord2;	// lightmap 2-3
attribute vec4		attr_LightNums0;
attribute vec4		attr_LightNums1;

uniform mat4		u_ModelMatrix;
uniform vec4		u_LightNums0;
uniform vec4		u_LightNums1;
uniform vec2		u_DetailScale;
uniform vec2		u_TexOffset;	// conveyor stuff
uniform vec4		u_ViewOrigin;

varying mat3		var_WorldMat;
varying vec2		var_TexDiffuse;
varying vec2		var_TexDetail;
varying vec2		var_TexGlobal;
varying vec3		var_Position;
varying vec4		var_LightNums0;
varying vec4		var_LightNums1;

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 ); // in object space
	vec4 worldpos = u_ModelMatrix * position;

	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	var_WorldMat = ComputeTBN( u_ModelMatrix );

	// used for diffuse, normalmap, specular and height map
	var_TexDiffuse = ( attr_TexCoord0.xy + u_TexOffset );
	var_TexDetail = ( attr_TexCoord0.xy + u_TexOffset ) * u_DetailScale;
	var_TexGlobal = attr_TexCoord0.zw;
	var_Position = worldpos.xyz;

	if( u_ViewOrigin.w != 0.0 )
	{
		// moveable brush models
		var_LightNums0 = u_LightNums0 * (1.0 / 255.0);
		var_LightNums1 = u_LightNums1 * (1.0 / 255.0);
	}
	else
	{
		// static world brushes
		var_LightNums0 = attr_LightNums0 * (1.0 / 255.0);
		var_LightNums1 = attr_LightNums1 * (1.0 / 255.0);
	}
}