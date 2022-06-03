/*
scene_decal_bmodel_fp.glsl - fragment uber shader for bmodel decals
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
#include "deferred\fitnormals.h"

uniform sampler2D		u_ColorMap;	// surface under decal
uniform sampler2D		u_DecalMap;
uniform sampler2D		u_GlossMap;
uniform sampler2D		u_NormalMap;	// refraction

uniform sampler2D		u_FragData0;
uniform sampler2D		u_FragData1;
uniform sampler2D		u_FragData2;

uniform vec2		u_ScreenSizeInv;
uniform float		u_Smoothness;
uniform float		u_RefractScale;
uniform float		u_ReflectScale;
uniform vec3		u_ViewOrigin;
uniform float		u_RealTime;
uniform vec4		u_LightNums0;
uniform vec4		u_LightNums1;

varying vec4		var_TexDiffuse;
varying vec2		var_TexGlobal;
varying vec2		var_TexDetail;
varying vec3		var_ViewDir;
varying mat3		var_WorldMat;
varying vec3		var_Position;

void main( void )
{
	vec4 albedo = vec4( 0.0 );
	vec4 normal = vec4( 0.0 );
	vec4 smooth = vec4( 0.0 );

	vec2 screenCoord = gl_FragCoord.xy * u_ScreenSizeInv;
	vec4 frag_diffuse = colormap2D( u_FragData0, screenCoord );
	vec4 frag_normal = colormap2D( u_FragData1, screenCoord );
	frag_normal.xyz = normalize( frag_normal.xyz * 2.0 - 1.0 );
	vec4 frag_smooth = colormap2D( u_FragData2, screenCoord );

#if defined( ALPHA_TEST )
	if( frag_diffuse.a <= BMODEL_ALPHA_THRESHOLD )
		discard;
#endif
	vec3 V = normalize( var_ViewDir );
	vec3 vecTexCoord = vec3( var_TexDiffuse.xy, 1.0 );

#if defined( PARALLAX_SIMPLE )
	float offset = texture2D( u_HeightMap, vecTexCoord.xy ).r * 0.04 - 0.02;
	vecTexCoord.xy = ( offset * -V.xy + vecTexCoord.xy );
#elif defined( PARALLAX_OCCLUSION )
	vecTexCoord = ParallaxOcclusionMap( var_TexDiffuse.xy, V );
#ifdef LATER
	vec4 vertex = vec4( var_Position + var_WorldMat * vec3( 0.0, 0.0, vecTexCoord.z * 15.0 ), 1.0 );
	vec4 p = gl_ModelViewProjectionMatrix * vertex;
	gl_FragDepth = 0.5 * (p.z / p.w + 1.0);
#endif
#endif

#if defined( HAS_NORMALMAP )
#if defined( DECAL_PUDDLE )
	vec2 tc_Normal0 = vecTexCoord.xy * 0.1;
	tc_Normal0.s += u_RealTime * 0.005;
	tc_Normal0.t -= u_RealTime * 0.005;

	vec2 tc_Normal1 = vecTexCoord.xy * 0.1;
	tc_Normal1.s -= u_RealTime * 0.005;
	tc_Normal1.t += u_RealTime * 0.005;

	vec3 N0 = normalmap2D( u_NormalMap, tc_Normal0 );
	vec3 N1 = normalmap2D( u_NormalMap, tc_Normal1 );
	vec3 N = normalize( vec3( ( N0.xy / N0.z ) + ( N1.xy / N1.z ), 1.0 ));
	normal.xyz = (var_WorldMat * N);
#else
	vec3 N = normalmap2D( u_NormalMap, vecTexCoord.xy );
	normal.xyz = (var_WorldMat * N);
#endif
#else
	// just use source normals
	normal.xyz = frag_normal.rgb;
	normal.xyz = normalize( var_WorldMat[2] );
#endif

#if defined( HAS_GLOSSMAP )
	smooth = colormap2D( u_GlossMap, vecTexCoord.xy );
	smooth.a = u_Smoothness;
#endif
	vec4 diffuse = decalmap2D( u_DecalMap, vecTexCoord.xy );

#if defined( APPLY_COLORBLEND )
	float alpha = (( diffuse.r * 2.0 ) + ( diffuse.g * 2.0 ) + ( diffuse.b * 2.0 )) / 3.0;
#endif

#if defined( DECAL_PUDDLE )
	diffuse = vec4( 0.19, 0.16, 0.11, 1.0 ); // replace with puddle color
#endif

#if defined( REFLECTION_CUBEMAP )
	normal.xyz *= 0.02;
	vec3 waveNormal = normal.xyz * u_RefractScale * alpha;
	vec3 mirror = CubemapReflectionProbe( var_Position, u_ViewOrigin, PUDDLE_NORMAL + waveNormal, 1.0 );
	float eta = GetFresnel( V, PUDDLE_NORMAL, WATER_F0_VALUE, FRESNEL_FACTOR ) * u_ReflectScale;
	diffuse.rgb = mix( diffuse.rgb, mirror.rgb, eta );
#if defined( APPLY_COLORBLEND )
	diffuse.rgb = mix( diffuse.rgb, vec3( 0.5 ), alpha );
#endif
#endif

#if defined( APPLY_COLORBLEND )
#if defined( DECAL_PUDDLE )
	albedo.rgb = mix( diffuse.rgb, frag_diffuse.rgb, alpha );
#else
	albedo.rgb = diffuse.rgb * frag_diffuse.rgb + diffuse.rgb * frag_diffuse.rgb;
#endif
	normal.rgb = mix( normal.xyz, frag_normal.rgb, alpha );
	smooth = mix( smooth, frag_smooth, alpha );
#else
	albedo.rgb = mix( frag_diffuse.rgb, diffuse.rgb, diffuse.a );
	normal.rgb = mix( frag_normal.rgb, normal.xyz, diffuse.a );
	smooth = mix( smooth, frag_smooth, diffuse.a );
#endif
	normal.xyz = ( normal.xyz + 1.0 ) * 0.5;
	CompressUnsignedNormalToNormalsBuffer( normal.xyz );

	albedo.a = frag_diffuse.a;
	normal.w = frag_normal.w;

	gl_FragData[0] = albedo;		// red, green, blue, alpha
	gl_FragData[1] = normal;		// X, Y, Z, flag fullbright
	gl_FragData[2] = smooth;		// gloss R, G, B, smooth factor
}