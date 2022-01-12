/*
bilateral_fp.glsl - bilateral filter
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
#include "mathlib.h"

uniform sampler2D	u_ColorMap;
uniform sampler2D	u_DepthMap;
uniform vec2	u_ShadowParams;
uniform float	u_zFar;

varying vec2	v_TexCoord;
varying vec4	v_TexCoords[4];

float AddSampleContribution( in vec2 texCoord, in float depth, in float weight, inout vec4 color )
{
	float	sampleDepth;

	// Load the depth from the depth map
	sampleDepth = texture2D( u_DepthMap, texCoord ).r;

	// Transform the depth into eye space
	sampleDepth = linearizeDepth( u_zFar, sampleDepth );

	// Check for depth discontinuities
	if( abs( depth - sampleDepth ) > 5.0 )
		return 0.0;

	// Sample the color map and add its contribution
	color += texture2D( u_ColorMap, texCoord ) * weight;

	return weight;
}

void main( void )
{
	vec4	color;
	float	depth;
	float	weightSum = 0.153170;

	// Load the depth from the depth map
	depth = texture2D( u_DepthMap, v_TexCoord ).r;

	// Transform the depth into eye space
	depth = linearizeDepth( u_zFar, depth );

	// Blur using a 9x9 bilateral Gaussian filter
	color = texture2D( u_ColorMap, v_TexCoord ) * weightSum;

	weightSum += AddSampleContribution( v_TexCoords[0].st, depth, 0.144893, color );
	weightSum += AddSampleContribution( v_TexCoords[0].pq, depth, 0.144893, color );

	weightSum += AddSampleContribution( v_TexCoords[1].st, depth, 0.122649, color );
	weightSum += AddSampleContribution( v_TexCoords[1].pq, depth, 0.122649, color );

	weightSum += AddSampleContribution( v_TexCoords[2].st, depth, 0.092903, color );
	weightSum += AddSampleContribution( v_TexCoords[2].pq, depth, 0.092903, color );

	weightSum += AddSampleContribution( v_TexCoords[3].st, depth, 0.062970, color );
	weightSum += AddSampleContribution( v_TexCoords[3].pq, depth, 0.062970, color );

	// Write the final color
	gl_FragColor = color / weightSum;
}