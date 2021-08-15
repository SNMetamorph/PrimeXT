/*
decal_studio_fp.glsl - fragment uber shader for studio decals
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
#include "lightmodel.h"
#include "texfetch.h"
#include "parallax.h"

uniform sampler2D	u_DecalMap;
uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_GlossMap;

uniform float	u_Smoothness;
uniform float	u_RenderAlpha;
uniform float	u_DiffuseFactor;
uniform float	u_AmbientFactor;
uniform vec2	u_LightShade;
uniform vec4	u_FogParams;

varying vec4	var_TexDiffuse;
varying vec3	var_AmbientLight;
varying vec3	var_DiffuseLight;

#if defined( REFLECTION_CUBEMAP )
varying vec3	var_WorldNormal;
varying vec3	var_Position;
#endif

varying vec3	var_LightDir;
varying vec3	var_ViewDir;
varying vec3	var_Normal;

void main( void )
{
	vec2 vecTexCoord = var_TexDiffuse.xy;
	vec3 V = normalize( var_ViewDir );
	if( bool( gl_FrontFacing )) V = -V;

#if defined( PARALLAX_SIMPLE )
	float offset = texture2D( u_HeightMap, vecTexCoord ).r * 0.04 - 0.02;
	vecTexCoord = ( offset * V.xy + vecTexCoord );
#elif defined( PARALLAX_OCCLUSION )
	vec3 srcV = vec3( V.x, V.y, -V.z );
	vecTexCoord = ParallaxOcclusionMap( var_TexDiffuse.xy, srcV ).xy;
#endif

#if defined( ALPHA_TEST )
	if( colormap2D( u_ColorMap, var_TexDiffuse.zw ).a <= STUDIO_ALPHA_THRESHOLD )
		discard;
#endif
	vec4 diffuse = decalmap2D( u_DecalMap, vecTexCoord );

	// decal fading on monster corpses
#if defined( APPLY_COLORBLEND )
	diffuse.rgb = mix( vec3( 0.5 ), diffuse.rgb, u_RenderAlpha );
#else
	diffuse.a *= u_RenderAlpha;
#endif

#if defined( HAS_NORMALMAP )
	vec3 N = normalmap2D( u_NormalMap, vecTexCoord );
#else
	vec3 N = normalize( var_Normal );
#endif
	// two side materials support
	if( bool( gl_FrontFacing )) N = -N;

#if !defined( LIGHTING_FULLBRIGHT )
	vec3 specular = vec3( 0.0 );
	vec4 glossmap = vec4( 0.0 );
	vec3 light = vec3( 1.0 );

	vec3 L = normalize( var_LightDir );
#if defined( LIGHTVEC_DEBUG )
	light = ( normalize( N + L ) + 1.0 ) * 0.5;
#else //!LIGHTVEC_DEBUG
#if defined( HAS_NORMALMAP )
#if defined (VERTEX_LIGHTING)
	float NdotB = ComputeStaticBump( L, N, u_AmbientFactor );
	float factor = DiffuseBRDF( N, V, L, u_Smoothness, NdotB );
	light = (var_DiffuseLight * factor) + (var_AmbientLight);
#else
	float factor = u_LightShade.y;
	float NdotL = (( dot( N, -L ) + ( SHADE_LAMBERT - 1.0 )) / SHADE_LAMBERT );
	if( NdotL > 0.0 ) factor -= DiffuseBRDF( N, V, L, u_Smoothness, saturate( NdotL )) * u_LightShade.y;
	light = (var_DiffuseLight * factor) + (var_AmbientLight);
#endif
#else //!HAS_NORMALMAP
	light = var_AmbientLight + var_DiffuseLight;
#endif//HAS_NORMALMAP
#endif//LIGHTVEC_DEBUG

	diffuse.rgb *= light;

// compute the specular term
#if defined( HAS_GLOSSMAP )
	float NdotL2 = saturate( dot( N, L ));
	glossmap = texture2D( u_GlossMap, vecTexCoord );
	specular = SpecularBRDF( N, V, L, u_Smoothness, glossmap.rgb ) * NdotL2;
	diffuse.rgb *= saturate( 1.0 - GetLuminance( specular ));
#if defined (VERTEX_LIGHTING)
	diffuse.rgb += var_DiffuseLight * specular;
#else
	diffuse.rgb += var_DiffuseLight * specular * u_LightShade.y; // prevent parazite lighting
#endif
#endif// HAS_GLOSSMAP

#if defined( LIGHTMAP_DEBUG ) || defined( LIGHTVEC_DEBUG )
	diffuse.rgb = light;
#endif
#endif// LIGHTING_FULLBRIGHT

#if defined( APPLY_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
#endif
	gl_FragColor = diffuse;
}