/*
downscale_luminance_fp.glsl - Luminance mip-level downscaling shader
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
uniform float		u_MipLod;
uniform vec4		u_TexCoordClamp;

varying vec2		var_TexCoord;

void main()
{
	vec4 averaged;
	vec4 texSample[5];
	vec2 pixelOffset = u_ScreenSizeInv;

	texSample[0] = textureLod(u_ScreenMap, clamp(var_TexCoord, u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[1] = textureLod(u_ScreenMap, clamp(var_TexCoord + vec2(pixelOffset.x, pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[2] = textureLod(u_ScreenMap, clamp(var_TexCoord + vec2(-pixelOffset.x, -pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[3] = textureLod(u_ScreenMap, clamp(var_TexCoord + vec2(pixelOffset.x, -pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[4] = textureLod(u_ScreenMap, clamp(var_TexCoord + vec2(-pixelOffset.x, pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);		

	averaged = texSample[0] + texSample[1] + texSample[2] + texSample[3] + texSample[4];
	averaged /= 5.0;
	gl_FragColor = averaged;
}