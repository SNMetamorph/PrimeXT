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

#define TITANFALL_BRDF
#define FALL_LIGHTMODEL_BLINN		// default lightmodel Blinn\Phong

struct LightingData
{
	vec3 diffuse;
	vec3 specular;
};

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

// for converting gloss value to roughness
float SmoothnessToRoughness(float smoothness)
{
	return (1.0 - smoothness) * (1.0 - smoothness);
}

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

vec3 SpecularBRDF( vec3 N, vec3 V, vec3 L, float m, vec3 f0, float NormalizationFactor )
{
	vec3 H = normalize( V + L );
	float m2 = m * m;

	// GGX NDF
	float NdotH = saturate( dot( N, H ));
	float spec = ( NdotH * m2 - NdotH ) * NdotH + 1.0;
	spec = m2 / ( spec * spec ) * NormalizationFactor;

	// Correlated Smith Visibility Term (including Cook-Torrance denominator)
	float NdotL = saturate( dot( N, L ));
	float NdotV = abs( dot( N, V )) + 1e-9;
	float Gv = NdotL * sqrt(( -NdotV * m2 + NdotV ) * NdotV + m2 );
	float Gl = NdotV * sqrt(( -NdotL * m2 + NdotL ) * NdotL + m2 );
	spec *= 0.5 / ( Gv + Gl );

	// Fresnel (Schlick approximation)
	float f90 = saturate( dot( f0, vec3( 0.33333 )) / 0.02 );  // Assume micro-occlusion when reflectance is below 2%
	vec3 fresnel = mix( f0, vec3( f90 ), pow( 1.0 - saturate( dot( L, H )), 5.0 ));

	return fresnel * spec * 0.5;
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

float filterRoughness(vec3 n)
{
#ifdef GLSL_SHADER_FRAGMENT
/*
    vec2 hpp   = h.xy;// / h.z;
    vec2 deltaU = dFdx(hpp);
    vec2 deltaV = dFdy(hpp);

    // Compute filtering region
    vec2 boundingRectangle = abs(deltaU) + abs(deltaV);
	vec2 variance = 0.25 * boundingRectangle * boundingRectangle;
	float kernelRoughness2 = min(2.0 * max(variance.x, variance.y) , 0.18);
*/
	vec3 dndu = dFdx(n);
	vec3 dndv = dFdy(n);
	float variance = 0.25 * (dot(dndu , dndu) + dot(dndv , dndv));
	float kernelRoughness2 = min(variance, 0.18);
	//float variance = dot(dndu , dndu) + dot(dndv , dndv);
	//float kernelRoughness2 = min(variance, 1.0);
    return kernelRoughness2;
#else
	return 1.0; // just stub because we can't use dFdx/dFdy functions in vertex shaders
#endif
}

/*
float a = glossmap.r * glossmap.r (roughness)
float m = glossmap.g (metalness)
vec3 F0 = mix(vec3(0.03), albedo.rgb, m);
float NV = abs(dot(N,V));
*/
LightingData ComputeLightingBRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 lightColor, vec4 materialInfo, out vec3 diffuseMultiscatter)
{
	LightingData output;
	float roughness = SmoothnessToRoughness(materialInfo.r);
	float metalness = materialInfo.g;
	float ambientOcclusion = materialInfo.b;
	float ambient_factor = min(length(L), 1.0);	//range 0.5 - 1.0, 1.0 means completely directional
	float r_modifier = ambient_factor * 2.0 - 1.0;
	vec3 F0 = mix(vec3(0.03), albedo, metalness);

	L = normalize(L);
	vec3 H = normalize(L + V);   
	float LV = dot(L, V);
	float NH = dot(N, H);
	float NL = dot(N, L);
	float NV = abs(dot(N, V));

	float a = clamp(roughness + filterRoughness(N), 0.0, 1.0);
	LV = max(LV, 0.000);
	NL = clamp(NL, 0.0, 1.0);
	NH = max(abs(NH), 0.1);
  
	// diffuse
	#if defined( TITANFALL_BRDF )
		float facing = 0.5 + 0.5 * LV;
		float rough = facing * (0.9 - 0.4 * facing) * (0.5 + NH) / NH;
		float smoothed = 1.05 * (1.0 - pow(1.0 - NL , 5.0)) * (1.0 - pow(1.0 - NV , 5.0));
		float single = mix(smoothed, rough, a);
		float multi = 0.1159 * a;
	#else
		float single = 1.0;
		float multi = 0.0;
	#endif
		
	// specular
	float a2 = a * a;
	a2 = max(a2 , 0.001);
	float d = ((NH * NH) * (a2 - 1.0) + 1.0);  
	float ggx = a2 / ( (d * d));
	ggx /= 2.0 * mix(2.0 * NL * NV, NL + NV, a);	  
  
	vec3 result = vec3(single, multi, ggx) * NL;	
	result = max(result, vec3(0.0));
	
	// fresnel?
	float HV = clamp(dot(H,V), 0.0 , 1.0);
	vec3 F = F0 + (vec3(1.0) - F0) * pow(1.0 - HV , 5.0);
	vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);
    
	#if defined( TITANFALL_BRDF )
		diffuseMultiscatter = result.y * kD * lightColor * 3.14159265;
	#endif
	output.diffuse = result.x * kD * lightColor; 
	output.specular = result.z * F * lightColor * 3.14159265;
	return output;
}

LightingData ComputeLighting(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 lightColor, vec4 materialInfo, float smoothness)
{
	float gloss = materialInfo.r;
	float metalness = materialInfo.g;
	float ambientOcclusion = materialInfo.b;
	LightingData output;

#if defined( APPLY_PBS )
	vec3 diffuseMultiscatter;
	output = ComputeLightingBRDF(N, V, L, albedo, lightColor, materialInfo, diffuseMultiscatter);
	output.diffuse = albedo * (output.diffuse + albedo * diffuseMultiscatter);
#else
	float NdotL = saturate( dot( N, L ));
	float specular = pow(max(dot(N, normalize(V + L)), 0.0), smoothness * smoothness * 256.0);
	output.diffuse = lightColor * NdotL * albedo;
	output.specular = lightColor * NdotL * gloss * specular;
#endif
	return output;
}

#endif//LIGHTMODEL_H
