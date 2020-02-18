/*
GenericDlight_fp.glsl - common fragment uber shader for all dlight types
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

#if defined( GENERIC_LIGHT_PROJECTION )
uniform sampler2D		u_ProjectMap;
#elif defined( GENERIC_LIGHT_OMNIDIRECTIONAL )
uniform samplerCube		u_ProjectMap;
#endif
uniform sampler2D		u_ColorMap;

#if defined( GENERIC_HAS_SHADOWS )
#if defined( GENERIC_LIGHT_PROJECTION )
#include "shadow_proj.h"
#endif
#endif

uniform vec4		u_LightDir;
uniform vec4		u_LightDiffuse;
uniform vec4		u_LightOrigin;
uniform vec4		u_ShadowParams;
uniform vec4		u_FogParams;
uniform float		u_LightScale;

varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;

#if defined( GENERIC_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
#if defined( GENERIC_HAS_SHADOWS )
varying vec4		var_ShadowCoord;
#endif
#endif

void main( void )
{
	float dist = length( var_LightVec );
	float atten = 1.0 - saturate( pow( dist * u_LightOrigin.w, u_LightDiffuse.a ));
	if( atten <= 0.0 ) discard; // fast reject
	vec3 L = vec3( 0.0 );

#if defined( GENERIC_LIGHT_PROJECTION )
	L = normalize( u_LightDir.xyz );
	
	// spot attenuation
	float spotDot = dot( normalize( var_LightVec ), L );
	float fov = ( u_LightDir.w * FOV_MULT * ( M_PI / 180.0 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos ) discard;
#elif defined( GENERIC_LIGHT_OMNIDIRECTIONAL )
	L = normalize( var_LightVec );
#endif
	// compute the diffuse term
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );
	diffuse.rgb *= diffuse.a; // FIXME: it's actual only for 'indexalpha' sprites
	vec3 light = vec3( 1.0 );
	float shadow = 1.0;

#if defined( GENERIC_LIGHT_PROJECTION )
	light = u_LightDiffuse.rgb;	// light color

	// texture or procedural spotlight
	light *= texture2DProj( u_ProjectMap, var_ProjCoord ).rgb;
//	atten *= smoothstep( spotCos, spotCos + 0.1, spotDot ) * 0.5;
#if defined( GENERIC_HAS_SHADOWS )
	shadow = ShadowProj( var_ShadowCoord, u_ShadowParams.xy, NORMAL_FLATSHADE );
	if( shadow <= 0.0 ) discard; // fast reject
#endif
#elif defined( GENERIC_LIGHT_OMNIDIRECTIONAL )
	light = u_LightDiffuse.rgb;

	light *= textureCube( u_ProjectMap, -var_LightVec ).rgb;
#endif
#if defined( GENERIC_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	atten = Q_mix( 0.0, atten, fogFactor );
#endif
	diffuse.rgb *= light * NORMAL_FLATSHADE * atten * DLIGHT_SCALE * u_LightScale;

	// compute final color
	gl_FragColor = diffuse * shadow;
}