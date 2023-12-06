/*
shadow_proj.h - shadow from directional source
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef SHADOW_PROJ_H
#define SHADOW_PROJ_H

uniform sampler2DShadow	u_ShadowMap0;
uniform sampler2DShadow	u_ShadowMap1;
uniform sampler2DShadow	u_ShadowMap2;
uniform sampler2DShadow	u_ShadowMap3;

uniform mat4		u_ShadowMatrix[MAX_SHADOWMAPS];
uniform vec4		u_ShadowSplitDist;
uniform vec4		u_TexelSize;	// shadowmap resolution

#include "mathlib.h"

vec3 WorldToTexel( vec3 world, int split )
{
	vec4 pos = u_ShadowMatrix[split] * vec4( world, 1.0 );
	vec3 coord = vec3( pos.xyz / ( pos.w + 0.0005 )); // z-bias
	coord.x = float( clamp( float( coord.x ), u_TexelSize[split], 1.0 - u_TexelSize[split] ));
	coord.y = float( clamp( float( coord.y ), u_TexelSize[split], 1.0 - u_TexelSize[split] ));
	coord.z = float( clamp( float( coord.z ), 0.0, 1.0 ));

	return coord;
}

float ShadowProj( const in vec3 world )
{
	float shadow = 0.0;

	// transform to camera space
	vec4 cam = gl_ModelViewMatrix * vec4( world.xyz, 1.0 );
	float vertexDistanceToCamera = -cam.z;
	vec3 shadowVert;

#if defined( SHADOW_VOGEL_DISK ) // vogel disk shadows smoothing
#define SAMPLE_COUNT 16
	float stepSize = 6.4 / 2560.0;		
	float rotation = InterleavedGradientNoise(gl_FragCoord.xy) * 6.2832;
	
#if (NUM_SHADOW_SPLITS == 1)
	if( vertexDistanceToCamera < u_ShadowSplitDist.x )
	{
		shadowVert = WorldToTexel( world, 0 );
        for( int i = 0; i < SAMPLE_COUNT; i += 1 )
	    {
	 	    shadow += texture( u_ShadowMap0, shadowVert + vec3(stepSize * VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
	    }
	}
	else
	{
		shadowVert = WorldToTexel( world, 1 );
        for( int i = 0; i < SAMPLE_COUNT; i += 1 )
		{
			shadow += texture( u_ShadowMap1, shadowVert + vec3(stepSize * 0.5 * VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
		}         
	}
#elif (NUM_SHADOW_SPLITS == 2)
	if( vertexDistanceToCamera < u_ShadowSplitDist.x )
	{
		shadowVert = WorldToTexel( world, 0 );
		for( int i = 0; i < SAMPLE_COUNT; i += 1 )
		{
			shadow += texture( u_ShadowMap0, shadowVert + vec3(stepSize * VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
		}
	}
	else if( vertexDistanceToCamera < u_ShadowSplitDist.y )
	{
		shadowVert = WorldToTexel( world, 1 );
		for( int i = 0; i < SAMPLE_COUNT; i += 1 )
		{
			shadow += texture( u_ShadowMap1, shadowVert + vec3(stepSize * 0.5 * VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
		}
		
	}
	else
	{
		shadowVert = WorldToTexel( world, 2 );
		for( int i = 0; i < SAMPLE_COUNT; i += 1 )
		{
			shadow += texture( u_ShadowMap2, shadowVert + vec3(stepSize * 0.25 * VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
		}
	}
#elif (NUM_SHADOW_SPLITS == 3)
	if( vertexDistanceToCamera < u_ShadowSplitDist.x )
	{
		shadowVert = WorldToTexel( world, 0 );
		for( int i = 0; i < SAMPLE_COUNT; i += 1 )
		{
			shadow += texture( u_ShadowMap0, shadowVert + vec3(stepSize * VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
		}
	}
	else if( vertexDistanceToCamera < u_ShadowSplitDist.y )
	{
		shadowVert = WorldToTexel( world, 1 );
		for( int i = 0; i < SAMPLE_COUNT; i += 1 )
		{
			shadow += texture( u_ShadowMap1, shadowVert + vec3(stepSize * 0.5 * VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
		}
	}
	else if( vertexDistanceToCamera < u_ShadowSplitDist.z )
	{
		shadowVert = WorldToTexel( world, 2 );
		for( int i = 0; i < SAMPLE_COUNT; i += 1 )
		{
			shadow += texture( u_ShadowMap2, shadowVert + vec3(stepSize * 0.25 *  VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
		}
	}
	else
	{
		shadowVert = WorldToTexel( world, 3 );
		for( int i = 0; i < SAMPLE_COUNT; i += 1 )
		{
			shadow += texture( u_ShadowMap3, shadowVert + vec3(stepSize * 0.125 * VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
		}
	}
#else // no shadow splits at all
	{
		shadowVert = WorldToTexel( world, 0 );
		for( int i = 0; i < SAMPLE_COUNT; i += 1 )
		{
			shadow += texture( u_ShadowMap0, shadowVert + vec3(stepSize * VogelDiskSample( i, SAMPLE_COUNT, rotation ), 0.0));
		}
	}
#endif			
	shadow /= float(SAMPLE_COUNT);	
#else // PCF shadows smoothing 
#if (NUM_SHADOW_SPLITS == 1)
	if( vertexDistanceToCamera < u_ShadowSplitDist.x )
	{
		shadowVert = WorldToTexel( world, 0 );
		shadow = texture( u_ShadowMap0, shadowVert.xyz ) * 0.25;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(-1.0, -1.0, 0.0) * u_TexelSize[0]) * 0.0625;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(-1.0, 0.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec2(-1.0, 1.0, 0.0) * u_TexelSize[0]) * 0.0625;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(0.0, -1.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(0.0, 1.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(1.0, -1.0, 0.0) * u_TexelSize[0]) * 0.0625;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(1.0, 0.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(1.0, 1.0, 0.0) * u_TexelSize[0]) * 0.0625;
	}
	else
	{
		shadowVert = WorldToTexel( world, 1 );
		shadow = texture( u_ShadowMap1, shadowVert.xyz );
	}
#elif (NUM_SHADOW_SPLITS == 2)
	if( vertexDistanceToCamera < u_ShadowSplitDist.x )
	{
		shadowVert = WorldToTexel( world, 0 );
		shadow = texture( u_ShadowMap0, shadowVert.xyz ) * 0.25;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(-1.0, -1.0, 0.0) * u_TexelSize[0]) * 0.0625;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(-1.0, 0.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec2(-1.0, 1.0, 0.0) * u_TexelSize[0]) * 0.0625;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(0.0, -1.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(0.0, 1.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(1.0, -1.0, 0.0) * u_TexelSize[0]) * 0.0625;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(1.0, 0.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(1.0, 1.0, 0.0) * u_TexelSize[0]) * 0.0625;
	}
	else if( vertexDistanceToCamera < u_ShadowSplitDist.y )
	{
		shadowVert = WorldToTexel( world, 1 );
		shadow = texture( u_ShadowMap1, shadowVert.xyz ).;
		
	}
	else
	{
		shadowVert = WorldToTexel( world, 2 );
		shadow = texture( u_ShadowMap2, shadowVert.xyz );
	}
#elif (NUM_SHADOW_SPLITS == 3)
	if( vertexDistanceToCamera < u_ShadowSplitDist.x )
	{
		shadowVert = WorldToTexel( world, 0 );
		shadow = texture( u_ShadowMap0, shadowVert.xyz ) * 0.25;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(-1.0, -1.0, 0.0) * u_TexelSize[0]) * 0.0625;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(-1.0, 0.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec2(-1.0, 1.0, 0.0) * u_TexelSize[0]) * 0.0625;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(0.0, -1.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(0.0, 1.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(1.0, -1.0, 0.0) * u_TexelSize[0]) * 0.0625;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(1.0, 0.0, 0.0) * u_TexelSize[0]) * 0.125;
		shadow += texture( u_ShadowMap0, shadowVert.xyz + vec3(1.0, 1.0, 0.0) * u_TexelSize[0]) * 0.0625;
	}
	else if( vertexDistanceToCamera < u_ShadowSplitDist.y )
	{
		shadowVert = WorldToTexel( world, 1 );
		shadow = texture( u_ShadowMap1, shadowVert.xyz );
	}
	else if( vertexDistanceToCamera < u_ShadowSplitDist.z )
	{
		shadowVert = WorldToTexel( world, 2 );
		shadow = texture( u_ShadowMap2, shadowVert.xyz );
	}
	else
	{
		shadowVert = WorldToTexel( world, 3 );
		shadow = texture( u_ShadowMap3, shadowVert.xyz );
	}
#else // no shadow splits at all
	{
		shadowVert = WorldToTexel( world, 0 );
		shadow = texture( u_ShadowMap0, shadowVert.xyz );
	}
#endif // NUM_SHADOW_SPLITS
#endif // SHADOW_VOGEL_DISK
	return shadow;
}

#endif//SHADOW_PROJ_H