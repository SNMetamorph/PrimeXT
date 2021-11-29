/*
downscale_luminance_fp.glsl - 
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
	vec4 output;
	vec4 texSample[9];
	vec2 pixelOffset = u_ScreenSizeInv;

	texSample[0] = texture2DLod(u_ScreenMap, clamp(var_TexCoord, u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[1] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(pixelOffset.x, 0.0), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[2] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(-pixelOffset.x, 0.0), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[3] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(0.0, -pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[4] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(0.0, pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);		
	texSample[5] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(pixelOffset.x, pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[6] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(-pixelOffset.x, pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[7] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(-pixelOffset.x, -pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);
	texSample[8] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(pixelOffset.x, -pixelOffset.y), u_TexCoordClamp.xy, u_TexCoordClamp.zw), u_MipLod);	
	
	output = texSample[0] + texSample[1] + texSample[2];
	output += texSample[3] + texSample[4] + texSample[5];
	output += texSample[6] + texSample[7] + texSample[8];
	output /= 9.0;
	
	gl_FragColor = output;
}