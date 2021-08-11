/*
indirect.h - ambient cube
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

#ifndef INDIRECT_H
#define INDIRECT_H

#include "mathlib.h"

uniform vec3	u_AmbientCube[6];

vec3 AmbientLight( const in vec3 worldN )
{
	vec3 normSqr = sqr( worldN );
	int side_x = ( worldN.x < 0.0 ) ? 1 : 0;
	int side_y = ( worldN.y < 0.0 ) ? 1 : 0;
	int side_z = ( worldN.z < 0.0 ) ? 1 : 0;
	return normSqr.x * u_AmbientCube[side_x] + normSqr.y * u_AmbientCube[side_y+2] + normSqr.z * u_AmbientCube[side_z+4];
}

vec3 AmbientLight( const vec4 parm0, const vec4 parm1, const vec3 worldN )
{
	vec3 normSqr = sqr( worldN );
	vec3 linearColor = vec3( 0.0 );

	linearColor += normSqr.x * mix( UnpackVector( parm0.x ), UnpackVector( parm0.y ), step( 0.0, worldN.x ));
	linearColor += normSqr.y * mix( UnpackVector( parm0.z ), UnpackVector( parm1.x ), step( 0.0, worldN.y ));
	linearColor += normSqr.z * mix( UnpackVector( parm1.y ), UnpackVector( parm1.z ), step( 0.0, worldN.z ));

	return linearColor;
}

#endif//INDIRECT_H