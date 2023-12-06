/*
generate_luminance_fp.glsl - Luminance extraction shader for automatic exposure correction
Copyright (C) 2021 SNMetamorph

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

varying vec2		var_TexCoord;

float GetLogLuminance(vec3 source)
{
	float lum = GetLuminance(source);
	return lum / (1 + lum);
}

void main()
{
	float samples[4];
	vec2 pixelOffset = u_ScreenSizeInv;
	vec3 sourceColor = texture(u_ScreenMap, var_TexCoord).xyz;
	float weight = 1.05 - 0.2 * length(var_TexCoord - vec2(0.5, 0.5));

	samples[0] = GetLogLuminance(weight * sourceColor);
	samples[1] = GetLogLuminance(weight * texture(u_ScreenMap, var_TexCoord + vec2(pixelOffset.x, 0)).xyz);
	samples[2] = GetLogLuminance(weight * texture(u_ScreenMap, var_TexCoord + vec2(0, pixelOffset.y)).xyz);
	samples[3] = GetLogLuminance(weight * texture(u_ScreenMap, var_TexCoord + pixelOffset).xyz);

	float luminance = (samples[0] + samples[1] + samples[2] + samples[3]) / 4.0;
	gl_FragColor = vec4(luminance, GetLuminance(sourceColor), 0.0, 0.0);
}
