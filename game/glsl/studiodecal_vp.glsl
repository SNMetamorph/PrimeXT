/*
DecalStudio_vp.glsl - vertex uber shader for studio decals
Copyright (C) 2015 Uncle Mike

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

varying vec4	var_VertexColor;

void main( void )
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;

	// decal coords
	gl_TexCoord[0] = gl_MultiTexCoord0;
	var_VertexColor = gl_Color;
}