/*
debug_draw_vp.glsl - debug draw
Copyright (C) 2025 ugo_zapad

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

attribute vec3	attr_Position;
attribute vec3  attr_Normal; // Color

uniform mat4    u_ModelMatrix; // model-view matrix
uniform mat4    u_ProjectionMatrix; // projection matrix

varying vec3 	var_Color;

void main()
{
    var_Color = attr_Normal;
	gl_Position = u_ProjectionMatrix * u_ModelMatrix * vec4(attr_Position, 1.0);
}
