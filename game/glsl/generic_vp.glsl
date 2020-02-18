/*
generic_vp.glsl - generic vertex program
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

varying vec4	var_Vertex;
varying vec3 	var_Normal;
varying vec2	var_TexCoord;
varying vec3	var_VertexColor;

void main( void )
{
	var_Vertex = gl_Vertex;

	var_Normal = gl_Normal;
	var_TexCoord = gl_MultiTexCoord0.xy;
	var_VertexColor = gl_Color.rgb;

	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}