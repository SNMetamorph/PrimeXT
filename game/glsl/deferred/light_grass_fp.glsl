/*
light_grass_fp.glsl - g-buffer rendering grass surfaces
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

varying vec2		var_TexDiffuse;
varying vec3		var_WorldNormal;

void main( void )
{
	if( colormap2D( u_ColorMap, var_TexDiffuse ).a < BMODEL_ALPHA_THRESHOLD )
		discard;

	float sign = -1.0;

	if( bool( gl_FrontFacing ))
		sign = -sign;
	vec3 normal = (sign * var_WorldNormal + 1.0) * 0.5;

	CompressUnsignedNormalToNormalsBuffer( normal.xyz );

	gl_FragData[0] = vec4( normal, GRASS_FLATSHADE );	// X, Y, Z, flag fullbright
	gl_FragData[1] = u_LightNums0 * (1.0 / 255.0);		// number of lights affected this face
	gl_FragData[2] = u_LightNums1 * (1.0 / 255.0);
}