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

uniform sampler2D	u_ScreenMap;
uniform float       u_Exposure;
varying vec2	    var_TexCoord;

float rand(vec2 co)
{
    float a = 0.129898;
    float b = 0.78233;
    float c = 43758.5453;
    float dt = dot(co.xy ,vec2(a,b));
    float sn = mod(dt,M_PI);
    return abs(fract(sin(sn) * c));
}

void main( void )
{
    const float gamma = 2.2;
	vec3 diffuse = texture2D( u_ScreenMap, var_TexCoord ).rgb;
	diffuse = vec3(1.0) - exp(-diffuse * u_Exposure); // tone compression with exposure
	//diffuse = pow(diffuse, vec3(1.0 / gamma)); // gamma-correction
    //= pow(diffuse, vec3(0.454545)) + 0.5 * vec3(rand(gl_FragCoord.xy) * 0.00784 - 0.00392);	
    gl_FragColor = vec4(diffuse, 1.0);
}