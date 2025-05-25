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
#include "material.h"
#include "fog.h"

uniform sampler2D	u_DecalMap;
uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_GlossMap;

uniform float	u_Smoothness;
uniform float	u_RenderAlpha;
uniform float	u_AmbientFactor;
uniform vec2	u_LightShade;
uniform vec4	u_FogParams;
uniform vec3	u_ViewOrigin;

varying vec4	var_TexDiffuse;
varying vec3	var_AmbientLight;
varying vec3	var_DiffuseLight;

#if defined( REFLECTION_CUBEMAP )
varying vec3	var_WorldNormal;
#endif

varying vec3	var_LightDir;
varying vec3	var_ViewDir;
varying vec3	var_Normal;
varying vec3	var_Position;

void main( void )
{
	MaterialData mat;
	LightingData lighting;
	vec2 vecTexCoord = var_TexDiffuse.xy;
	vec3 V = normalize( var_ViewDir );
	if( bool( gl_FrontFacing )) {
		V = -V;
	}

#if defined( PARALLAX_SIMPLE )
	//vecTexCoord = ParallaxMapSimple(var_TexDiffuse.xy, V);
	//vecTexCoord = ParallaxOffsetMap(u_NormalMap, var_TexDiffuse.xy, V);
	vecTexCoord = ParallaxReliefMap(var_TexDiffuse.xy, V);
#elif defined( PARALLAX_OCCLUSION )
	vecTexCoord = ParallaxOcclusionMap(var_TexDiffuse.xy, V).xy;
#endif

#if defined( ALPHA_TEST )
	if( colormap2D( u_ColorMap, var_TexDiffuse.zw ).a <= STUDIO_ALPHA_THRESHOLD )
		discard;
#endif
	vec4 albedo = decalmap2D( u_DecalMap, vecTexCoord );
	vec4 result = albedo;

	// decal fading on monster corpses
#if defined( APPLY_COLORBLEND )
	result.rgb = mix( vec3( 0.5 ), result.rgb, u_RenderAlpha );
#else
	result.a *= u_RenderAlpha;
#endif

#if defined( HAS_NORMALMAP )
	vec3 N = normalmap2D( u_NormalMap, vecTexCoord );
#else
	vec3 N = normalize( var_Normal );
#endif
	// two side materials support
	if( bool( gl_FrontFacing )) N = -N;

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

#if !defined( LIGHTING_FULLBRIGHT )
	lighting.specular = vec3( 0.0 );
	lighting.diffuse = vec3( 0.0 );

	vec3 L = normalize( var_LightDir );
#if defined( LIGHTVEC_DEBUG )
	lighting.diffuse = ( normalize( N + L ) + 1.0 ) * 0.5;
#else // !LIGHTVEC_DEBUG
#if defined( HAS_NORMALMAP )
#if defined( VERTEX_LIGHTING )
	lighting = ComputeLighting( N, V, L, albedo.rgb, var_DiffuseLight, mat );
	lighting.diffuse += var_AmbientLight;
#else
	lighting = ComputeLighting( N, V, L, albedo.rgb, var_DiffuseLight, mat );
	lighting.specular *= u_LightShade.y;
	lighting.diffuse *= u_LightShade.y;
	lighting.diffuse += var_AmbientLight;
#endif
#else // !HAS_NORMALMAP
	lighting.diffuse = var_AmbientLight + var_DiffuseLight;
#endif // HAS_NORMALMAP
#endif // LIGHTVEC_DEBUG
#if defined( APPLY_PBS )
	result.rgb = (lighting.kD * result.rgb / M_PI) * lighting.diffuse;
#else
	result.rgb *= lighting.diffuse; // apply diffuse/ambient lighting
#endif
	result.rgb += lighting.specular; // apply specular lighting

#if defined( LIGHTMAP_DEBUG ) || defined( LIGHTVEC_DEBUG )
	result.rgb = lighting.diffuse;
#endif
#endif // LIGHTING_FULLBRIGHT

#if defined( APPLY_FOG_EXP )
	result.rgb = CalculateFog(result.rgb, u_FogParams, gl_FragCoord.z / gl_FragCoord.w);
#endif
	gl_FragColor = result;
}