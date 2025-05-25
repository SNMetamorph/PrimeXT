/*
scene_bmodel_fp.glsl - fragment uber shader for brush surfaces with static lighting
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
#include "terrain.h"
#include "cubemap.h"
#include "screen.h"
#include "fresnel.h"
#include "parallax.h"
#include "material.h"
#include "fog.h"
#include "alpha2coverage.h"

// texture units
#if defined( APPLY_TERRAIN )
uniform sampler2DArray	u_ColorMap;
uniform sampler2DArray	u_NormalMap;
uniform sampler2DArray	u_GlossMap;
#else
uniform sampler2D		u_ColorMap;
uniform sampler2D		u_NormalMap;
uniform sampler2D		u_GlossMap;
#endif

uniform sampler2D		u_GlowMap;
uniform sampler2D		u_DetailMap;
uniform sampler2D		u_DepthMap;

// uniforms
uniform vec3		u_ViewOrigin;
uniform float		u_ReflectScale;
uniform vec4		u_RenderColor;
uniform vec4		u_FogParams;
uniform float		u_RealTime;
uniform float		u_zFar;

#if defined( APPLY_TERRAIN )
uniform float		u_Smoothness[TERRAIN_NUM_LAYERS];
#else
uniform float		u_Smoothness;
#endif

// shared variables
centroid varying vec2		var_TexDiffuse;
centroid varying vec3		var_TexLight0;
centroid varying vec3		var_TexLight1;
centroid varying vec3		var_TexLight2;
centroid varying vec3		var_TexLight3;
centroid varying vec2		var_TexDetail;
centroid varying vec2		var_TexGlobal;
centroid varying vec3		var_ViewDir;
centroid varying vec3		var_Position;

#if defined( PLANAR_REFLECTION ) || defined( PORTAL_SURFACE )
varying vec4		var_TexMirror;	// mirror coords
#endif

#if defined( REFLECTION_CUBEMAP )
centroid varying mat3		var_MatrixTBN;
#endif

#if !defined( HAS_NORMALMAP )
varying vec3	var_Normal;
#endif

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
centroid varying vec3	var_TangentViewDir;
#endif

void main( void )
{
	vec4 albedo = vec4( 0.0 );
	vec4 result = vec4( 0.0 );
	vec2 vec_TexDiffuse = var_TexDiffuse;
	MaterialData mat;
	LightingData lighting;

#if defined( APPLY_TERRAIN )
	vec4 mask0, mask1, mask2, mask3;
	TerrainReadMask( var_TexGlobal, mask0, mask1, mask2, mask3 );
#endif

#if defined( PARALLAX_SIMPLE )
	//vec_TexDiffuse = ParallaxMapSimple(var_TexDiffuse.xy, normalize(var_TangentViewDir));
	vec_TexDiffuse = ParallaxOffsetMap(u_NormalMap, var_TexDiffuse.xy, normalize(var_TangentViewDir));
	//vec_TexDiffuse = ParallaxReliefMap(var_TexDiffuse.xy, normalize(var_TangentViewDir));
#elif defined( PARALLAX_OCCLUSION )
	vec_TexDiffuse = ParallaxOcclusionMap(var_TexDiffuse.xy, normalize(var_TangentViewDir)).xy; 
#endif
	
// compute the normal first
#if defined( HAS_NORMALMAP )
#if defined( APPLY_TERRAIN )
	vec3 N = TerrainMixNormal( u_NormalMap, vec_TexDiffuse, mask0, mask1, mask2, mask3 );
#else
	vec3 N = normalmap2D( u_NormalMap, vec_TexDiffuse );
#endif // APPLY_TERRAIN
#else
	vec3 N = normalize( var_Normal );
#endif // HAS_NORMALMAP

// setup material params values
#if defined( HAS_GLOSSMAP )
#if defined( APPLY_TERRAIN )
	vec4 glossmap = TerrainMixGlossmap( u_GlossMap, vec_TexDiffuse, mask0, mask1, mask2, mask3 );
#else
	vec4 glossmap = colormap2D( u_GlossMap, vec_TexDiffuse );
#endif
	// get params from texture
	mat = MaterialFetchTexture( glossmap );
#else // !HAS_GLOSSMAP
#if defined( APPLY_TERRAIN )
	mat.smoothness = TerrainMixSmoothness( u_Smoothness, mask0, mask1, mask2, mask3 );
#else
	mat.smoothness = u_Smoothness;
#endif
	// default params 
	mat.metalness = 0.0;
	mat.ambientOcclusion = 1.0;
	mat.specularIntensity = 1.0;
#endif // HAS_GLOSSMAP

#if defined( LIQUID_SURFACE )
	float waterBorderFactor = 1.0, waterRefractFactor = 1.0;
	float fSampledDepth = texture( u_DepthMap, gl_FragCoord.xy * u_ScreenSizeInv ).r;
	fSampledDepth = linearizeDepth( u_zFar, fSampledDepth );
	fSampledDepth = RemapVal( fSampledDepth, Z_NEAR, u_zFar, 0.0, 1.0 );

	float fOwnDepth = gl_FragCoord.z;
	fOwnDepth = linearizeDepth( u_zFar, fOwnDepth );
	fOwnDepth = RemapVal( fOwnDepth, Z_NEAR, u_zFar, 0.0, 1.0 );

#if defined (LIQUID_UNDERWATER)
	waterBorderFactor = waterRefractFactor = u_RenderColor.a;
#else
	float depthDelta = fOwnDepth - fSampledDepth;
	waterBorderFactor = 1.0 - saturate(exp2( -768.0 * 100.0 * depthDelta ));
	waterRefractFactor = 1.0 - saturate(exp2( -768.0 * 5.0 * depthDelta ));
#endif // LIQUID_UNDERWATER
#endif // LIQUID_SURFACE

// compute the result term
#if defined( PLANAR_REFLECTION ) || defined( PORTAL_SURFACE )
	albedo = reflectmap2D( u_ColorMap, var_TexMirror, N, gl_FragCoord.xyz, u_RefractScale );
#elif defined( APPLY_TERRAIN )
	albedo = TerrainMixDiffuse( u_ColorMap, vec_TexDiffuse, mask0, mask1, mask2, mask3 );
#elif defined( MONITOR_BRUSH )
	// in case of rendering monitor we don't need SRGB->linear conversion
	albedo = texture( u_ColorMap, vec_TexDiffuse );
#else
	albedo = colormap2D( u_ColorMap, vec_TexDiffuse );
#endif

#if defined( MONOCHROME )
	albedo.rgb = vec3(GetLuminance(albedo.rgb));
#endif

#if !defined( ALPHA_BLENDING ) && !defined( USING_SCREENCOPY ) && !defined( APPLY_TERRAIN )
	albedo.a = AlphaRescaling( u_ColorMap, vec_TexDiffuse, albedo.a );
#endif
#if defined( ALPHA_TO_COVERAGE )
	// TODO should be cutoff value hardcoded?
	albedo.a = AlphaSharpening( albedo.a, 0.95 );
#endif
	result = albedo;

#if defined( HAS_DETAIL )
#if defined( APPLY_TERRAIN )
	result.rgb *= detailmap2D( u_DetailMap, var_TexGlobal ).rgb * DETAIL_SCALE;
#else
	result.rgb *= detailmap2D( u_DetailMap, var_TexDetail ).rgb * DETAIL_SCALE;
#endif
#endif

#if !defined( LIQUID_SURFACE )
	result.rgb *= u_RenderColor.rgb;
#endif

#if defined( ALPHA_BLENDING ) || defined( USING_SCREENCOPY )
	result.a *= u_RenderColor.a;
#endif

	vec3 V = normalize( var_ViewDir );

	// lighting the world polys
#if !defined( LIGHTING_FULLBRIGHT )
	lighting.diffuse = vec3(0.0);
	lighting.specular = vec3(0.0);
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
	// convert values back to normal range and clamp it
	lighting.diffuse = min(( lighting.diffuse * LIGHTMAP_SHIFT ), 1.0 );
	lighting.specular = min(( lighting.specular * LIGHTMAP_SHIFT ), 1.0 );
#endif

#if defined( LIGHTVEC_DEBUG )
	lighting.diffuse = normalize(( lighting.diffuse + 1.0 ) * 0.5 ); // convert range to [0..1]
#else
	// apply diffuse & specular lighting
#if defined( APPLY_PBS )
	result.rgb = (lighting.kD * result.rgb / M_PI) * lighting.diffuse;
#else
	result.rgb *= lighting.diffuse;
#endif
	result.rgb += lighting.specular;
#endif

#endif // !LIGHTING_FULLBRIGHT

#if defined( HAS_LUMA )
	result.rgb += texture( u_GlowMap, vec_TexDiffuse ).rgb;
#endif

#if defined( REFLECTION_CUBEMAP )
	mat3 tbnBasis = mat3(normalize(var_MatrixTBN[0]), normalize(var_MatrixTBN[1]), normalize(var_MatrixTBN[2]));
	vec3 worldNormal = normalize(tbnBasis * N);
	vec3 reflected = CubemapReflectionProbe( var_Position, u_ViewOrigin, worldNormal, mat.smoothness );
#endif // REFLECTION_CUBEMAP

#if defined( USING_SCREENCOPY )
#if defined( LIQUID_SURFACE )
	float distortScale = waterRefractFactor;
#else 
	float distortScale = 1.0;
#endif
	// prohibits displaying in refractions objects, that are closer to camera than brush surface
	float distortedDepth = texture(u_DepthMap, GetDistortedTexCoords(N, distortScale)).r;
	distortScale *= step(gl_FragCoord.z, distortedDepth);

	// fetch color for saved screencopy
	vec3 screenmap = GetScreenColor( N, distortScale );
#if defined( PLANAR_REFLECTION ) || defined( PORTAL_SURFACE )
	result.a = GetFresnel( saturate(dot(V, N)), WATER_F0_VALUE, FRESNEL_FACTOR );
#endif // PLANAR_REFLECTION

#if defined( LIQUID_SURFACE )
	vec3 waterColor = u_RenderColor.rgb;
	vec3 refracted = mix( screenmap, screenmap * waterColor, waterBorderFactor ); // smooth transition between water and ground
#if defined( REFLECTION_CUBEMAP )
	// blend refracted and reflected part together 
	float fresnel = GetFresnel( V, N, WATER_F0_VALUE, FRESNEL_FACTOR );
	result.rgb = refracted + reflected * fresnel * waterBorderFactor * u_ReflectScale; 
#else
	result.rgb = refracted;
#endif // REFLECTION_CUBEMAP

#else // !LIQUID_SURFACE
	// for translucent non-liquid stuff (glass, etc.)
	result.rgb = mix( screenmap, result.rgb, result.a );
#endif // LIQUID_SURFACE
#else // !USING_SCREENCOPY
#if defined( REFLECTION_CUBEMAP )
	float fresnel = GetFresnel( V, N, GENERIC_F0_VALUE, FRESNEL_FACTOR );
	result.rgb += reflected * fresnel * u_ReflectScale * mat.smoothness;
#endif
#endif // USING_SCREENCOPY

#if defined( APPLY_FOG_EXP )
	result.rgb = CalculateFog(result.rgb, u_FogParams, gl_FragCoord.z / gl_FragCoord.w);
#endif

#if defined( LIGHTMAP_DEBUG ) || defined( LIGHTVEC_DEBUG )
	result.rgb = lighting.diffuse;
#endif

	// compute final color
	gl_FragColor = result;
}
