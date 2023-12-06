/*
scene_bmodel_fp.glsl - g-buffer rendering brush surfaces
Copyright (C) 2018 Uncle Mike

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
#include "terrain.h"
#include "cubemap.h"
#include "deferred\fitnormals.h"
#include "parallax.h"

// texture units
#if defined( APPLY_TERRAIN )
uniform sampler2DArray	u_ColorMap;
uniform sampler2DArray	u_NormalMap;
uniform sampler2DArray	u_GlossMap;
uniform sampler2DArray	u_GlowMap;
uniform sampler2DArray	u_LayerMap;
#else
uniform sampler2D		u_ColorMap;
uniform sampler2D		u_NormalMap;
uniform sampler2D		u_GlossMap;
uniform sampler2D		u_GlowMap;
#endif

uniform sampler2D		u_DetailMap;

// uniforms
uniform float		u_ReflectScale;
uniform vec4		u_ViewOrigin;

#if defined( APPLY_TERRAIN )
uniform float		u_Smoothness[TERRAIN_NUM_LAYERS];
#else
uniform float		u_Smoothness;
#endif

// shared variables
varying mat3		var_WorldMat;
varying vec2		var_TexDiffuse;
varying vec2		var_TexDetail;
varying vec2		var_TexGlobal;
varying vec3		var_Position;
flat varying vec4		var_LightNums0;
flat varying vec4		var_LightNums1;

void main( void )
{
	vec4 albedo = vec4( 0.0 );
	vec4 normal = vec4( 0.0 );
	vec4 smoothness = vec4( 0.0 );
	vec2 vec_TexDiffuse = var_TexDiffuse;

// parallax     
#if defined( PARALLAX_SIMPLE )
	vec_TexDiffuse = ParallaxMapSimple(var_TexDiffuse.xy, normalize(var_ViewDir));
#elif defined( PARALLAX_OCCLUSION )
	vec_TexDiffuse = ParallaxOcclusionMap(var_TexDiffuse.xy, normalize(var_ViewDir)).xy;
#endif

#if defined( APPLY_TERRAIN )
	vec4 mask0, mask1, mask2, mask3;
	TerrainReadMask( var_TexGlobal, mask0, mask1, mask2, mask3 );
	albedo = TerrainMixDiffuse( u_ColorMap, vec_TexDiffuse, mask0, mask1, mask2, mask3 );

	// apply the detail texture
#if defined( HAS_DETAIL )
	albedo.rgb *= detailmap2D( u_DetailMap, var_TexGlobal ).rgb * DETAIL_SCALE;
#endif

#if defined( HAS_NORMALMAP )
	normal.xyz = TerrainMixNormal( u_NormalMap, vec_TexDiffuse, mask0, mask1, mask2, mask3 );
	normal.xyz = (( var_WorldMat * normal.xyz ) + 1.0 ) * 0.5;
#else
	normal.xyz = normalize( var_WorldMat[2] );
	normal.xyz = ( normal.xyz + 1.0 ) * 0.5;
#endif

#if defined( HAS_GLOSSMAP )
	smoothness = TerrainMixGlossmap( u_GlossMap, vec_TexDiffuse, mask0, mask1, mask2, mask3 );
	smoothness.a = TerrainMixSmoothness( u_Smoothness, mask0, mask1, mask2, mask3 );
#endif

#else	// !APPLY_TERRAIN
	albedo = colormap2D( u_ColorMap, vec_TexDiffuse );

	// apply the detail texture
#if defined( HAS_DETAIL )
	albedo.rgb *= detailmap2D( u_DetailMap, var_TexDetail ).rgb * DETAIL_SCALE;
#endif
	// apply fullbright pixels
#if defined( HAS_LUMA )
	albedo.rgb += texture( u_GlowMap, vec_TexDiffuse ).rgb;
#endif

#if defined( HAS_LUMA ) || defined( LIGHTING_FULLBRIGHT )
	normal.w = 1.0; // this pixel is fullbright
#endif

#if defined( HAS_NORMALMAP )
	normal.xyz = normalmap2D( u_NormalMap, vec_TexDiffuse );
	normal.xyz = (( var_WorldMat * normal.xyz ) + 1.0 ) * 0.5;
#else
	normal.xyz = normalize( var_WorldMat[2] );
	normal.xyz = ( normal.xyz + 1.0 ) * 0.5;
#endif

#if defined( HAS_GLOSSMAP )
	smoothness = colormap2D( u_GlossMap, vec_TexDiffuse );
	smoothness.a = u_Smoothness;
#endif

#endif	// !APPLY_TERRAIN

	CompressUnsignedNormalToNormalsBuffer( normal.xyz );

// multiply gloss intensity with reflection cube color
#if defined( REFLECTION_CUBEMAP )
	vec3 reflectance = CubemapReflectionProbe( var_Position, u_ViewOrigin.xyz, var_WorldMat[2], smoothness.a );
	albedo.rgb += reflectance * u_ReflectScale;
#endif
	gl_FragData[0] = albedo;		// red, green, blue, alpha
	gl_FragData[1] = normal;		// X, Y, Z, flag fullbright
	gl_FragData[2] = smoothness;		// gloss R, G, B, smoothness factor
	gl_FragData[3] = var_LightNums0;	// number of lights affected this face
	gl_FragData[4] = var_LightNums1;
}