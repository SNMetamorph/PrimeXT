/*
ibl_filter_specular_vp.glsl - pre-filtering of image-based lighting specular irradiance
Copyright (C) 2022 SNMetamorph

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
attribute vec3  attr_Normal;
attribute vec2	attr_TexCoord0;

uniform mat4    u_ModelMatrix; // model-view matrix
uniform mat4    u_ReflectMatrix; // projection matrix

varying vec3 	var_PositionLocal;

void main()
{
    var_PositionLocal = attr_Position;
	gl_Position = u_ReflectMatrix * u_ModelMatrix * vec4(attr_Position, 1.0);
}
