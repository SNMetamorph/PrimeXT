/*
matrix.h - matrix GLSL utility
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef MATRIX_H
#define MATRIX_H

mat4 Mat4FromOriginQuat( const in vec4 quaternion, const in vec3 origin )
{
	mat4 mat;

	mat[0][0] = 1.0 - 2.0 * (quaternion.y * quaternion.y + quaternion.z * quaternion.z);
	mat[1][0] = 2.0 * (quaternion.x * quaternion.y - quaternion.z * quaternion.w);
	mat[2][0] = 2.0 * (quaternion.x * quaternion.z + quaternion.y * quaternion.w);
	mat[3][0] = origin.x;
	mat[0][1] = 2.0 * (quaternion.x * quaternion.y + quaternion.z * quaternion.w);
	mat[1][1] = 1.0 - 2.0 * (quaternion.x * quaternion.x + quaternion.z * quaternion.z);
	mat[2][1] = 2.0 * (quaternion.y * quaternion.z - quaternion.x * quaternion.w);
	mat[3][1] = origin.y;
	mat[0][2] = 2.0 * (quaternion.x * quaternion.z - quaternion.y * quaternion.w);
	mat[1][2] = 2.0 * (quaternion.y * quaternion.z + quaternion.x * quaternion.w);
	mat[2][2] = 1.0 - 2.0 * (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
	mat[3][2] = origin.z;
	mat[0][3] = 0.0;
	mat[1][3] = 0.0;
	mat[2][3] = 0.0;
	mat[3][3] = 1.0;

	return mat;
}

mat4 Mat4FromVec4Array( const vec4 v0, const vec4 v1, const vec4 v2 )
{
	mat4 m;

	m[0][0] = v0.x;
	m[0][1] = v0.y;
	m[0][2] = v0.z;
	m[0][3] = 0.0;
	m[1][0] = v1.x;
	m[1][1] = v1.y;
	m[1][2] = v1.z;
	m[1][3] = 0.0;
	m[2][0] = v2.x;
	m[2][1] = v2.y;
	m[2][2] = v2.z;
	m[2][3] = 0.0;
	m[3][0] = v0.w;
	m[3][1] = v1.w;
	m[3][2] = v2.w;
	m[3][3] = 1.0;

	return m;
}

// glLoadIdentity();
// glTranslate( 0.5, 0.5, 0.5 );
// glScale( 0.5, scale_y, 0.5 );
mat4 Mat4Texture( const float scale_y )
{
	mat4 m;

	m[0][0] = 0.5;
	m[1][0] = 0.0;
	m[2][0] = 0.0;
	m[3][0] = 0.5;
	m[0][1] = 0.0;
	m[1][1] = scale_y;
	m[2][1] = 0.0;
	m[3][1] = 0.5;
	m[0][2] = 0.0;
	m[1][2] = 0.0;
	m[2][2] = 0.5;
	m[3][2] = 0.5;
	m[0][3] = 0.0;
	m[1][3] = 0.0;
	m[2][3] = 0.0;
	m[3][3] = 1.0;

	return m;
}

#endif//MATRIX_H