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

float GetHeightMapSample(sampler2D heightMap, const in vec2 texCoord)
{
	return texture(heightMap, texCoord).r;
}

float GetHeightMapSampleLOD(sampler2D heightMap, const in vec2 texCoord, float lod)
{
	return textureLod(heightMap, texCoord, lod).r;
}

float GetDepthMapSample(sampler2D heightMap, const in vec2 texCoord)
{
	return 1.0 - GetHeightMapSample(heightMap, texCoord);
}

float GetDepthMapSampleLOD(sampler2D heightMap, const in vec2 texCoord, float lod)
{
	return 1.0 - GetHeightMapSampleLOD(heightMap, texCoord, lod);
}

vec2 ParallaxOffsetMap( sampler2D normalMap, const in vec2 texCoord, const in vec3 viewVec )
{
	float bumpScale = u_ReliefParams.z * 0.1;
	vec3 newCoords = vec3( texCoord, 0 );
	float lod = ComputeLOD( texCoord );

	for( int i = 0; i < int( PARALLAX_STEPS ); i++ )
	{
		float nz = normalmap2D( normalMap, newCoords.xy ).z;
		float h = GetHeightMapSampleLOD(u_HeightMap, newCoords.xy, lod);
		float height = h * bumpScale;
		newCoords += (height - newCoords.z) * nz * vec3(viewVec.x, -viewVec.y, viewVec.z);
	}

	return newCoords.xy;
}

vec2 ParallaxReliefMap( const in vec2 texCoord, const in vec3 viewVec )
{
	// 14 sample relief mapping: linear search and then binary search, 
	float lod = ComputeLOD( texCoord );
	vec2 scaleVector = vec2( 1.0 / u_ReliefParams.x, 1.0 / u_ReliefParams.y );
	vec3 offsetVector = vec3( vec2(viewVec.x, -viewVec.y) * scaleVector * vec2( u_ReliefParams.z * 100.0 ), -1.0 );
	vec3 offsetBest = vec3( texCoord, 1.0 );
	offsetVector *= 0.1;

	offsetBest += offsetVector *  step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z );
	offsetBest += offsetVector *  step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z );
	offsetBest += offsetVector *  step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z );
	offsetBest += offsetVector *  step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z );
	offsetBest += offsetVector *  step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z );
	offsetBest += offsetVector *  step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z );
	offsetBest += offsetVector *  step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z );
	offsetBest += offsetVector *  step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z );
	offsetBest += offsetVector *  step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z );
	offsetBest += offsetVector * (step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z ) - 0.5);
	offsetBest += offsetVector * (step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z ) * 0.5 - 0.25);
	offsetBest += offsetVector * (step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z ) * 0.25 - 0.125);
	offsetBest += offsetVector * (step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z ) * 0.125 - 0.0625);
	offsetBest += offsetVector * (step( GetDepthMapSampleLOD( u_HeightMap, offsetBest.xy, lod ), offsetBest.z ) * 0.0625 - 0.03125);

	return offsetBest.xy;  
}

vec3 ParallaxOcclusionMap( const in vec2 texCoord, const in vec3 viewVec )
{
	float step =  1.0 / PARALLAX_STEPS;
	float bumpScale = 0.2 * u_ReliefParams.z;
	float lod = ComputeLOD( texCoord );

	vec2 delta = bumpScale * vec2(viewVec.x, -viewVec.y) / (viewVec.z * PARALLAX_STEPS);
	float NB0 = GetDepthMapSample(u_HeightMap, texCoord);
	float height = 1.0 - step;
	vec2 offset = texCoord + delta;
	
	float NB1 = GetDepthMapSample(u_HeightMap, offset);

	for( int i = 0; i < int( PARALLAX_STEPS ); i++ )
	{
		if( NB1 >= height )
			break;

		NB0 = NB1;
		height -= step;
		offset += delta;
		NB1 = GetDepthMapSampleLOD(u_HeightMap, offset, lod);
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

		float NB = GetDepthMapSampleLOD(u_HeightMap, offsetBest, lod);

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

float ParallaxSelfShadow( in vec3 N, in vec3 L, in vec2 texCoord )
{
	vec2	delta;
	float	steps, height;
	float	NL = saturate( dot( N, L ));
   
	if( NL <= 0.0 ) return 1.0;
	float lod = ComputeLOD( texCoord );
	if( lod > 8.0 ) return 1.0;

	float tex = GetHeightMapSampleLOD( u_HeightMap, texCoord, lod );
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
		tex = GetHeightMapSampleLOD( u_HeightMap, offsetCoord, lod );
    }

	return clamp(( height - tex ) * 8.0, 0.0, 1.0 );
}

vec2 ParallaxMapSimple(const in vec2 texCoords, const in vec3 viewDir)
{ 
	float heightScale = u_ReliefParams.z * 0.25;
	float offset = texture(u_HeightMap, texCoords).r * heightScale - 0.0075;
	return (offset * vec2(viewDir.x, -viewDir.y) + texCoords);
} 

#endif//PARALLAX_H