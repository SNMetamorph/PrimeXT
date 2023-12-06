/*
light_studio_fp.glsl - studio forward lighting
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
#include "material.h"
#include "parallax.h"

uniform sampler2D		u_ProjectMap;
uniform sampler2D		u_ColorMap;
uniform sampler2D		u_DetailMap;
uniform sampler2D		u_NormalMap;
uniform sampler2D		u_GlossMap;

#if defined( APPLY_SHADOW )
#if defined( LIGHT_SPOT )
#include "shadow_spot.h"
#elif defined( LIGHT_OMNI )
#include "shadow_omni.h"
#elif defined( LIGHT_PROJ )
#include "shadow_proj.h"
#endif
#endif

uniform vec4		u_LightDir;
uniform vec3		u_LightDiffuse;
uniform vec4		u_LightOrigin;
uniform vec4		u_ShadowParams;
uniform float		u_Smoothness;
uniform float		u_RefractScale;
uniform float		u_AmbientFactor;
uniform float		u_SunRefract;

varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;
varying vec3		var_ViewVec;

#if defined( HAS_DETAIL )
varying vec2		var_TexDetail;
#endif

#if defined( HAS_NORMALMAP )
varying mat3		var_WorldMat;
#else
varying vec3		var_Normal;
#endif

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
varying vec3		var_TangentViewDir;
#endif

#if defined( LIGHT_SPOT )
varying vec4		var_ProjCoord;
#endif

#if defined( LIGHT_SPOT ) || defined( LIGHT_PROJ )
varying vec4		var_ShadowCoord;
#endif

void main( void )
{
	vec3 V = normalize( var_ViewVec );
	vec3 L = vec3( 0.0 );
	vec2 vec_TexDiffuse;
	float atten = 1.0;
	MaterialData mat;

#if !defined( LIGHT_PROJ )
	atten = LightAttenuation(var_LightVec, u_LightOrigin.w);
	if (atten <= 0.0) 
		discard; // fast reject
#endif

#if defined( PARALLAX_SIMPLE )
	//vec_TexDiffuse = ParallaxMapSimple(var_TexDiffuse, normalize(var_TangentViewDir));
	vec_TexDiffuse = ParallaxOffsetMap(u_NormalMap, var_TexDiffuse, normalize(var_TangentViewDir));
	//vec_TexDiffuse = ParallaxReliefMap(var_TexDiffuse, normalize(var_TangentViewDir));
#elif defined( PARALLAX_OCCLUSION )
	vec_TexDiffuse = ParallaxOcclusionMap(var_TexDiffuse, normalize(var_TangentViewDir)).xy; 
#else
	vec_TexDiffuse = var_TexDiffuse;
#endif

#if defined( LIGHT_SPOT )
	L = normalize( var_LightVec );
	
	// spot attenuation
	float spotDot = dot( normalize( u_LightDir.xyz ), L );
	float fov = ( u_LightDir.w * FOV_MULT * ( M_PI / 180.0 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos ) 
		discard;

#elif defined( LIGHT_OMNI )
	L = normalize( var_LightVec );
#elif defined( LIGHT_PROJ )
	L = normalize( u_LightDir.xyz );
#endif
#if defined( HAS_NORMALMAP )
	vec3 N = normalmap2D( u_NormalMap, vec_TexDiffuse );
	N = normalize( var_WorldMat * N ); // rotate normal to worldspace
#else
	vec3 N = normalize( var_Normal );
#endif
	// two-sided textures support
	if( bool( gl_FrontFacing )) N = -N;

	// compute the diffuse term
	vec4 diffuse = colormap2D( u_ColorMap, vec_TexDiffuse );

#if defined( HAS_DETAIL )
	diffuse.rgb *= detailmap2D( u_DetailMap, var_TexDetail ).rgb * DETAIL_SCALE;
#endif

// setup material params values
#if defined( HAS_GLOSSMAP )
	// get params from texture
	mat = MaterialFetchTexture(colormap2D( u_GlossMap, vec_TexDiffuse ));
#else // !HAS_GLOSSMAP
	// use default parameter values
	mat.smoothness = u_Smoothness;
	mat.metalness = 0.0;
	mat.ambientOcclusion = 1.0;
	mat.specularIntensity = 1.0;
#endif // HAS_GLOSSMAP

	vec3 light = u_LightDiffuse * DLIGHT_SCALE;	// light color
	float shadow = 1.0;
	float NdotL = saturate( dot( N, L ));
	if ( NdotL <= 0.0 ) 
		discard; // fast reject

#if defined( LIGHT_SPOT )
	// texture or procedural spotlight
	light *= textureProj( u_ProjectMap, var_ProjCoord ).rgb;
#if defined( APPLY_SHADOW )
	if( NdotL > 0.0 ) 
		shadow = ShadowSpot( var_ShadowCoord, u_ShadowParams.xy );
#endif
#elif defined( LIGHT_OMNI )
#if defined( APPLY_SHADOW )
	if( NdotL > 0.0 ) 
		shadow = ShadowOmni( -var_LightVec, u_ShadowParams );
#endif
#elif defined( LIGHT_PROJ )
#if defined( APPLY_SHADOW )
	if( NdotL > 0.0 ) {
		shadow = ShadowProj( var_ShadowCoord.xyz );
	}
#endif
#endif
	if (shadow <= 0.0) 
		discard; // fast reject
 
#if defined( LIGHTMAP_DEBUG )
	vec3 albedo = vec3(1.0);
#else 
	vec3 albedo = diffuse.rgb;
#endif
	light *= atten * shadow; // apply attenuation and shadowing
	LightingData lighting = ComputeLighting(N, V, L, albedo, light, mat);
#if defined( APPLY_PBS )
	diffuse.rgb = (lighting.kD * albedo / M_PI) * lighting.diffuse;
#else
	diffuse.rgb = lighting.diffuse * albedo;
#endif

	diffuse.rgb += lighting.specular;
	gl_FragColor = diffuse;
}