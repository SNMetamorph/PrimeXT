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

uniform sampler2D	u_ScreenMap;
uniform vec2		u_ScreenSizeInv;
uniform float		u_MipLod;	// source mip level, not target one
uniform vec4		u_TexCoordClamp;
uniform int			u_BloomFirstPass;

varying vec2		var_TexCoord;

void main( void )
{
	//gaussian 3x3
	float weight[3];
	//weight[0] = 0.2;
	//weight[1] = 0.12;
	//weight[2] = 0.08;		

	//better?
	weight[0] = 0.25;
	weight[1] = 0.125;
	weight[2] = 0.0625;	
			
	vec4 tex_sample[9];
	tex_sample[0] = texture2DLod(u_ScreenMap, clamp(var_TexCoord ,u_TexCoordClamp.xy , u_TexCoordClamp.zw), u_MipLod);
	tex_sample[1] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(u_ScreenSizeInv.x , 0.0) ,u_TexCoordClamp.xy , u_TexCoordClamp.zw), u_MipLod);
	tex_sample[2] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(-u_ScreenSizeInv.x , 0.0) ,u_TexCoordClamp.xy , u_TexCoordClamp.zw), u_MipLod);
	tex_sample[3] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(0.0 , -u_ScreenSizeInv.y) ,u_TexCoordClamp.xy , u_TexCoordClamp.zw), u_MipLod);
	tex_sample[4] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(0.0 , u_ScreenSizeInv.y) ,u_TexCoordClamp.xy , u_TexCoordClamp.zw), u_MipLod);		
	tex_sample[5] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(u_ScreenSizeInv.x , u_ScreenSizeInv.y) ,u_TexCoordClamp.xy , u_TexCoordClamp.zw), u_MipLod);
	tex_sample[6] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(-u_ScreenSizeInv.x , u_ScreenSizeInv.y) ,u_TexCoordClamp.xy , u_TexCoordClamp.zw), u_MipLod);
	tex_sample[7] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(-u_ScreenSizeInv.x , -u_ScreenSizeInv.y) ,u_TexCoordClamp.xy , u_TexCoordClamp.zw), u_MipLod);
	tex_sample[8] = texture2DLod(u_ScreenMap, clamp(var_TexCoord + vec2(u_ScreenSizeInv.x , -u_ScreenSizeInv.y) ,u_TexCoordClamp.xy , u_TexCoordClamp.zw), u_MipLod);	
	
	vec4 tex;
	tex = weight[0] * tex_sample[0];
	tex += weight[1] * (tex_sample[1] + tex_sample[2] + tex_sample[3] + tex_sample[4]);
	tex += weight[2] * (tex_sample[5] + tex_sample[6] + tex_sample[7] + tex_sample[8]);	
	
	if (u_BloomFirstPass > 0) 
	{
		tex *= min(1.0, length(tex) * 0.5);	// threshold
		float brightness = dot(tex.rgb, vec3(0.2126, 0.7152, 0.0722));
		tex *= min(brightness * 4.0, 1.0);
	}

	gl_FragColor = tex;
}