/*
gaussblur_fp.glsl - Bloom onscreen using mips program
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

#include "texfetch.h"

uniform sampler2D	u_ScreenMap;
uniform vec2		u_ScreenSizeInv;	// actually screensize

varying vec2		var_TexCoord;

void main()
{	
	vec4 tex = vec4(0.0);	
	vec2 texSize = floor(u_ScreenSizeInv * 0.5); 

	tex += textureBicubic(u_ScreenMap, var_TexCoord, 2.0, texSize) * 2.0;
	texSize = floor(texSize * 0.5);
	tex += textureBicubic(u_ScreenMap, var_TexCoord, 3.0, texSize);
	texSize = floor(texSize * 0.5);
	tex += textureBicubic(u_ScreenMap, var_TexCoord, 4.0, texSize);
	texSize = floor(texSize * 0.5);
	tex += textureBicubic(u_ScreenMap, var_TexCoord, 5.0, texSize);
	texSize = floor(texSize * 0.5);
	tex += textureBicubic(u_ScreenMap, var_TexCoord, 6.0, texSize);
	tex *= 0.2;		
		
	tex += textureLod(u_ScreenMap, var_TexCoord, 1.0);	
	tex *= 0.25;
	
	gl_FragColor = tex;
}
