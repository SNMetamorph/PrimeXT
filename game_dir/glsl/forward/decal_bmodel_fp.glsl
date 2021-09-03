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

#if defined( PLANAR_REFLECTION )
varying vec4	var_TexMirror;	// mirror coords
#endif

#if defined( REFLECTION_CUBEMAP )
varying mat3	var_WorldMat;
varying vec3	var_Position;
#endif

void main( void )
{
	vec3 light = vec3( 0.0 );
	vec3 gloss = vec3( 0.0 );

#if defined( ALPHA_TEST )
	if( colormap2D( u_ColorMap, var_TexDiffuse.zw ).a <= BMODEL_ALPHA_THRESHOLD )
		discard;
#endif
	vec3 V = normalize( var_ViewDir );
	vec4 glossmap = vec4( 0.0 );
	vec2 vecTexCoord = var_TexDiffuse.xy;

#if defined( PARALLAX_SIMPLE )
	//vec_TexDiffuse = ParallaxMapSimple(var_TexDiffuse.xy, var_TangentViewDir);
	vecTexCoord = ParallaxOffsetMap(u_NormalMap, var_TexDiffuse.xy, var_TangentViewDir);
	//vec_TexDiffuse = ParallaxReliefMap(var_TexDiffuse.xy, var_TangentViewDir);
#elif defined( PARALLAX_OCCLUSION )
	vecTexCoord = ParallaxOcclusionMap(var_TexDiffuse.xy, var_TangentViewDir).xy; 
#endif
	vec4 diffuse = decalmap2D( u_DecalMap, vecTexCoord );

#if defined( APPLY_COLORBLEND )
	float alpha = (( diffuse.r * 2.0 ) + ( diffuse.g * 2.0 ) + ( diffuse.b * 2.0 )) / 3.0;
#endif

#if defined( DECAL_PUDDLE )
	diffuse = vec4( 0.19, 0.16, 0.11, 1.0 ); // replace with puddle color
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

#if defined( HAS_GLOSSMAP ) && defined( HAS_DELUXEMAP )
	glossmap = texture2D( u_GlossMap, vecTexCoord );
#endif

#if defined( APPLY_STYLE0 )
	ApplyLightStyle( var_TexLight0, N, V, glossmap.rgb, u_Smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE1 )
	ApplyLightStyle( var_TexLight1, N, V, glossmap.rgb, u_Smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE2 )
	ApplyLightStyle( var_TexLight2, N, V, glossmap.rgb, u_Smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE3 )
	ApplyLightStyle( var_TexLight3, N, V, glossmap.rgb, u_Smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE0 ) || defined( APPLY_STYLE1 ) || defined( APPLY_STYLE2 ) || defined( APPLY_STYLE3 )
	// convert values back to normal range and clamp it
	light = min(( light * LIGHTMAP_SHIFT ), 1.0 );
	gloss = min(( gloss * LIGHTMAP_SHIFT ), 1.0 );
#endif

#if !defined( APPLY_COLORBLEND )
	diffuse.rgb *= light;
#endif // APPLY_COLORBLEND
	diffuse.rgb += gloss;

#if defined( PLANAR_REFLECTION ) || defined( REFLECTION_CUBEMAP )
#if defined( REFLECTION_CUBEMAP )
	vec3 waveNormal = N * 0.02 * u_RefractScale; // puddles are always placed on floor so we skip rotation from tangentspace to worldspace
	vec3 mirror = GetReflectionProbe( var_Position, u_ViewOrigin, PUDDLE_NORMAL + waveNormal, glossmap.rgb, 1.0 );
#elif defined( PLANAR_REFLECTION )
	vec3 mirror = reflectmap2D( u_ColorMap, var_TexMirror, N, gl_FragCoord.xyz, u_RefractScale ).rgb;
#endif
	float eta = GetFresnel( V, PUDDLE_NORMAL, FRESNEL_FACTOR, u_ReflectScale );
	diffuse.rgb = mix( diffuse.rgb, mirror.rgb, eta );
#if defined( APPLY_COLORBLEND )
	diffuse.rgb = mix( diffuse.rgb, vec3( 0.5 ), alpha );
#endif
#endif

#if defined( APPLY_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
#if defined( APPLY_COLORBLEND )
	diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
#else
	diffuse.a *= fogFactor; // modulate alpha
#endif
#endif
	gl_FragColor = diffuse;
}