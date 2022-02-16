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
varying vec2		var_TexDiffuse;
varying vec3		var_TexLight0;
varying vec3		var_TexLight1;
varying vec3		var_TexLight2;
varying vec3		var_TexLight3;
varying vec2		var_TexDetail;
varying vec2		var_TexGlobal;
varying vec3		var_ViewDir;

#if defined( PLANAR_REFLECTION )
varying vec4		var_TexMirror;	// mirror coords
#endif

#if defined( REFLECTION_CUBEMAP )
varying vec3		var_WorldNormal;
varying vec3		var_Position;
varying mat3		var_MatrixTBN;
#endif

#if !defined( HAS_NORMALMAP )
varying vec3	var_Normal;
#endif

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
varying vec3	var_TangentViewDir;
#endif

void main( void )
{
	vec3 light = vec3( 0.0 );
	vec3 gloss = vec3( 0.0 );
	vec2 vec_TexDiffuse = var_TexDiffuse;

#if defined( APPLY_TERRAIN )
	vec4 mask0, mask1, mask2, mask3;
	TerrainReadMask( var_TexGlobal, mask0, mask1, mask2, mask3 );
#endif

#if defined( PARALLAX_SIMPLE )
	//vec_TexDiffuse = ParallaxMapSimple(var_TexDiffuse.xy, var_TangentViewDir);
	vec_TexDiffuse = ParallaxOffsetMap(u_NormalMap, var_TexDiffuse.xy, var_TangentViewDir);
	//vec_TexDiffuse = ParallaxReliefMap(var_TexDiffuse.xy, var_TangentViewDir);
#elif defined( PARALLAX_OCCLUSION )
	vec_TexDiffuse = ParallaxOcclusionMap(var_TexDiffuse.xy, var_TangentViewDir).xy; 
#endif
	
// compute the normal first
#if defined( HAS_NORMALMAP )
#if defined( APPLY_TERRAIN )
	vec3 N = TerrainApplyNormal( u_NormalMap, vec_TexDiffuse, mask0, mask1, mask2, mask3 );
#else
#if defined( LIQUID_SURFACE )
	vec3 N = normalmap2D( u_NormalMap, var_TexGlobal );
#else
	vec3 N = normalmap2D( u_NormalMap, vec_TexDiffuse );
#endif // LIQUID_SURFACE
#endif // APPLY_TERRAIN
#else
	vec3 N = normalize( var_Normal );
#endif // HAS_NORMALMAP

	float waterBorderFactor = 1.0, waterAbsorbFactor = 1.0, waterRefractFactor = 1.0;

#if defined( LIQUID_SURFACE )
	vec_TexDiffuse.x += N.x * 4.0 * u_RefractScale;
	vec_TexDiffuse.y += N.y * 4.0 * u_RefractScale;
	float fOwnDepth = gl_FragCoord.z;
	fOwnDepth = linearizeDepth( u_zFar, fOwnDepth );
	fOwnDepth = RemapVal( fOwnDepth, Z_NEAR, u_zFar, 0.0, 1.0 );

	float fSampledDepth = texture2D( u_DepthMap, gl_FragCoord.xy * u_ScreenSizeInv ).r;
	fSampledDepth = linearizeDepth( u_zFar, fSampledDepth );
	fSampledDepth = RemapVal( fSampledDepth, Z_NEAR, u_zFar, 0.0, 1.0 );

#if defined (LIQUID_UNDERWATER)
	waterBorderFactor = waterAbsorbFactor = waterRefractFactor = u_RenderColor.a;
#else
	float depthDelta = fOwnDepth - fSampledDepth;
	waterBorderFactor = 1.0 - saturate(exp2( -768.0 * depthDelta ));
	waterRefractFactor = saturate( depthDelta * 32.0 );
	waterAbsorbFactor = 1.0 - saturate(exp2( -768.0 * u_RenderColor.a * depthDelta ));
#endif
#endif

// compute the diffuse term
#if defined( PLANAR_REFLECTION )
	vec4 diffuse = reflectmap2D( u_ColorMap, var_TexMirror, N, gl_FragCoord.xyz, u_RefractScale );
#elif defined( APPLY_TERRAIN )
	vec4 diffuse = TerrainApplyDiffuse( u_ColorMap, vec_TexDiffuse, mask0, mask1, mask2, mask3 );
#else
	vec4 diffuse = colormap2D( u_ColorMap, vec_TexDiffuse );
#endif

#if defined( HAS_DETAIL )
#if defined( APPLY_TERRAIN )
	diffuse.rgb *= detailmap2D( u_DetailMap, var_TexGlobal ).rgb * DETAIL_SCALE;
#else
	diffuse.rgb *= detailmap2D( u_DetailMap, var_TexDetail ).rgb * DETAIL_SCALE;
#endif
#endif

#if !defined( LIQUID_SURFACE )
	diffuse.rgb *= u_RenderColor.rgb;
#endif

#if defined( SIGNED_DISTANCE_FIELD )
	diffuse.a *= smoothstep( SOFT_EDGE_MIN, SOFT_EDGE_MAX, diffuse.a ); 
#endif

#if !defined( ALPHA_GLASS ) && defined( TRANSLUCENT )
	diffuse.a *= (u_RenderColor.a);
#endif
	vec3 V = normalize( var_ViewDir );

	// lighting the world polys
#if !defined( LIGHTING_FULLBRIGHT )
#if defined( APPLY_TERRAIN )
	float smoothness = TerrainCalcSmoothness( u_Smoothness, mask0, mask1, mask2, mask3 );
#else
	float smoothness = u_Smoothness;
#endif
	vec4 glossmap = vec4( 0.0 );

	// compute the specular term
#if defined( HAS_GLOSSMAP ) && defined( HAS_DELUXEMAP )
#if defined( APPLY_TERRAIN )
	glossmap = TerrainApplySpecular( u_GlossMap, vec_TexDiffuse, mask0, mask1, mask2, mask3 );
#else
#if defined( LIQUID_SURFACE )
	glossmap = colormap2D( u_GlossMap, var_TexGlobal );
#else
	glossmap = colormap2D( u_GlossMap, vec_TexDiffuse );
#endif
#endif
#endif//(HAS_GLOSSMAP && HAS_DELUXEMAP)

#if defined( APPLY_STYLE0 )
	ApplyLightStyle( var_TexLight0, N, V, glossmap.rgb, smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE1 )
	ApplyLightStyle( var_TexLight1, N, V, glossmap.rgb, smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE2 )
	ApplyLightStyle( var_TexLight2, N, V, glossmap.rgb, smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE3 )
	ApplyLightStyle( var_TexLight3, N, V, glossmap.rgb, smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE0 ) || defined( APPLY_STYLE1 ) || defined( APPLY_STYLE2 ) || defined( APPLY_STYLE3 )
	// convert values back to normal range and clamp it
	light = min(( light * LIGHTMAP_SHIFT ), 1.0 );
	gloss = min(( gloss * LIGHTMAP_SHIFT ), 1.0 );
#endif

#if defined( LIGHTVEC_DEBUG )
	light = normalize(( light + 1.0 ) * 0.5 ); // convert range to [0..1]
#else
	diffuse.rgb *= light;

#if defined( HAS_GLOSSMAP )
	diffuse.rgb *= saturate( 1.0 - GetLuminance( gloss ));
	diffuse.rgb += gloss;
#endif
#endif

#if defined( LIGHTMAP_DEBUG ) || defined( LIGHTVEC_DEBUG )
	diffuse.rgb = light;
#endif
#endif // LIGHTING_FULLBRIGHT

#if defined( HAS_LUMA )
	diffuse.rgb += texture2D( u_GlowMap, vec_TexDiffuse ).rgb;
#endif

#if defined( REFLECTION_CUBEMAP )
	mat3 tbnBasis = mat3(normalize(var_MatrixTBN[0]), normalize(var_MatrixTBN[1]), normalize(var_MatrixTBN[2]));
	vec3 worldNormal = normalize(tbnBasis * N);
	vec3 reflected = GetReflectionProbe( var_Position, u_ViewOrigin, worldNormal, smoothness );
	float fresnel = GetFresnel( V, N, WATER_F0_VALUE, FRESNEL_FACTOR );
#endif // REFLECTION_CUBEMAP

#if defined( TRANSLUCENT )
	vec3 screenmap = GetScreenColor( N, waterRefractFactor );
#if defined( PLANAR_REFLECTION )
	diffuse.a = GetFresnel( saturate(dot(V, N)), WATER_F0_VALUE, FRESNEL_FACTOR );
#else
	diffuse.a = 1.0 - u_RenderColor.a;
#endif // PLANAR_REFLECTION

#if defined( LIQUID_SURFACE )
	vec3 waterColor = u_RenderColor.rgb;
	vec3 borderSmooth = mix( screenmap, screenmap * waterColor, waterBorderFactor ); // smooth transition between water and ground
	vec3 refracted = mix( borderSmooth, waterColor * light, waterAbsorbFactor ); // mix between refracted light and own water color
#if defined( REFLECTION_CUBEMAP )
	// blend refracted and reflected part together 
	diffuse.rgb = refracted + reflected * fresnel * waterBorderFactor * u_ReflectScale; 
#else
	diffuse.rgb = refracted;
#endif // REFLECTION_CUBEMAP
#else
	// for translucent non-liquid stuff (glass, etc.)
	diffuse.rgb = mix( screenmap, diffuse.rgb, diffuse.a * u_RenderColor.a );
#endif // LIQUID_SURFACE
#else // !TRANSLUCENT

#if defined( REFLECTION_CUBEMAP )
	diffuse.rgb += reflected * fresnel * u_ReflectScale;
#endif

#endif // TRANSLUCENT

#if defined( APPLY_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
#endif
	// compute final color
	gl_FragColor = diffuse;
}