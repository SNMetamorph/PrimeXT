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

vec3 ColorGradient(float t)
{
    float red = smoothstep(0.33, 0.66, t);
    float green = smoothstep(0.66, 1.0, t);
    float blue = smoothstep(0.00, 0.66, t) - smoothstep(0.33, 0.66, t);
    return vec3(red, green, blue);
}

float GetGradientFraction(float luminance, float limit, float slope)
{
    return clamp(pow(luminance / limit, slope), 0.0, 1.0);
}

//vec3 TonemapExposure(vec3 source)
//{
//    return vec3(1.0) - exp(-source);
//}

//vec3 TonemapFilmic(vec3 source)
//{
//    source = max(vec3(0.), source - vec3(0.004));
//    source = (source * (6.2 * source + .5)) / (source * (6.2 * source + 1.7) + 0.06);
//    return source;
//}

//vec3 TonemapReinhard(vec3 source)
//{
//    return source / (source + vec3(1.0));
//}

vec3 TonemapMGS5(vec3 source)
{
    const float a = 0.6;
    const float b = 0.45333;
    vec3 t = step(a, source);
    vec3 f1 = source;
    vec3 f2 = min(vec3(1.0), a + b - (b*b) / (f1 - a + b));
    return mix(f1, f2, t);
}

void main()
{
    vec3 outputColor;
    vec3 source = texture(u_ScreenMap, var_TexCoord).rgb;

#if defined( DEBUG_LUMINANCE )
    const float slope = 0.9;
    const float limit = 20.0;
    float luminance = GetLuminance(source);
    float fraction = GetGradientFraction(luminance, limit, slope);
    outputColor = ColorGradient(fraction);
#else
    float exposure = texture(u_ColorMap, vec2(0.5)).r;
	outputColor = TonemapMGS5(source * exposure); // tone compression with exposure
#endif
    gl_FragColor = vec4(outputColor, 1.0);
}
