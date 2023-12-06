/*
gaussblur_fp.glsl - Gaussian Blur For Mips program
Copyright (C) 2021 ncuxonaT

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "mathlib.h"

uniform sampler2D	u_ScreenMap;
uniform vec2		u_ScreenSizeInv;
uniform float		u_MipLod;	// source mip level, not target one
uniform vec4		u_TexCoordClamp;
uniform int			u_BloomFirstPass;
uniform float 		u_RefractScale; // bloom scale

varying vec2		var_TexCoord;

vec4 SampleScreenTexture(float offsetX, float offsetY)
{
	vec2 texelSize = u_ScreenSizeInv;
	vec2 coordOffset = vec2(offsetX, offsetY) * texelSize;
	vec2 texCoords = clamp(var_TexCoord + coordOffset, u_TexCoordClamp.xy, u_TexCoordClamp.zw);
	return textureLod(u_ScreenMap, texCoords, u_MipLod);
}

void main()
{
	// gaussian 3x3, sum of weights should be equal 1.0
	// weights and samples distribution scheme:
	// 0.0625   0.125   0.0625  |  6   4   5
	// 0.125    0.25    0.125   |  2   0   1
	// 0.0625   0.125   0.0625  |  7   3   8
	float weight[3];
	weight[0] = 0.25;
	weight[1] = 0.125;
	weight[2] = 0.0625;	
			
	vec4 texSamples[9];
	texSamples[0] = SampleScreenTexture(0.0, 0.0);
	texSamples[1] = SampleScreenTexture(1.0, 0.0);
	texSamples[2] = SampleScreenTexture(-1.0, 0.0);
	texSamples[3] = SampleScreenTexture(0.0, -1.0);
	texSamples[4] = SampleScreenTexture(0.0, 1.0);
	texSamples[5] = SampleScreenTexture(1.0, 1.0);
	texSamples[6] = SampleScreenTexture(-1.0, 1.0);
	texSamples[7] = SampleScreenTexture(-1.0, -1.0);
	texSamples[8] = SampleScreenTexture(1.0, -1.0);

	vec4 outputColor;
	outputColor = weight[0] * texSamples[0];
	outputColor += weight[1] * (texSamples[1] + texSamples[2] + texSamples[3] + texSamples[4]);
	outputColor += weight[2] * (texSamples[5] + texSamples[6] + texSamples[7] + texSamples[8]);	
	
	if (u_BloomFirstPass > 0) 
	{
		const float threshold = 0.8;
		const float transitionSmooth = 0.7;
		float luminance = GetLuminance(outputColor.rgb);
		outputColor *= smoothstep(threshold - transitionSmooth, threshold, luminance);
		outputColor = clamp(outputColor * u_RefractScale, vec4(0.0), vec4(25000.0));
	}

	gl_FragColor = outputColor;
}
