/*
fresnel.h - fresnel approximations
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

#ifndef FRESNEL_H
#define FRESNEL_H

#include "mathlib.h"

#define FRESNEL_FACTOR	4.0

float GetFresnel( float NdotI, float bias, float power )
{
	float facing = (1.0 - NdotI);
	return saturate( bias + (1.0 - bias) * pow( facing, power )) + 0.001;
}

float GetFresnel( const vec3 v, const vec3 n, float fresnelExp, float scale )
{
	return 0.001 + pow( 1.0 - max( dot( n, v ), 0.0 ), fresnelExp ) * scale;
}

vec3 GetEnvmapFresnel( vec3 specCol0, float gloss, float fNdotE, float exp )
{
	const vec3 specCol90 = vec3( 1.0 );

	// Empirical approximation to the reduced gain at grazing angles for rough materials
	return mix( specCol0, specCol90, pow( 1.0 - saturate( fNdotE ), exp ) / ( 40.0 - 39.0 * gloss ));
}

vec3 GetEnvBRDFFresnel( vec3 specCol0, float gloss, float fNdotV, in sampler2D sampEnvBRDF )
{
	// Use a LUT that contains the numerically integrated BRDF for given N.V and smoothness parameters
	// x: integral for base reflectance 0.0, y: integral for base reflectance 1.0
	
	vec2 envBRDF = texture2D( sampEnvBRDF, vec2( fNdotV, gloss )).xy;
	return mix( envBRDF.xxx, envBRDF.yyy, specCol0 );
}

#endif//FRESNEL_H