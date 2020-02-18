/*
gaussblur_fp.glsl - Gaussian Blur program
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

uniform sampler2D	u_ColorMap;

uniform float	u_BlurFactor;

varying vec2	var_TexCoord;

void main( void )
{
	vec2 tx = var_TexCoord;
	vec4 sum = texture2D( u_ColorMap, tx ) * 0.134598;
	vec2 dx = vec2( 0.0 ); // assume no blur

#ifdef BLUR_X
	dx = vec2( 0.01953, 0.0 ) * u_BlurFactor;
#endif

#ifdef BLUR_Y
	dx = vec2( 0.0, 0.01953 ) * u_BlurFactor;
#endif
	vec2 sdx = dx;
	sum += (texture2D( u_ColorMap, tx + sdx ) + texture2D( u_ColorMap, tx - sdx )) * 0.127325;
	sdx += dx;
	sum += (texture2D( u_ColorMap, tx + sdx ) + texture2D( u_ColorMap, tx - sdx )) * 0.107778;
	sdx += dx;
	sum += (texture2D( u_ColorMap, tx + sdx ) + texture2D( u_ColorMap, tx - sdx )) * 0.081638;
	sdx += dx;
	sum += (texture2D( u_ColorMap, tx + sdx ) + texture2D( u_ColorMap, tx - sdx )) * 0.055335;
	sdx += dx;
	sum += (texture2D( u_ColorMap, tx + sdx ) + texture2D( u_ColorMap, tx - sdx )) * 0.033562;
	sdx += dx;
	sum += (texture2D( u_ColorMap, tx + sdx ) + texture2D( u_ColorMap, tx - sdx )) * 0.018216;
	sdx += dx;
	sum += (texture2D( u_ColorMap, tx + sdx ) + texture2D( u_ColorMap, tx - sdx )) * 0.008847;
	sdx += dx;

	gl_FragColor = sum;
}