/*
decal_bmodel_fp.glsl - fragment uber shader for bmodel decals
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "const.h"
#include "mathlib.h"
#include "texfetch.h"
#include "lightmodel.h"
#include "deluxemap.h"	// directional lightmap routine
#include "cubemap.h"
#include "fresnel.h"
#include "parallax.h"
#include "material.h"
#include "fog.h"

uniform sampler2D	u_DecalMap;
uniform sampler2D	u_ColorMap;	// surface under decal
uniform sampler2D	u_GlossMap;
uniform sampler2D	u_NormalMap;	// refraction

uniform float	u_Smoothness;
uniform float	u_RefractScale;
uniform float	u_ReflectScale;
uniform vec4	u_FogParams;
uniform vec3	u_ViewOrigin;
uniform float	u_RealTime;
uniform vec3	u_LightDir;	// already in tangent space

varying vec4	var_TexDiffuse;
varying vec3	var_TexLight0;
varying vec3	var_TexLight1;
varying vec3	var_TexLight2;
varying vec3	var_TexLight3;
varying vec3	var_ViewDir;
varying vec3	var_Normal;
varying vec3	var_Position;

#if defined( PLANAR_REFLECTION )
varying vec4	var_TexMirror;	// mirror coords
#endif

#if defined( REFLECTION_CUBEMAP )
varying mat3	var_MatrixTBN;
#endif

void main( void )
{
	MaterialData mat;
	LightingData lighting;
	lighting.diffuse = vec3(0.0);
	lighting.specular = vec3(0.0);
	
#if defined( ALPHA_TEST )
	if( colormap2D( u_ColorMap, var_TexDiffuse.zw ).a <= BMODEL_ALPHA_THRESHOLD )
		discard;
#endif
	vec3 V = normalize( var_ViewDir );
	vec2 vecTexCoord = var_TexDiffuse.xy;
#if defined( PARALLAX_SIMPLE )
	//vec_TexDiffuse = ParallaxMapSimple(var_TexDiffuse.xy, V);
	vecTexCoord = ParallaxOffsetMap(u_NormalMap, var_TexDiffuse.xy, V);
	//vec_TexDiffuse = ParallaxReliefMap(var_TexDiffuse.xy, V);
#elif defined( PARALLAX_OCCLUSION )
	vecTexCoord = ParallaxOcclusionMap(var_TexDiffuse.xy, V).xy; 
#endif
	vec4 albedo = decalmap2D( u_DecalMap, vecTexCoord );
	vec4 result = albedo;

#if defined( APPLY_COLORBLEND )
	float alpha = (( result.r * 2.0 ) + ( result.g * 2.0 ) + ( result.b * 2.0 )) / 3.0;
#endif

#if defined( DECAL_PUDDLE )
	result = vec4( 0.19, 0.16, 0.11, 1.0 ); // replace with puddle color
#endif

#if defined( HAS_NORMALMAP )
#if defined( DECAL_PUDDLE )
	vec2 tc_Normal0 = vecTexCoord * 0.1;
	tc_Normal0.s += u_RealTime * 0.005;
	tc_Normal0.t -= u_RealTime * 0.005;

	vec2 tc_Normal1 = vecTexCoord * 0.1;
	tc_Normal1.s -= u_RealTime * 0.005;
	tc_Normal1.t += u_RealTime * 0.005;

	vec3 N0 = normalmap2D( u_NormalMap, tc_Normal0 );
	vec3 N1 = normalmap2D( u_NormalMap, tc_Normal1 );
	vec3 N = normalize( vec3( ( N0.xy / N0.z ) + ( N1.xy / N1.z ), 1.0 ));
#else
	vec3 N = normalmap2D( u_NormalMap, vecTexCoord );
#endif
#else
	vec3 N = normalize( var_Normal );
#endif

// setup material params values
#if defined( HAS_GLOSSMAP )
	// get params from texture
	mat = MaterialFetchTexture(colormap2D( u_GlossMap, vecTexCoord ));
#else // !HAS_GLOSSMAP
	// use default parameter values
	mat.smoothness = u_Smoothness;
	mat.metalness = 0.0;
	mat.ambientOcclusion = 1.0;
	mat.specularIntensity = 1.0;
#endif // HAS_GLOSSMAP

#if defined( APPLY_STYLE0 )
	ApplyLightStyle( var_TexLight0, N, V, albedo, mat, lighting );
#endif
#if defined( APPLY_STYLE1 )
	ApplyLightStyle( var_TexLight1, N, V, albedo, mat, lighting );
#endif
#if defined( APPLY_STYLE2 )
	ApplyLightStyle( var_TexLight2, N, V, albedo, mat, lighting );
#endif
#if defined( APPLY_STYLE3 )
	ApplyLightStyle( var_TexLight3, N, V, albedo, mat, lighting );
#endif
#if defined( APPLY_STYLE0 ) || defined( APPLY_STYLE1 ) || defined( APPLY_STYLE2 ) || defined( APPLY_STYLE3 )
	// convert values back to normal range and clamp it (should be only for LDR-lightmaps)
	lighting.diffuse = min(( lighting.diffuse * LIGHTMAP_SHIFT ), 1.0 );
	lighting.specular = min(( lighting.specular * LIGHTMAP_SHIFT ), 1.0 );
#endif

#if !defined( APPLY_COLORBLEND )
#if defined( APPLY_PBS )
	result.rgb = (lighting.kD * result.rgb / M_PI) * lighting.diffuse;
#else
	result.rgb *= lighting.diffuse; // apply diffuse lighting
#endif
#endif // APPLY_COLORBLEND
	result.rgb += lighting.specular; // apply specular lighting

#if defined( PLANAR_REFLECTION ) || defined( REFLECTION_CUBEMAP )
#if defined( REFLECTION_CUBEMAP )
	vec3 waveNormal = N * 0.02 * u_RefractScale; // puddles are always placed on floor so we skip rotation from tangentspace to worldspace
	vec3 mirror = CubemapReflectionProbe( var_Position, u_ViewOrigin, PUDDLE_NORMAL + waveNormal, 1.0 );
#elif defined( PLANAR_REFLECTION )
	vec3 mirror = reflectmap2D( u_ColorMap, var_TexMirror, N, gl_FragCoord.xyz, u_RefractScale ).rgb;
#endif
	float eta = GetFresnel( V, PUDDLE_NORMAL, WATER_F0_VALUE, FRESNEL_FACTOR ) * u_ReflectScale;
	result.rgb = mix( result.rgb, mirror.rgb, eta );
#if defined( APPLY_COLORBLEND )
	result.rgb = mix( result.rgb, vec3( 0.5 ), alpha );
#endif
#endif

#if defined( APPLY_FOG_EXP )
#if defined( APPLY_COLORBLEND )
	result.rgb = CalculateFog(result.rgb, u_FogParams, length(u_ViewOrigin - var_Position));
#else
	float fogFactor = saturate(exp2(-u_FogParams.w * length(u_ViewOrigin - var_Position)));
	result.a *= fogFactor; // modulate alpha
#endif
#endif
	gl_FragColor = result;
}
