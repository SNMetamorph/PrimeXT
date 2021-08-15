/*
tnbasis.h - compute TBN matrix
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef TNBASIS_H
#define TNBASIS_H

attribute vec3		attr_Normal;

#if defined( COMPUTE_TBN )
attribute vec3		attr_Tangent;
attribute vec3		attr_Binormal;
#endif

mat3 ComputeTBN( const in mat4 modelMatrix )
{
	mat3	tbn;
#if defined( COMPUTE_TBN )
	tbn[0] = (modelMatrix * vec4( normalize( attr_Tangent ), 0.0 )).xyz;
	tbn[1] = (modelMatrix * vec4( normalize( attr_Binormal ), 0.0 )).xyz;
#else
	tbn[0] = vec3( 0.0 ); // no data
	tbn[1] = vec3( 0.0 );
#endif
	tbn[2] = (modelMatrix * vec4( normalize( attr_Normal ), 0.0 )).xyz;

	return tbn;
}

#endif//TNBASIS_H