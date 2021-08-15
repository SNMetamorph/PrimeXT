/*
grassdata.h - implicit grass data
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

#ifndef GRASSDATA_H
#define GRASSDATA_H

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

#endif//GRASSDATA_H