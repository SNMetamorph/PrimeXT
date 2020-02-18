/*
BmodelDecal_vp.glsl - vertex uber shader for bmodel decals
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

attribute vec3		attr_Position;
attribute vec4		attr_TexCoord0;	// diffuse\terrain
attribute vec4		attr_TexCoord1;	// lightmap 0-1
attribute vec4		attr_TexCoord2;	// lightmap 2-3
attribute vec4		attr_LightStyles;

uniform float		u_LightStyleValues[MAX_LIGHTSTYLES];
uniform mat4		u_ModelMatrix;

varying vec4		var_TexDiffuse;
varying vec3		var_TexLight0;
varying vec3		var_TexLight1;
varying vec3		var_TexLight2;
varying vec3		var_TexLight3;

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );
	vec4 worldpos = u_ModelMatrix * position;
	int style;

	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// decal & surface scissor coords
	var_TexDiffuse = attr_TexCoord0;

	// setup lightstyles
#if defined( DECAL_APPLY_STYLE0 )
	var_TexLight0.xy = attr_TexCoord1.xy;
	var_TexLight0.z = u_LightStyleValues[int( attr_LightStyles[0] )];
#endif

#if defined( DECAL_APPLY_STYLE1 )
	var_TexLight1.xy = attr_TexCoord1.zw;
	var_TexLight1.z = u_LightStyleValues[int( attr_LightStyles[1] )];
#endif

#if defined( DECAL_APPLY_STYLE2 )
	var_TexLight2.xy = attr_TexCoord2.xy;
	var_TexLight2.z = u_LightStyleValues[int( attr_LightStyles[2] )];
#endif

#if defined( DECAL_APPLY_STYLE3 )
	var_TexLight3.xy = attr_TexCoord2.zw;
	var_TexLight3.z = u_LightStyleValues[int( attr_LightStyles[3] )];
#endif
}