/*
scene_studio_vp.glsl - g-buffer rendering studio triangles
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
#include "cubemap.h"
#include "deferred\fitnormals.h"

uniform sampler2D		u_ColorMap;
uniform sampler2D		u_DetailMap;
uniform sampler2D		u_NormalMap;
uniform sampler2D		u_GlossMap;
uniform sampler2D		u_GlowMap;

uniform float		u_Smoothness;
uniform float		u_ReflectScale;
uniform vec3		u_ViewOrigin;
uniform vec4		u_LightNums0;
uniform vec4		u_LightNums1;

// shared variables
varying mat3		var_WorldMat;
varying vec2		var_TexDiffuse;
varying vec2		var_TexDetail;
varying vec3		var_Position;

void main( void )
{
	// compute the diffuse term
	vec4 albedo = colormap2D( u_ColorMap, var_TexDiffuse );
	vec4 normal = vec4( 0.0 );
	vec4 smooth = vec4( 0.0 );

#if defined( HAS_DETAIL )
	albedo.rgb *= detailmap2D( u_DetailMap, var_TexDetail ).rgb * DETAIL_SCALE;
#endif

#if defined( LIGHTING_FLATSHADE )
	normal.w = NORMAL_FLATSHADE;
#endif
	// apply fullbright pixels
#if defined( HAS_LUMA )
	vec3 luma = texture2D( u_GlowMap, var_TexDiffuse ).rgb;
	if( VectorMax( luma ) > EQUAL_EPSILON )
		normal.w = 1.0;	// this pixel is fullbright
	albedo.rgb += luma;		// add luma color
#endif

#if defined( LIGHTING_FULLBRIGHT )
	normal.w = 1.0; // this pixel is fullbright
#endif

#if defined( HAS_NORMALMAP )
	normal.xyz = normalmap2D( u_NormalMap, var_TexDiffuse );
	normal.xyz = ( normalize( var_WorldMat * normal.xyz ) + 1.0 ) * 0.5;
#else
	normal.xyz = normalize( var_WorldMat[2] );
	normal.xyz = ( normal.xyz + 1.0 ) * 0.5;
#endif

	CompressUnsignedNormalToNormalsBuffer( normal.xyz );

#if defined( HAS_GLOSSMAP )
	smooth = colormap2D( u_GlossMap, var_TexDiffuse );
	smooth.a = u_Smoothness;
#endif

// multiply gloss intensity with reflection cube color
#if defined( REFLECTION_CUBEMAP )
	vec3 reflectance = CubemapReflectionProbe( var_Position, u_ViewOrigin.xyz, var_WorldMat[2], smooth.a );
	albedo.rgb += reflectance * u_ReflectScale;
#endif
	gl_FragData[0] = albedo;	// red, green, blue, alpha
	gl_FragData[1] = normal;	// X, Y, Z, flag fullbright
	gl_FragData[2] = smooth;	// gloss R, G, B, smooth factor
	gl_FragData[3] = u_LightNums0 * (1.0 / 255.0);
	gl_FragData[4] = u_LightNums1 * (1.0 / 255.0);
}