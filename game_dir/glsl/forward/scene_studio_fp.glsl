/*
scene_studio_fp.glsl - vertex uber shader for studio meshes with static lighting
Copyright (C) 2019 Uncle Mike

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
#include "screen.h"
#include "material.h"
#include "parallax.h"
#include "fog.h"
#include "alpha2coverage.h"

uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_GlossMap;
uniform sampler2D	u_GlowMap;
uniform sampler2D	u_DetailMap;
uniform sampler2D	u_DepthMap;

uniform vec4	u_RenderColor;
uniform float	u_Smoothness;
uniform float	u_ReflectScale;
uniform vec4	u_FogParams;
uniform vec3	u_ViewOrigin;
uniform vec2	u_LightShade;

varying vec2	var_TexDiffuse;
varying vec2	var_TexDetail;
varying vec3	var_AmbientLight;
varying vec3	var_DiffuseLight;

#if defined (SURFACE_LIGHTING)
varying vec3	var_TexLight0;
varying vec3	var_TexLight1;
varying vec3	var_TexLight2;
varying vec3	var_TexLight3;
#endif

#if defined( REFLECTION_CUBEMAP ) || defined( APPLY_PBS )
varying vec3	var_WorldNormal;
#endif

#if defined( REFLECTION_CUBEMAP ) || defined( APPLY_PBS ) || defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
varying mat3	var_MatrixTBN;
#endif

varying vec3	var_LightDir;
varying vec3	var_ViewDir;
varying vec3	var_Normal;
varying vec3	var_Position;

void main( void )
{
	vec2 vec_TexDiffuse;
	vec4 result = vec4( 0.0 );
	vec4 albedo = vec4( 0.0 );
	MaterialData mat;
	LightingData lighting;

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION ) ||defined( REFLECTION_CUBEMAP ) || defined( APPLY_PBS )
	mat3 tbnBasis = mat3(normalize(var_MatrixTBN[0]), normalize(var_MatrixTBN[1]), normalize(var_MatrixTBN[2]));
#endif

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
	vec3 tangentViewDir = normalize(u_ViewOrigin - var_Position) * tbnBasis;
#endif

#if defined( PARALLAX_SIMPLE )
	//vec_TexDiffuse = ParallaxMapSimple(var_TexDiffuse, tangentViewDir);
	vec_TexDiffuse = ParallaxOffsetMap(u_NormalMap, var_TexDiffuse, tangentViewDir);
	//vec_TexDiffuse = ParallaxReliefMap(var_TexDiffuse, tangentViewDir);
#elif defined( PARALLAX_OCCLUSION )
	vec_TexDiffuse = ParallaxOcclusionMap(var_TexDiffuse, tangentViewDir).xy; 
#else
	vec_TexDiffuse = var_TexDiffuse;
#endif
	albedo = colormap2D( u_ColorMap, vec_TexDiffuse );
#if !defined( ALPHA_BLENDING )
	albedo.a = AlphaRescaling( u_ColorMap, vec_TexDiffuse, albedo.a );
#endif
#if defined( ALPHA_TO_COVERAGE )
	// TODO should be cutoff value hardcoded?
	albedo.a = AlphaSharpening( albedo.a, 0.5 );
#endif
	result = albedo;

#if defined( HAS_DETAIL )
	result.rgb *= detailmap2D( u_DetailMap, var_TexDetail ).rgb * DETAIL_SCALE;
#endif
	// kRenderTransColor support
	result.rgb *= u_RenderColor.rgb;

#if !defined( ALPHA_BLENDING )
	result.a *= u_RenderColor.a;
#endif

#if defined( HAS_NORMALMAP )
	vec3 N = normalmap2D( u_NormalMap, vec_TexDiffuse );
#else
	vec3 N = normalize( var_Normal );
#endif
	// two side materials support
	if( bool( gl_FrontFacing )) N = -N;

#if !defined( LIGHTING_FULLBRIGHT )
	vec3 L = normalize( var_LightDir );
	vec3 V = normalize( var_ViewDir );

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

#if defined( REFLECTION_CUBEMAP ) || defined( APPLY_PBS )
#if defined( HAS_NORMALMAP )
	// TBN generates only for models with normal maps (just optimization)
	// because of that we can use TBN matrix only for this case
	vec3 worldNormal = normalize(tbnBasis * N);
#else
	vec3 worldNormal = N;
#endif
#endif

#if defined( SURFACE_LIGHTING ) // lightmap lighting
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
	// convert values back to normal range and clamp it (should be disabled when using HDR lightmaps)
	lighting.diffuse = min(( lighting.diffuse * LIGHTMAP_SHIFT ), 1.0 );
	lighting.specular = min(( lighting.specular * LIGHTMAP_SHIFT ), 1.0 );
#endif
#else // !SURFACE_LIGHTING
#if defined( LIGHTING_FALLBACK ) || !defined( APPLY_PBS )
#if defined( VERTEX_LIGHTING ) 
// vertex lighting
lighting = ComputeLighting( N, V, L, albedo.rgb, var_DiffuseLight, mat );
lighting.diffuse += var_AmbientLight;
///
#else 
// virtual light source
lighting = ComputeLighting( N, V, L, albedo.rgb, var_DiffuseLight, mat );
lighting.specular *= u_LightShade.y;
lighting.diffuse *= u_LightShade.y;
lighting.diffuse += var_AmbientLight;
///
#endif
#else // !LIGHTING_FALLBACK
// sampling specular IBL and light probes
	vec3 prefilteredColor = CubemapSpecularIBLProbe( var_Position, u_ViewOrigin, worldNormal, mat.smoothness );
	lighting.specular = ComputeSpecularIBL( N, V, albedo.rgb, prefilteredColor, mat ) * mat.ambientOcclusion;
	lighting.diffuse = ComputeIndirectDiffuse( N, V, albedo.rgb, var_AmbientLight, mat ) * mat.ambientOcclusion;
///
#endif

#endif // SURFACE_LIGHTING

#if defined( APPLY_PBS )
	result.rgb = (lighting.kD * result.rgb / M_PI) * lighting.diffuse;
#else
	result.rgb *= lighting.diffuse; // apply diffuse/ambient lighting
#endif
	result.rgb += lighting.specular; // apply specular lighting

#if defined( LIGHTVEC_DEBUG ) && !defined (SURFACE_LIGHTING)
	lighting.diffuse = ( normalize( N + L ) + 1.0 ) * 0.5;
#endif // LIGHTVEC_DEBUG

#if defined( REFLECTION_CUBEMAP )
	vec3 reflected = CubemapReflectionProbe( var_Position, u_ViewOrigin, worldNormal, mat.smoothness );
	float fresnel = GetFresnel( V, N, WATER_F0_VALUE, FRESNEL_FACTOR );
	result.rgb += reflected * mat.smoothness * u_ReflectScale;
#endif // REFLECTION_CUBEMAP

#if defined( LIGHTMAP_DEBUG ) || defined( LIGHTVEC_DEBUG )
	result.rgb = lighting.diffuse;
#endif
#endif // LIGHTING_FULLBRIGHT

#if defined( HAS_LUMA )
	result.rgb += texture( u_GlowMap, vec_TexDiffuse ).rgb;
#endif

#if defined( USING_SCREENCOPY )
	// prohibits displaying in refractions objects, that are closer to camera than this studiomodel
	float distortedDepth = texture(u_DepthMap, GetDistortedTexCoords(N, 1.0)).r;
	float distortScale = step(gl_FragCoord.z, distortedDepth);
	result.rgb = mix(GetScreenColor(N, distortScale), result.rgb, result.a);
#endif

#if defined( APPLY_FOG_EXP )
	result.rgb = CalculateFog(result.rgb, u_FogParams, gl_FragCoord.z / gl_FragCoord.w);
#endif
	// compute final color
	gl_FragColor = result;
}
