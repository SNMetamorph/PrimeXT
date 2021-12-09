/*
deluxemap.h - directional lightmaps
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

#ifndef DELUXEMAP_H
#define DELUXEMAP_H

#include "texfetch.h"

uniform sampler2D		u_LightMap;
uniform sampler2D		u_DeluxeMap;
uniform float		u_AmbientFactor;
uniform float		u_DiffuseFactor;
uniform float		u_LightGamma;

void ApplyLightStyle( const vec3 lminfo, const vec3 N, const vec3 V, const vec3 glossmap, float smoothness, inout vec3 light, inout vec3 gloss )
{
	vec4 lmsrc = lightmap2D( u_LightMap, lminfo.xy, u_LightGamma );
	vec3 lightmap = lmsrc.rgb * LIGHT_SCALE;

#if defined( HAS_DELUXEMAP )
	vec3 deluxmap = deluxemap2D( u_DeluxeMap, lminfo.xy );
	vec3 L = normalize( deluxmap );
	float NdotL = saturate( dot( N, L ));
	float NdotB = ComputeStaticBump( L, N, u_AmbientFactor );

#if defined( HAS_NORMALMAP )
	lightmap *= DiffuseBRDF( N, V, L, smoothness, NdotB );
#endif
#endif

#if defined( HAS_DELUXEMAP ) && defined( LIGHTVEC_DEBUG )
	light += ( N + deluxmap ) * lminfo.z;
#else
	light += (lightmap) * lminfo.z;
#endif

#if defined( HAS_DELUXEMAP ) && defined( HAS_GLOSSMAP )
	float scaleD = smoothstep( length( lightmap * 4.0 ), 0.0, u_DiffuseFactor );
	vec3 specular = SpecularBRDF( N, V, L, smoothness, glossmap ) * lightmap * lminfo.z * lmsrc.a * NdotL;
	gloss += specular;
#endif
}

#endif//DELUXEMAP_H