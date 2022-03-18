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
#include "lightmodel.h"
#include "material.h"

uniform sampler2D		u_LightMap;
uniform sampler2D		u_DeluxeMap;
uniform float		u_AmbientFactor;
uniform float		u_DiffuseFactor;
uniform float		u_LightGamma;

void ApplyLightStyle( const vec3 lminfo, const vec3 N, const vec3 V, const vec4 albedo, in MaterialData mat, inout LightingData lighting )
{
	const float lightstyleMult = lminfo.z;
	vec4 lmsrc = lightmap2D( u_LightMap, lminfo.xy, u_LightGamma );
	vec3 lightmap = lmsrc.rgb * lightstyleMult;
	
#if defined( HAS_DELUXEMAP )
	vec3 deluxmap = deluxemap2D( u_DeluxeMap, lminfo.xy );
	vec3 L = normalize( deluxmap );

#if defined( APPLY_PBS )
	// ideally, this should be done in pxrad, but it's not and we should do it here :)
	float flatLightFactor = max(dot(L, vec3(0.0, 0.0, 1.0)), 0.025);
	lightmap /= flatLightFactor;
#endif
	LightingData lighting2 = ComputeLighting(N, V, L, albedo.rgb, lightmap, mat);
	
#if defined( LIGHTVEC_DEBUG )
	lighting.diffuse += (N + deluxmap) * lightstyleMult;
#else
	lighting.diffuse += lighting2.diffuse; // diffuse lighting
	lighting.specular += lighting2.specular; // specular lighting
#endif

#else // !HAS_DELUXEMAP
	// fallback for old maps without deluxedata
	lighting.diffuse += lightmap; 
	return;
#endif // HAS_DELUXEMAP
}

#endif // DELUXEMAP_H
