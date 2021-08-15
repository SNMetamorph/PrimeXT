/*
parallax.h - parallax occlusion mapping
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

#ifndef PARALLAX_H
#define PARALLAX_H

#define PARALLAX_STEPS	15.0

uniform sampler2D	u_HeightMap;
uniform vec4	u_ReliefParams;

float ComputeLOD( const vec2 tc )
{ 
	vec2 dx = dFdx( tc ); 
	vec2 dy = dFdy( tc ); 
	vec2 mag = (abs( dx ) + abs( dy )) * u_ReliefParams.xy; 
	float lod = log2( max( mag.x, mag.y )); 

	return lod; 
} 

vec2 OffsetMap( sampler2D normalMap, const in vec2 texCoord, const in vec3 viewVec )
{
	float bumpScale = u_ReliefParams.z * 0.1;
	vec3 newCoords = vec3( texCoord, 0 );
	float lod = ComputeLOD( texCoord );

	for( int i = 0; i < int( PARALLAX_STEPS ); i++ )
	{
		float nz = normalmap2D( normalMap, newCoords.xy ).z;
		float h = 1.0 - texture2DLod( u_HeightMap, newCoords.xy, lod ).r;
		float height = h * bumpScale;
		newCoords += (height - newCoords.z) * nz * viewVec;
	}

	return newCoords.xy;
}

vec2 ReliefMapping( const in vec2 texCoord, const in vec3 viewVec )
{
	// 14 sample relief mapping: linear search and then binary search, 
	float lod = ComputeLOD( texCoord );
	vec3 offsetBest;

	vec2 scaleVector = vec2( 1.0 / u_ReliefParams.x, 1.0 / u_ReliefParams.y );
	vec3 offsetVector = vec3( viewVec.xy * scaleVector * vec2( u_ReliefParams.z * 100.0 ), -1.0 );
	offsetBest = vec3( texCoord, 1.0 );
	offsetVector *= 0.1;

	offsetBest += offsetVector *  step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z );
	offsetBest += offsetVector *  step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z );
	offsetBest += offsetVector *  step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z );
	offsetBest += offsetVector *  step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z );
	offsetBest += offsetVector *  step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z );
	offsetBest += offsetVector *  step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z );
	offsetBest += offsetVector *  step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z );
	offsetBest += offsetVector *  step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z );
	offsetBest += offsetVector *  step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z );
	offsetBest += offsetVector * (step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z ) - 0.5);
	offsetBest += offsetVector * (step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z ) * 0.5 - 0.25);
	offsetBest += offsetVector * (step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z ) * 0.25 - 0.125);
	offsetBest += offsetVector * (step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z ) * 0.125 - 0.0625);
	offsetBest += offsetVector * (step( texture2DLod( u_HeightMap, offsetBest.xy, lod ).r, offsetBest.z ) * 0.0625 - 0.03125);

	return offsetBest.xy;  
}

vec3 ParallaxOcclusionMap( const in vec2 texCoord, const in vec3 viewVec )
{
	float step =  1.0 / PARALLAX_STEPS;
	float bumpScale = u_ReliefParams.z * 0.1;
	float lod = ComputeLOD( texCoord );

	vec2 delta = vec2( viewVec.x, viewVec.y ) * bumpScale / (viewVec.z * PARALLAX_STEPS);
	float NB0 = texture2D( u_HeightMap, texCoord ).r;
	float height = 1.0 - step;
	vec2 offset = texCoord + delta;
	
	float NB1 = texture2D( u_HeightMap, offset ).r;

	for( int i = 0; i < int( PARALLAX_STEPS ); i++ )
	{
		if( NB1 >= height )
			break;

		NB0 = NB1;
		height -= step;
		offset += delta;
		NB1 = texture2DLod( u_HeightMap, offset, lod ).r;
	}

	vec2 offsetBest = offset;
	float error = 1.0;
	float t1 = height;
	float t0 = t1 + step;
	float delta1 = t1 - NB1;
	float delta0 = t0 - NB0;

	vec4 intersect = vec4( delta * PARALLAX_STEPS, delta * PARALLAX_STEPS + texCoord );

	float t = 0;

	for( int i = 0; i < 10; i++ )
	{
		if( abs( error ) <= 0.01 )
			break;

		float denom = delta1 - delta0;
		t = (t0 * delta1 - t1 * delta0) / denom;
		offsetBest = -t * intersect.xy + intersect.zw;

		float NB = texture2DLod( u_HeightMap, offsetBest, lod ).r;

		error = t - NB;
		if( error < 0.0 )
		{
			delta1 = error;
			t1 = t;
		}
		else
		{
			delta0 = error;
			t0 = t;
		}
	}

	return vec3( offsetBest, t );
}

float SelfShadow( in vec3 N, in vec3 L, in vec2 texCoord )
{
	vec2	delta;
	float	steps, height;
	float	NL = saturate( dot( N, L ));
   
	if( NL <= 0.0 ) return 1.0;
	float lod = ComputeLOD( texCoord );
	if( lod > 8.0 ) return 1.0;

	float tex = texture2DLod( u_HeightMap, texCoord, lod ).r;
	if( tex >= 0.9 ) return 1.0;

	vec2 offsetCoord = texCoord;
	float numShadowSteps = mix( 60.0, 5.0, -L.z );
	steps = 1.0 / numShadowSteps;
	delta = L.xy / L.z * u_ReliefParams.w / numShadowSteps;
	height = tex + steps * 0.1;

	while(( tex < height ) && ( height < 1.0 ))
	{
		height += steps;
		offsetCoord += delta;
		tex = texture2DLod( u_HeightMap, offsetCoord, lod ).r;
            }

	return clamp(( height - tex ) * 8.0, 0.0, 1.0 );
}

#endif//PARALLAX_H