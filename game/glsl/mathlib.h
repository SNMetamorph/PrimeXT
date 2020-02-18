/*
mathlib.h - math subroutines for GLSL
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

#ifndef MATHLIB_H
#define MATHLIB_H

#ifndef saturate
#define saturate( x )	clamp( x, 0.0, 1.0 )
#endif

#define PLANE_S		vec4( 1.0, 0.0, 0.0, 0.0 )
#define PLANE_T		vec4( 0.0, 1.0, 0.0, 0.0 )
#define PLANE_R		vec4( 0.0, 0.0, 1.0, 0.0 )
#define PLANE_Q		vec4( 0.0, 0.0, 0.0, 1.0 )

#define sqr( x )		( x * x )
#define rcp( x )		( 1.0 / x )

#define UnpackVector( c )	fract( c * vec3( 1.0, 256.0, 65536.0 ))
#define UnpackNormal( c )	((fract( c * vec3( 1.0, 256.0, 65536.0 )) - 0.5) * 2.0)

#define Q_mix( A, B, frac )	(A + (B - A) * frac)

// remap a value in the range [A,B] to [C,D].
float RemapVal( float val, const in vec4 bounds )
{
	return bounds.z + (bounds.w - bounds.z) * (val - bounds.x) / (bounds.y - bounds.x);
}

// remap a value in the range [A,B] to [C,D].
float RemapVal( float val, float A, float B, float C, float D )
{
	return C + (D - C) * (val - A) / (B - A);
}

float GetLuminance( vec3 color ) { return dot( color, vec3( 0.2126, 0.7152, 0.0722 )); }

float linearizeDepth( float zfar, float depth )
{
//	return 2.0 * Z_NEAR * zfar / ( zfar + Z_NEAR - ( 2.0 * depth - 1.0 ) * ( zfar - Z_NEAR ));
	return -zfar * Z_NEAR / ( depth * ( zfar - Z_NEAR ) - zfar );
}

float linearizeDepth( float znear, float depth, float zfar )
{
	return -zfar * znear / ( depth * ( zfar - znear ) - zfar );
//	return 2.0 * znear * zfar / ( zfar + znear - ( 2.0 * depth - 1.0 ) * ( zfar - znear ));
}

vec2 GetTexCoordsForVertex( int vertexNum )
{
	if( vertexNum == 0 || vertexNum == 4 || vertexNum == 8 || vertexNum == 12 )
		return vec2( 0.0, 1.0 );
	if( vertexNum == 1 || vertexNum == 5 || vertexNum == 9 || vertexNum == 13 )
		return vec2( 0.0, 0.0 );
	if( vertexNum == 2 || vertexNum == 6 || vertexNum == 10 || vertexNum == 14 )
		return vec2( 1.0, 0.0 );
	return vec2( 1.0, 1.0 );
}

vec3 GetNormalForVertex( int vertexNum )
{
	if( vertexNum == 0 || vertexNum == 1 || vertexNum == 2 || vertexNum == 3 )
		return vec3( 0.0, 1.0, 0.0 );
	if( vertexNum == 4 || vertexNum == 5 || vertexNum == 6 || vertexNum == 7 )
		return vec3( -1.0, 0.0, 0.0 );
	if( vertexNum == 8 || vertexNum == 9 || vertexNum == 10 || vertexNum == 11 )
		return vec3( -0.707107, 0.707107, 0.0 );
	return vec3( 0.707107, 0.707107, 0.0 );
}

#endif//MATHLIB_H