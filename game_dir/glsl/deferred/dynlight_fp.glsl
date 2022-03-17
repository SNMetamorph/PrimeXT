/*
dynlight_fp.glsl - add dynamic lights
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
#include "texfetch.h"
#include "matrix.h"
#include "mathlib.h"
#include "lightmodel.h"

#if defined( APPLY_SHADOW )
#if defined( LIGHT_SPOT )
#include "shadow_spot.h"
#elif defined( LIGHT_OMNI )
#include "shadow_omni.h"
#endif
#endif

uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_GlossMap;
uniform sampler2D	u_DepthMap;
uniform sampler2D	u_ProjectMap;

uniform vec3	u_ViewOrigin;
uniform vec4	u_LightDir;
uniform vec3	u_LightDiffuse;
uniform vec4	u_LightOrigin;
uniform vec4	u_ShadowParams;
uniform mat4	u_LightViewProjMatrix;
uniform float	u_AmbientFactor;
uniform float	u_zFar;

varying vec2	var_TexCoord;
varying vec3 	var_RayVec;

void main( void )
{
	vec4 normal = texture2D( u_NormalMap, var_TexCoord );

	// ignore fullbright pixels
	if( normal.w == 1.0 )
		discard;

	vec3 diffuse = texture2D( u_ColorMap, var_TexCoord ).rgb;
	vec4 glossmap = colormap2D( u_GlossMap, var_TexCoord );
	float srcW = texture2D( u_DepthMap, var_TexCoord ).r;
	float w = linearizeDepth( u_zFar, srcW );
	vec4 worldpos = vec4( u_ViewOrigin + var_RayVec * w, 1.0 ); // nudge point to avoid z-fighting
	vec3 lvec = u_LightOrigin.xyz - worldpos.xyz;
	vec3 N = normalize( normal.xyz * 2.0 - 1.0 );
	vec3 V = normalize( -var_RayVec );
	vec3 L = normalize( lvec );

	float dist = length( lvec );
	float atten = 1.0 - saturate( pow( dist * u_LightOrigin.w, LIGHT_FALLOFF ));
	if( atten <= 0.0 ) discard; // fast reject

#if defined( LIGHT_SPOT )
	L = normalize( u_LightDir.xyz ); // override
	// spot attenuation
	float spotDot = dot( normalize( lvec ), L );
	float fov = ( u_LightDir.w * 0.25 * ( M_PI / 180.0 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos ) discard;
#endif
	vec3 light = vec3( 1.0 );
	float shadow = 1.0;

	light = u_LightDiffuse * DLIGHT_SCALE;	// light color
	float NdotL = saturate( dot( N, L ));
	if( NdotL <= 0.0 ) discard; // fast reject

#if defined( LIGHT_SPOT )
	vec4 tproj = ( Mat4Texture( -0.5 ) * u_LightViewProjMatrix ) * worldpos;
	light *= texture2DProj( u_ProjectMap, tproj ).rgb;
#if defined( APPLY_SHADOW )
	vec4 sproj = ( Mat4Texture( 0.5 ) * u_LightViewProjMatrix ) * worldpos;
	if( NdotL > 0.0 ) shadow = ShadowSpot( sproj, u_ShadowParams.xy );
#endif
#elif defined( LIGHT_OMNI )
#if defined( APPLY_SHADOW )
	if( NdotL > 0.0 ) shadow = ShadowOmni( -lvec, u_ShadowParams );
#endif
#endif
	if( shadow <= 0.0 ) discard; // fast reject

	vec3 albedo = diffuse.rgb;
	diffuse.rgb *= light.rgb * atten * shadow;
#if defined( HAS_GLOSSMAP )
	//vec3 gloss = SpecularBRDF( N, V, L, glossmap.a, glossmap.rgb ) * light * NdotL * atten;
	diffuse.rgb += shadow; // * gloss;
#endif
	gl_FragColor = vec4( diffuse, 1.0 );
}