/*
tonemap_fp.glsl - tone mapping
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

#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_ScreenMap;
uniform sampler2D   u_ColorMap;
varying vec2	    var_TexCoord;

// vec3 TonemapExposure(vec3 source, float exposure)
// {
//     return vec3(1.0) - exp(-exposure * source);
// }

// vec3 TonemapFilmic(vec3 source)
// {
// 	source = max(vec3(0.), source - vec3(0.004));
// 	source = (source * (6.2 * source + .5)) / (source * (6.2 * source + 1.7) + 0.06);
// 	return source;
// }

// tonemapping by ForhaxeD, too dark for some reason
// vec3 TonemapForhaxed(vec3 source, float averageLuminance)
// {
//     const float middleGrey = 0.6f;
//     const float maxLuminance = 16.0f;
//     float lumAvg = exp(averageLuminance);
//     float lumPixel = GetLuminance(source);
//     float lumScaled = (lumPixel * middleGrey) / lumAvg;
//     float lumCompressed = (lumScaled * (1 + (lumScaled / (maxLuminance * maxLuminance)))) / (1 + lumScaled);
//     return lumCompressed * source;
// }

float CurveMGS5(float x)
{
    const float a = 0.6;
    const float b = 0.45333;
    if (x <= a)
        return x;
    else
        return min(1.0, a + b - (b*b) / (x - a + b));
}

vec3 TonemapMGS5(vec3 source)
{
    return vec3(
        CurveMGS5(source.r),
        CurveMGS5(source.g),
        CurveMGS5(source.b)
    );
}

void main()
{
    vec3 output;
    vec3 source = texture2D(u_ScreenMap, var_TexCoord).rgb;
    float exposure = texture2D(u_ColorMap, vec2(0.5)).r;

	output = TonemapMGS5(source * exposure); // tone compression with exposure
	output = pow(output, vec3(1.0 / SCREEN_GAMMA)); // gamma-correction (linear space -> sRGB space)
    gl_FragColor = vec4(output, 1.0);
}
