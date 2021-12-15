/*
lightmodel.h - BRDF models
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

#ifndef LIGHTMODEL_H
#define LIGHTMODEL_H

#include "const.h"

#define FALL_LIGHTMODEL_BLINN		// default lightmodel Blinn\Phong
#define BRDF_LIGHTMODEL_DISNEY		// physical based lightmodel Disney\Oren Nayar

float SmoothnessToRoughness( float smoothness )
{
	return ( 1.0 - smoothness ) * ( 1.0 - smoothness );
}

// used for spot/omni lights
float LightAttenuation(vec3 lightVec, float radius)
{
#if 0
	// quadratic attenuation with culling, based on filament and UE
	float distanceSquare = dot(lightVec, lightVec);
	float attenFactor = distanceSquare * radius * radius;
	float smoothFactor = max(1.0 - attenFactor * attenFactor, 0.0);
	return (smoothFactor * smoothFactor) / max(distanceSquare, 1.0);	
#else
	// default attenuation model from P2:Savior
	float distance = length(lightVec);
	return 1.0 - saturate(pow(distance * radius, LIGHT_FALLOFF));
#endif
}

#if defined( BRDF_LIGHTMODEL_DISNEY )
float DiffuseBRDF( vec3 N, vec3 V, vec3 L, float Gloss, float NdotL )
{
#if defined( APPLY_PBS )
	// TODO: Share computations with Specular BRDF
	float roughness = SmoothnessToRoughness( min( Gloss, 1.0 ));
	float VdotH = saturate( dot( V, normalize( V + L )));
	float NdotV = max( abs( dot( N, V )) + 1e-9, 0.1 );
	
	// Burley BRDF with renormalization to conserve energy
	float energyBias = 0.5 * roughness;
	float energyFactor = mix( 1.0, 1.0 / 1.51, roughness );
	float fd90 = energyBias + 2.0 * VdotH * VdotH * roughness;
	float scatterL = mix( 1.0, fd90, pow( 1.0 - NdotL, 5.0 ));
	float scatterV = mix( 1.0, fd90, pow( 1.0 - NdotV, 5.0 ));

	return scatterL * scatterV * energyFactor * NdotL;
#else
	return NdotL;
#endif
}

vec3 SpecularBRDF( vec3 N, vec3 V, vec3 L, float Gloss, vec3 SpecCol )
{
#if defined( APPLY_PBS )
	float m = max( SmoothnessToRoughness( Gloss ), 0.001 );  // Prevent highlights from getting too tiny without area lights
	return SpecularBRDF( N, V, L, m, SpecCol, 1.0 );
#else
#if defined( FALL_LIGHTMODEL_BLINN )
	return SpecCol * pow( max( dot( N, normalize( V + L )), 0.0 ), ( Gloss * Gloss ) * 256.0 ); // Blinn
#else
	return SpecCol * pow( max( dot( reflect( -L, N ), V ), 0.0 ), ( Gloss * Gloss ) * 256.0 ); // Phong
#endif
#endif
}

#endif
#endif//LIGHTMODEL_H
