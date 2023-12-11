/*
light_bmodel_fp.glsl - g-buffer rendering brush surfaces
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
#include "deferred\fitnormals.h"

uniform sampler2D		u_ColorMap;

// shared variables
varying mat3		var_WorldMat;
varying vec2		var_TexDiffuse;
varying vec4		var_LightNums0;
varying vec4		var_LightNums1;

void main( void )
{
	if( texture( u_ColorMap, var_TexDiffuse ).a < BMODEL_ALPHA_THRESHOLD )
		discard;

	vec4 normal = vec4( 0.0 );

	normal.xyz = normalize( var_WorldMat[2] );
	normal.xyz = ( normal.xyz + 1.0 ) * 0.5;

	CompressUnsignedNormalToNormalsBuffer( normal.xyz );

#if defined( LIGHTING_FULLBRIGHT )
	normal.w = 1.0; // this pixel is fullbright
#endif
	gl_FragData[0] = normal;		// X, Y, Z, flag fullbright
	gl_FragData[1] = var_LightNums0;	// number of lights affected this face
	gl_FragData[2] = var_LightNums1;
}