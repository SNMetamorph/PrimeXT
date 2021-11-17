/*
light_grass_fp.glsl - fragment uber shader for all dlight types for grass meshes
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

uniform sampler2D		u_ColorMap;
uniform sampler2D		u_ProjectMap;

#if defined( APPLY_SHADOW )
#if defined( LIGHT_SPOT )
#include "shadow_spot.h"
#elif defined( LIGHT_OMNI )
#include "shadow_omni.h"
#elif defined( LIGHT_PROJ )
#include "shadow_proj.h"
#endif
#endif

uniform vec4		u_LightDir;
uniform vec3		u_LightDiffuse;
uniform vec4		u_LightOrigin;
uniform vec4		u_ShadowParams;
uniform float		u_AmbientFactor;
uniform float		u_SunRefract;

varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;
varying vec3		var_ViewVec;
varying vec3		var_Normal;

#if defined( LIGHT_SPOT )
varying vec4		var_ProjCoord;
#endif

#if defined( LIGHT_SPOT ) || defined( LIGHT_PROJ )
varying vec4		var_ShadowCoord;
#endif

void main( void )
{
#if !defined( LIGHT_PROJ )
	float dist = length( var_LightVec );
	float atten = 1.0 - saturate( pow( dist * u_LightOrigin.w, LIGHT_FALLOFF ));
	if( atten <= 0.0 ) discard; // fast reject
#else
	// directional light has no attenuation
	float atten = 1.0;
#endif
	vec3 V = normalize( var_ViewVec );
	vec3 L = vec3( 0.0 );

#if defined( LIGHT_SPOT )
	L = normalize( var_LightVec );
	
	// spot attenuation
	float spotDot = dot( normalize( u_LightDir.xyz ), L );
	float fov = ( u_LightDir.w * FOV_MULT * ( M_PI / 180.0 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos ) discard;
#elif defined( LIGHT_OMNI )
	L = normalize( var_LightVec );
#elif defined( LIGHT_PROJ )
	L = normalize( u_LightDir.xyz );
#endif
	vec3 N = normalize( var_Normal );

	// compute the diffuse term
	vec4 diffuse = colormap2D( u_ColorMap, var_TexDiffuse );

	vec3 light = vec3( 1.0 );
	float shadow = 1.0;

	if( bool( gl_FrontFacing )) N = -N, V = -V;

	light = u_LightDiffuse * DLIGHT_SCALE;	// light color
	float NdotL = saturate( dot( N, L ));
	if( NdotL <= 0.0 ) discard; // fast reject

#if defined( LIGHT_SPOT )
	// texture or procedural spotlight
	light *= texture2DProj( u_ProjectMap, var_ProjCoord ).rgb;
#if defined( APPLY_SHADOW )
	if( NdotL > 0.0 ) shadow = ShadowSpot( var_ShadowCoord, u_ShadowParams.xy );
#endif
#elif defined( LIGHT_OMNI )
#if defined( APPLY_SHADOW )
	if( NdotL > 0.0 ) shadow = ShadowOmni( -var_LightVec, u_ShadowParams );
#endif
#elif defined( LIGHT_PROJ )
#if defined( APPLY_SHADOW )
	if( NdotL > 0.0 )
	{
		shadow = ShadowProj( var_ShadowCoord.xyz );
		shadow = mix( shadow, 0.0, u_SunRefract );
		shadow = max( shadow, u_AmbientFactor );
	}
#endif
#endif
	if( shadow <= 0.0 ) discard; // fast reject

	vec3 albedo = diffuse.rgb;
	diffuse.rgb *= light.rgb * NdotL * atten * shadow;
#if defined( LIGHT_PROJ )
	diffuse.rgb += (albedo * u_LightDiffuse * u_AmbientFactor);
#endif
	// compute final color
	gl_FragColor = diffuse;
}