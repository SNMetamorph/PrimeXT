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

void main()
{
	vec4 color = texture2D(u_ScreenMap, var_TexCoord);
	float weight = 1.05 - 1.9 * length(var_TexCoord - vec2(0.5, 0.5));
	float luminance = max(weight, 0.0) * GetLuminance(color.rgb);
	gl_FragColor = vec4(luminance, 0.0, 0.0, 1.0);
}
