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

#extension GL_EXT_gpu_shader4 : require

#define NUM_SAMPLES		4.0

uniform samplerCubeShadow	u_ShadowMap;

float depthCube( const in vec3 coord, const float scale, const float bias )
{
	float fs_z = max( abs( coord.x ), max( abs( coord.y ), abs( coord.z ))) - 4.0;	// acne remove factor
	float depth = (( fs_z * scale + bias ) / fs_z ) * 0.5 + 0.5;

	return shadowCube( u_ShadowMap, vec4( coord, depth )).r;
}

float ShadowOmni( const in vec3 I, const in vec4 params )
{
#if defined( SHADOW_PCF2X2 ) || defined( SHADOW_PCF3X3 )
	vec3 forward, right, up;

	forward = normalize( I );
	MakeNormalVectors( forward, right, up );

#if defined( SHADOW_PCF2X2 )
	float filterWidth = params.x * length( I ) * 2.0;	// PCF2X2
#elif defined( SHADOW_PCF3X3 )
	float filterWidth = params.x * length( I ) * 3.0;	// PCF3X3
#else	// dead code
	float filterWidth = params.x * length( I );	 // Hardware PCF1X1
#endif
	// compute step size for iterating through the kernel
	float stepSize = NUM_SAMPLES * filterWidth / NUM_SAMPLES;

	float shadow = 0.0;

	for( float i = -filterWidth; i < filterWidth; i += stepSize )
	{
		for( float j = -filterWidth; j < filterWidth; j += stepSize )
		{
			shadow += depthCube( I + right * i + up * j, params.z, params.w );
		}
	}

	// return average of the samples
	shadow *= ( 4.0 / ( NUM_SAMPLES * NUM_SAMPLES ));

	return shadow;
#else
	return depthCube( I, params.z, params.w ); // no PCF
#endif
}

#endif//SHADOW_OMNI_H