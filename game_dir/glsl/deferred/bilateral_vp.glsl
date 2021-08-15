/*
bilateral_vp.glsl - bilateral filter
Copyright (C) 2019 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

uniform vec2	u_ScreenSizeInv;

varying vec2	v_TexCoord;
varying vec4	v_TexCoords[4];

void main( void )
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	// Copy the texture coord
	v_TexCoord = gl_MultiTexCoord0.xy;

	// Compute the offset texture coords
	v_TexCoords[0].st = gl_MultiTexCoord0.xy - u_ScreenSizeInv * 1.5;
	v_TexCoords[0].pq = gl_MultiTexCoord0.xy + u_ScreenSizeInv * 1.5;

	v_TexCoords[1].st = gl_MultiTexCoord0.xy - u_ScreenSizeInv * 3.5;
	v_TexCoords[1].pq = gl_MultiTexCoord0.xy + u_ScreenSizeInv * 3.5;

	v_TexCoords[2].st = gl_MultiTexCoord0.xy - u_ScreenSizeInv * 5.5;
	v_TexCoords[2].pq = gl_MultiTexCoord0.xy + u_ScreenSizeInv * 5.5;

	v_TexCoords[3].st = gl_MultiTexCoord0.xy - u_ScreenSizeInv * 7.5;
	v_TexCoords[3].pq = gl_MultiTexCoord0.xy + u_ScreenSizeInv * 7.5;
}
