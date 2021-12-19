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

// vec3 TonemapExposure(vec3 source)
// {
//     return vec3(1.0) - exp(-source);
// }

// vec3 TonemapFilmic(vec3 source)
// {
// 	source = max(vec3(0.), source - vec3(0.004));
// 	source = (source * (6.2 * source + .5)) / (source * (6.2 * source + 1.7) + 0.06);
// 	return source;
// }

// vec3 TonemapMGS5(vec3 source)
// {
//     const float a = 0.6;
//     const float b = 0.45333;
//     vec3 t = step(a, source);
//     vec3 f1 = source;
//     vec3 f2 = min(vec3(1.0), a + b - (b*b) / (f1 - a + b));
//     return mix(f1, f2, t);
// }

vec3 TonemapReinhard(vec3 source)
{
    return source / (source + vec3(1.0));
}

void main()
{
    vec3 output;
    vec3 source = texture2D(u_ScreenMap, var_TexCoord).rgb;
    float exposure = texture2D(u_ColorMap, vec2(0.5)).r;

	output = TonemapReinhard(source * exposure); // tone compression with exposure
	output = pow(output, vec3(1.0 / SCREEN_GAMMA)); // gamma-correction (linear space -> sRGB space)
    gl_FragColor = vec4(output, 1.0);
}
