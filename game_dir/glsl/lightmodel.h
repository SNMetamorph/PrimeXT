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

uniform sampler2D u_BRDFApproxMap;

struct LightingData
{
	vec3 diffuse;
	vec3 specular;
	vec3 kD;
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
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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

vec3 ComputeSpecularIBL(vec3 N, vec3 V, vec3 albedo, vec3 prefilteredSample, MaterialData mat)
{
	vec3 F0 = mix(vec3(0.02), albedo, mat.metalness);
	vec2 brdf = texture(u_BRDFApproxMap, vec2(max(dot(N, V), 0.0), SmoothnessToRoughness(mat.smoothness))).rg;
	return prefilteredSample * (F0 * brdf.x + brdf.y) * mat.specularIntensity;
}

vec3 ComputeIndirectDiffuse(vec3 N, vec3 V, vec3 albedo, vec3 irradiance, MaterialData mat)
{
	vec3 F0 = mix(vec3(0.02), albedo, mat.metalness);
	vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, SmoothnessToRoughness(mat.smoothness));
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - mat.metalness;	  
    return kD * irradiance;
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
	lighting.kD = kD;
	lighting.diffuse = lightColor * NdotL;
	lighting.specular = specular * lightColor * materialInfo.specularIntensity * NdotL;
	return lighting;
}

LightingData ComputeLighting(vec3 N, vec3 V, vec3 L, vec3 albedo, vec3 lightColor, MaterialData materialInfo)
{
	LightingData lighting;
	float smoothness = materialInfo.smoothness;
#if defined( APPLY_PBS )
	lighting = ComputeLightingBRDF(N, V, L, albedo, lightColor, materialInfo);
#else
	float NdotL = saturate( dot( N, L ));
	float specular = pow(max(dot(N, normalize(V + L)), 0.0), 32.0);
	lighting.kD = vec3(0.0);
	lighting.diffuse = lightColor * NdotL;
	lighting.specular = lightColor * NdotL * smoothness * specular;
#endif
	return lighting;
}

#endif // LIGHTMODEL_H
