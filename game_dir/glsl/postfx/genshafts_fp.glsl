/*
genshafts_fp.glsl - generate sun shafts
Copyright (C) 2014 Uncle Mike

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

uniform sampler2D		u_ScreenMap;
uniform sampler2D		u_DepthMap;
uniform float		u_zFar;

varying vec2       		var_TexCoord;

void main( void )
{
	float sceneDepth = linearizeDepth( u_zFar, texture( u_DepthMap, var_TexCoord ).r );
	vec4 sceneColor = texture( u_ScreenMap, var_TexCoord );
	float fShaftsMask = RemapVal( sceneDepth, Z_NEAR, u_zFar, 0.0, 1.0 ); // now it's blend, not depth
	sceneColor.rgb = vec3( GetLuminance( sceneColor.rgb ));

	// g-cont. use linear depth scale to prevent parazite lighting
	gl_FragColor = vec4( sceneColor.rgb * fShaftsMask, 1.0 - fShaftsMask );
}