/*
shadow_spot.h - shadow for projection dlights and PCF filtering
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

#ifndef SHADOW_SPOT_H
#define SHADOW_SPOT_H

#define NUM_SAMPLES		4.0

uniform sampler2DShadow	u_ShadowMap;

float ShadowSpot( const in vec4 projection, const in vec2 texel )
{
	vec3 coord = vec3( projection.xyz / ( projection.w + 0.0005 )); // z-bias

	coord.s = float( clamp( float( coord.s ), texel.x, 1.0 - texel.x ));
	coord.t = float( clamp( float( coord.t ), texel.y, 1.0 - texel.y ));
	coord.r = float( clamp( float( coord.r ), 0.0, 1.0 ));

#if defined( SHADOW_PCF2X2 ) || defined( SHADOW_PCF3X3 )
#if defined( SHADOW_PCF2X2 )
	float filterWidth = texel.x * length( coord ) * 0.5;
#elif defined( SHADOW_PCF3X3 )
	float filterWidth = texel.x * length( coord ) * 0.75;
#else	// dead code
	float filterWidth = texel.x * length( coord ) * 0.25;
#endif
	// compute step size for iterating through the kernel
	float stepSize = NUM_SAMPLES * ( filterWidth / NUM_SAMPLES );
	float shadow = 0.0;

	for( float i = -filterWidth; i < filterWidth; i += stepSize )
	{
		for( float j = -filterWidth; j < filterWidth; j += stepSize )
		{
			shadow += texture( u_ShadowMap, coord + vec3( i, j, -0.000005 ));
		}
	}

	// return average of the samples
	shadow *= ( 4.0 / ( NUM_SAMPLES * NUM_SAMPLES ));
	return shadow;
#elif defined( SHADOW_VOGEL_DISK )
	float shadow = 0.0;	
	float stepSize = 6.4 / 3200.0;	
	float rotation = InterleavedGradientNoise(gl_FragCoord.xy) * 6.2832;
	
	for( int i = 0; i < 16; ++i )
	{
		shadow += texture(u_ShadowMap, coord + vec3(stepSize * VogelDiskSample(i, 16, rotation), 0.0));
	}
	
    shadow *= 0.0625;	
	return shadow;
#else
	return texture( u_ShadowMap, coord ); // no PCF
#endif
}

#endif//SHADOW_SPOT_H