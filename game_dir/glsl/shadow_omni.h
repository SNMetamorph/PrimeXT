/*
shadow_omni.h - shadow for omnidirectional dlights and PCF filtering
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

#ifndef SHADOW_OMNI_H
#define SHADOW_OMNI_H

uniform samplerCubeShadow	u_ShadowMap;

float depthCube( const in vec3 coord, const float scale, const float bias )
{
	const float acneRemoveFactor = 1.0;
	float fs_z = max(abs(coord.x), max(abs(coord.y), abs(coord.z))) - acneRemoveFactor;
	float depth = ((fs_z * scale + bias) / fs_z) * 0.5 + 0.5;
	return texture(u_ShadowMap, vec4(coord, depth));
}

float ShadowOmni( const in vec3 I, const in vec4 params )
{
#if defined( SHADOW_PCF2X2 ) || defined( SHADOW_PCF3X3 ) || defined( SHADOW_VOGEL_DISK )
	vec3 forward = normalize(I);
	vec3 right = vec3(0.0);
	vec3 up = vec3(0.0);
	MakeNormalVectors( forward, right, up );

	float shadow = 0.0;
#endif

#if defined( SHADOW_PCF2X2 ) || defined( SHADOW_PCF3X3 )
#if defined( SHADOW_PCF2X2 )
	#define NUM_SAMPLES	4.0
	float filterWidth = params.x * length( I ) * 2.0;	// PCF 2x2
#elif defined( SHADOW_PCF3X3 )
	#define NUM_SAMPLES	4.0
	float filterWidth = params.x * length( I ) * 4.0;	// PCF 4x4
#endif
	// compute step size for iterating through the kernel
	int k = 0;
	float a = filterWidth / 2.0;
	float stepSize = filterWidth / NUM_SAMPLES;
	for (float i = -a; i <= a; i += stepSize)
	{
		for (float j = -a; j <= a; j += stepSize)
		{
			shadow += depthCube( I + right * i + up * j, params.z, params.w );
			k += 1;
		}
	}

	// return average of the samples
	shadow /= k;
	return shadow;
#elif defined( SHADOW_VOGEL_DISK )
	float stepSize = 1.1;
	float rotation = InterleavedGradientNoise(gl_FragCoord.xy) * 6.2832;

	for( int i = 0; i < 16; ++i)
	{
		vec2 vogel = stepSize * VogelDiskSample(i, 16, rotation); 
		shadow += depthCube(I + right * vogel.x + up * vogel.y, params.z, params.w );
	}
	
	shadow *= 0.0625;	
	return shadow;	
#else
	return depthCube( I, params.z, params.w ); // no shadow smoothing at all
#endif
}

#endif//SHADOW_OMNI_H