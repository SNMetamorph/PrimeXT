/*
BmodelDlight_fp.glsl - fragment uber shader for all dlight types for bmodel surfaces
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
#include "terrain.h"

#if defined( BMODEL_MULTI_LAYERS )
uniform sampler2DArray	u_ColorMap;
#else
uniform sampler2D		u_ColorMap;
#endif
uniform sampler2D		u_DetailMap;
#if defined( BMODEL_LIGHT_PROJECTION )
uniform sampler2D		u_ProjectMap;
#elif defined( BMODEL_LIGHT_OMNIDIRECTIONAL )
uniform samplerCube		u_ProjectMap;
#endif
uniform sampler2D		u_ScreenMap;	// screen copy

#if defined( BMODEL_MULTI_LAYERS )
uniform sampler2DArray	u_HeightMap;
#endif

#if defined( BMODEL_HAS_SHADOWS )
#if defined( BMODEL_LIGHT_PROJECTION )
#include "shadow_proj.h"
#endif
#endif

uniform vec4		u_LightDir;
uniform vec4		u_LightDiffuse;
uniform vec4		u_LightOrigin;
uniform vec4		u_ShadowParams;
uniform vec2		u_ScreenSizeInv;
uniform vec4		u_RenderColor;
uniform vec4		u_FogParams;

varying vec2		var_TexDiffuse;
varying vec2		var_TexDetail;
varying vec3		var_LightVec;
varying vec3		var_Normal;

#if defined( BMODEL_REFLECTION_PLANAR )
varying vec4		var_TexMirror;	// mirror coords
#endif

varying vec2		var_TexGlobal;

#if defined( BMODEL_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
varying vec4		var_ShadowCoord;
#endif

void main( void )
{
	float dist = length( var_LightVec );
	float atten = 1.0 - saturate( pow( dist * u_LightOrigin.w, u_LightDiffuse.a ));
	if( atten <= 0.0 ) discard; // fast reject
	vec3 L = vec3( 0.0 );

#if defined( BMODEL_LIGHT_PROJECTION )
	L = normalize( u_LightDir.xyz );

	// spot attenuation
	float spotDot = dot( normalize( var_LightVec ), L );
	float fov = ( u_LightDir.w * FOV_MULT * ( M_PI / 180.0 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos ) discard;
#elif defined( BMODEL_LIGHT_OMNIDIRECTIONAL )
	L = normalize( var_LightVec );
#endif
	vec3 N = normalize( var_Normal );

#if defined( BMODEL_MULTI_LAYERS )
	vec4 mask0, mask1 = vec4( 0.0 ), mask2 = vec4( 0.0 ), mask3 = vec4( 0.0 );

	mask0 = texture2DArray( u_HeightMap, vec3( var_TexGlobal, 0.0 ));
#if TERRAIN_NUM_LAYERS >= 4
	mask1 = texture2DArray( u_HeightMap, vec3( var_TexGlobal, 1.0 ));
#endif
#if TERRAIN_NUM_LAYERS >= 8
	mask2 = texture2DArray( u_HeightMap, vec3( var_TexGlobal, 2.0 ));
#endif
#if TERRAIN_NUM_LAYERS >= 12
	mask3 = texture2DArray( u_HeightMap, vec3( var_TexGlobal, 3.0 ));
#endif
#endif
	// compute the diffuse term
#if defined( BMODEL_REFLECTION_PLANAR )
	vec4 diffuse = texture2DProj( u_ColorMap, var_TexMirror );
#elif defined( BMODEL_MULTI_LAYERS )
	vec4 diffuse = TerrainApplyDiffuse( u_ColorMap, var_TexDiffuse, mask0, mask1, mask2, mask3 );
#else
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );
#endif
	// apply the detail texture
#if defined( BMODEL_HAS_DETAIL )
	diffuse.rgb *= texture2D( u_DetailMap, var_TexGlobal ).rgb * DETAIL_SCALE;
#endif
	vec3 light = vec3( 1.0 );
	float shadow = 1.0;

#if defined( BMODEL_LIGHT_PROJECTION )
	light = u_LightDiffuse.rgb;	// light color

	// texture or procedural spotlight
	light *= texture2DProj( u_ProjectMap, var_ProjCoord ).rgb;
//	atten *= smoothstep( spotCos, spotCos + 0.1, spotDot ) * 0.5;
#if defined( BMODEL_HAS_SHADOWS )
	shadow = ShadowProj( var_ShadowCoord, u_ShadowParams.xy, dot( N, L ));
	if( shadow <= 0.0 ) discard; // fast reject
#endif
#elif defined( BMODEL_LIGHT_OMNIDIRECTIONAL )
	light = u_LightDiffuse.rgb;

	light *= textureCube( u_ProjectMap, -var_LightVec ).rgb;
#endif

#if defined( BMODEL_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	atten = Q_mix( 0.0, atten, fogFactor );
#endif
	// do modified hemisperical lighting
	float NdotL = max( (dot( N, L ) + ( SHADE_LAMBERT - 1.0 )) / SHADE_LAMBERT, 0.0 );
	diffuse.rgb *= light.rgb * NdotL * atten * DLIGHT_SCALE;

#if defined( BMODEL_TRANSLUCENT )
	vec3 screenmap = texture2D( u_ScreenMap, gl_FragCoord.xy * u_ScreenSizeInv ).xyz;
	screenmap.rgb *= light.rgb * NdotL * atten;
	diffuse.rgb = Q_mix( screenmap, diffuse.rgb, u_RenderColor.a );
#endif
	// compute final color
	gl_FragColor = diffuse * shadow;
}