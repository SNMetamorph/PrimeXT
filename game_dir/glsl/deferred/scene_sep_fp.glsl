/*
scene_fp.glsl - combine deferred lighting with albedo
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
#include "lightmodel.h"
#include "deferred\worldlight.h"

// shows lights count per pixel from 1 (bright green) to MAXDYNLIGHTS (bright red)
//#define LIGHT_COUNT_DEBUG
//#define DEBUG_LIGHTMAP

uniform sampler2D		u_ColorMap;
uniform sampler2D		u_NormalMap;
uniform sampler2D		u_GlossMap;
uniform sampler2D		u_VisLightMap0;
uniform sampler2D		u_VisLightMap1;
uniform sampler2D		u_DepthMap;
uniform sampler2D		u_ShadowMap;

// reconstruct pos
uniform vec3	u_ViewOrigin;
uniform float	u_LightStyleValues[MAX_LIGHTSTYLES];
uniform vec4	u_GammaTable[64];
uniform float	u_LightThreshold;
uniform float	u_LightGamma;
uniform float	u_LightScale;
uniform float	u_zFar;

varying vec2	var_TexCoord;
varying vec3 	var_RayVec;

void main( void )
{
	vec3 diffuse = texture2D( u_ColorMap, var_TexCoord ).rgb;
	vec4 normal = texture2D( u_NormalMap, var_TexCoord );
	float gammaIndex;

	if( normal.w == 1.0 )
	{
		// something fullbright (e.g. sky)
		gl_FragColor = vec4( diffuse, 1.0 );
		return;
	}

	vec4 glossmap = texture2D( u_GlossMap, var_TexCoord );
	vec4 shadowsrc = texture2D( u_ShadowMap, var_TexCoord );
	float w = texture2D( u_DepthMap, var_TexCoord ).r;
	normal.xyz = normalize( normal.xyz * 2.0 - 1.0 );
	w = linearizeDepth( u_zFar, RemapVal( w, 0.0, 0.8, 0.0, 1.0 ));
	vec3 pos = u_ViewOrigin + var_RayVec * (w - 1.0); // nudge point to avoid z-fighting
	vec3 V = normalize( -var_RayVec );

	vec3 lightmap = vec3( 0.0 );
	vec4 styles = vec4( 255.0 );
	vec3 s_light[MAXLIGHTMAPS];
	float numLights = 0.0;
	vec4 shadowmap;

	s_light[0] = vec3( 0.0 );
	s_light[1] = vec3( 0.0 );
	s_light[2] = vec3( 0.0 );
	s_light[3] = vec3( 0.0 );

	// read first four lights
	vec4 lights = texture2D( u_VisLightMap0, var_TexCoord ) * 255.0;
	shadowmap = smoothstep( 0.0, 0.25, shadowsrc );

	if( lights[0] != 255.0 )
	{
		ProcessWorldLight( lights[0], shadowmap[0], normal, pos, s_light, styles );
		numLights += 1.0;
	}

	if( lights[1] != 255.0 )
	{
		ProcessWorldLight( lights[1], shadowmap[1], normal, pos, s_light, styles );
		numLights += 1.0;
	}

	if( lights[2] != 255.0 )
	{
		ProcessWorldLight( lights[2], shadowmap[2], normal, pos, s_light, styles );
		numLights += 1.0;
	}

	if( lights[3] != 255.0 )
	{
		ProcessWorldLight( lights[3], shadowmap[3], normal, pos, s_light, styles );
		numLights += 1.0;
	}

	// read next four lights
	lights = texture2D( u_VisLightMap1, var_TexCoord ) * 255.0;
	shadowmap = smoothstep( 0.25, 0.75, shadowsrc );

	if( lights[0] != 255.0 )
	{
		ProcessWorldLight( lights[0], shadowmap[0], normal, pos, s_light, styles );
		numLights += 1.0;
	}

	if( lights[1] != 255.0 )
	{
		ProcessWorldLight( lights[1], shadowmap[1], normal, pos, s_light, styles );
		numLights += 1.0;
	}

	if( lights[2] != 255.0 )
	{
		ProcessWorldLight( lights[2], shadowmap[2], normal, pos, s_light, styles );
		numLights += 1.0;
	}

	if( lights[3] != 255.0 )
	{
		ProcessWorldLight( lights[3], shadowmap[3], normal, pos, s_light, styles );
		numLights += 1.0;
	}

	for( int i = 0; i < MAXLIGHTMAPS && styles[i] != 255.0; i++ )
	{
		vec3 lb = ColorNormalize( s_light[i] * u_LightScale, u_LightThreshold );
		s_light[i] = ApplyGamma( lb / 256.0, u_LightGamma, 1.0 );
		lightmap += s_light[i] * u_LightStyleValues[int( styles[i] )];
	}

	// convert values back to normal range and clamp it
	lightmap = min(( lightmap * LIGHTMAP_SHIFT ), 1.0 );

	// apply screen gamma
	gammaIndex = (lightmap.r * 255.0);
	lightmap.r = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	gammaIndex = (lightmap.g * 255.0);
	lightmap.g = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	gammaIndex = (lightmap.b * 255.0);
	lightmap.b = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	diffuse *= lightmap;

#if defined( DEBUG_LIGHTMAP )
	diffuse = lightmap;
#endif

#if defined( LIGHT_COUNT_DEBUG )
	diffuse = mix( vec3( 0.0, 1.0, 0.0 ), vec3( 1.0, 0.0, 0.0 ), numLights / float( MAXDYNLIGHTS ));
#endif
	gl_FragColor = vec4( diffuse, 1.0 );
}