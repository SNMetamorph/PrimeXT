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
#include "mathlib.h"
#include "texfetch.h"
#include "deferred\fitnormals.h"

uniform sampler2D		u_ColorMap;
uniform vec4		u_LightNums0;
uniform vec4		u_LightNums1;

// shared variables
varying vec2		var_TexDiffuse;
varying vec3		var_Normal;

void main( void )
{
	vec4 albedo = colormap2D( u_ColorMap, var_TexDiffuse );
	vec4 normal = vec4( 0.0 );

	if( albedo.a < STUDIO_ALPHA_THRESHOLD )
		discard;

	normal.xyz = normalize( var_Normal );
	normal.xyz = ( normal.xyz + 1.0 ) * 0.5;

	CompressUnsignedNormalToNormalsBuffer( normal.xyz );

#if defined( LIGHTING_FLATSHADE )
	normal.w = NORMAL_FLATSHADE;
#endif

#if defined( LIGHTING_FULLBRIGHT )
	normal.w = 1.0; // this pixel is fullbright
#endif
	gl_FragData[0] = normal;	// X, Y, Z, flag fullbright
	gl_FragData[1] = u_LightNums0 * (1.0 / 255.0);
	gl_FragData[2] = u_LightNums1 * (1.0 / 255.0);
}