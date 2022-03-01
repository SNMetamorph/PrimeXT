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
#include "material.h"

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

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

    return nom / max(denom, 0.0001);
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Source: Y. Tokuyoshi, A. Kaplanyan "Improved Geometric Specular Antialiasing"
float SpecularAntialiasing(vec3 n, float roughness)
{
#ifdef GLSL_SHADER_FRAGMENT
	const float sigma2 = 0.5; // default value is 0.25
	const float kappa = 0.18;
	vec3 dndu = dFdx(n);
	vec3 dndv = dFdy(n);
	float variance = sigma2 * (dot(dndu, dndu) + dot(dndv, dndv));
	float kernelRoughness2 = min(variance, kappa);
    return saturate(roughness + kernelRoughness2);
#else
	return roughness;
#endif
}

LightingData ComputeLightingBRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 lightColor, MaterialData materialInfo)
{
	LightingData lighting;
	float roughness = SmoothnessToRoughness(materialInfo.smoothness);
	float metalness = materialInfo.metalness;
	float ambientOcclusion = materialInfo.ambientOcclusion;

	vec3 F0 = mix(vec3(0.02), albedo, metalness);
	L = normalize(L);
	vec3 H = normalize(L + V);   
	roughness = SpecularAntialiasing(N, roughness);
	
	// Cook-Torrance BRDF
	float NDF = DistributionGGX(N, H, roughness);   
	float G   = GeometrySmith(N, V, L, roughness);      
	vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
		
	vec3 numerator    = NDF * G * F; 
	float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
	vec3 specular = numerator / max(denominator, 0.0001);
	
	// kS is equal to Fresnel
	vec3 kS = F;
	// for energy conservation, the diffuse and specular light can't
	// be above 1.0 (unless the surface emits light); to preserve this
	// relationship the diffuse component (kD) should equal 1.0 - kS.
	vec3 kD = vec3(1.0) - kS;
	// multiply kD by the inverse metalness such that only non-metals 
	// have diffuse lighting, or a linear blend if partly metal (pure metals
	// have no diffuse light).
	kD *= 1.0 - metalness;	  

	// scale light by NdotL
	float NdotL = max(dot(N, L), 0.0);        

	lighting.diffuse = (kD * albedo / M_PI) * lightColor * NdotL;
	lighting.specular = kS * specular * lightColor * NdotL;
	return lighting;
}

LightingData ComputeLighting(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 lightColor, MaterialData materialInfo)
{
	LightingData lighting;
	float smoothness = materialInfo.smoothness;
#if defined( APPLY_PBS )
	lighting = ComputeLightingBRDF(N, V, L, albedo, lightColor, materialInfo);
	lighting.diffuse = albedo * lighting.diffuse;
#else
	float NdotL = saturate( dot( N, L ));
	float specular = pow(max(dot(N, normalize(V + L)), 0.0), 32.0);
	lighting.diffuse = lightColor * NdotL * albedo;
	lighting.specular = lightColor * NdotL * smoothness * specular;
#endif
	return lighting;
}

#endif//LIGHTMODEL_H
