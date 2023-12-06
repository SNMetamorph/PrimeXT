/*
texfetch.h - textures fetching
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef TEXFETCH_H
#define TEXFETCH_H

#include "const.h"
#include "mathlib.h"

vec4 detailmap2D(sampler2D tex, vec2 uv)
{
	return texture(tex, uv);
}

vec4 decalmap2D(sampler2D tex, vec2 uv)
{
	return texture(tex, uv);
}

vec4 colormap2DArray(sampler2DArray tex, vec2 uv, float layer)
{
	vec4 sample = texture(tex, vec3(uv, layer));
	return vec4(ConvertSRGBToLinear(sample.rgb), sample.a);
}

vec4 colormap2D(sampler2D tex, vec2 uv)
{
	vec4 sample = texture(tex, uv);
	return vec4(ConvertSRGBToLinear(sample.rgb), sample.a);
}

vec4 reflectmap2D( sampler2D tex, vec4 projTC, vec3 N, vec3 fragCoord, float refraction )
{
#if defined( APPLY_REFRACTION )
#if defined( LIQUID_SURFACE )
	projTC.x += N.x * refraction * fragCoord.x;
	projTC.y -= N.y * refraction * fragCoord.y;
	projTC.z += N.z * refraction * fragCoord.x;
#else
	projTC.x += N.x * refraction * 2.0;
	projTC.y -= N.y * refraction * 2.0;
	projTC.z += N.z * refraction * 2.0;
#endif
#endif
	return textureProj( tex, projTC );
}

vec3 chromemap2D( sampler2D tex, vec2 screenCoord, vec3 N, float aberration )
{
	vec2 screenCoord2 = screenCoord;
	vec3 screenmap;

	screenCoord2.x += N.z * aberration * screenCoord.x;
	screenCoord2.y -= N.x * aberration * screenCoord.y;
	screenCoord2.x = clamp( screenCoord2.x, 0.01, 0.99 );
	screenCoord2.y = clamp( screenCoord2.y, 0.01, 0.99 );
	screenmap.r = texture( tex, screenCoord2 ).r;

	screenCoord2 = screenCoord;
	screenCoord2.x += N.y * aberration * screenCoord.x;
	screenCoord2.y -= N.z * aberration * screenCoord.y;
	screenCoord2.x = clamp( screenCoord2.x, 0.01, 0.99 );
	screenCoord2.y = clamp( screenCoord2.y, 0.01, 0.99 );
	screenmap.g = texture( tex, screenCoord2 ).g;

	screenCoord2 = screenCoord;
	screenCoord2.x -= N.z * aberration * screenCoord.x;
	screenCoord2.y += N.x * aberration * screenCoord.y;
	screenCoord2.x = clamp( screenCoord2.x, 0.01, 0.99 );
	screenCoord2.y = clamp( screenCoord2.y, 0.01, 0.99 );
	screenmap.b = texture( tex, screenCoord2 ).b;

	return screenmap;
}

vec4 lightmap2D(sampler2D tex, const vec2 uv, float lightmapGamma)
{
	vec4 sample = texture(tex, uv);
	return vec4(pow(sample.rgb, vec3(1.0 / lightmapGamma)), sample.a);
}

vec3 deluxemap2D( sampler2D tex, const vec2 uv )
{
	vec3 deluxmap = texture( tex, uv ).xyz;
	return (( deluxmap - 0.5 ) * 2.0 );
}

// get support for various normalmap encode
vec3 normalmap2D( sampler2D tex, const vec2 uv )
{
	vec4 normalmap = texture( tex, uv );
	vec3 N;

#if defined( NORMAL_AG_PARABOLOID )
	N.x = 2.0 * ( normalmap.a - 0.5 );
	N.y = 2.0 * ( normalmap.g - 0.5 );
	N.z = 1.0 - saturate( dot( N.xy, N.xy ));
#elif defined( NORMAL_RG_PARABOLOID )
	N.x = 2.0 * ( normalmap.g - 0.5 );
	N.y = 2.0 * ( normalmap.r - 0.5 );
	N.z = 1.0 - saturate( dot( N.xy, N.xy ));
#elif defined( NORMAL_3DC_PARABOLOID )
	N.x = -2.0 * ( normalmap.g - 0.5 );
	N.y = -2.0 * ( normalmap.r - 0.5 );
	N.z = 1.0 - saturate( dot( N.xy, N.xy ));
#else
	// uncompressed normals
	N = ( 2.0 * ( normalmap.xyz - 0.5 ));
#endif
	N.y = -N.y; // !!!
	N = normalize( N );

	return N;
}

vec3 normalmap2DLod( sampler2D tex, const vec2 uv, float lod )
{
	vec4 normalmap = textureLod( tex, uv, lod );
	vec3 N;

#if defined( NORMAL_AG_PARABOLOID )
	N.x = 2.0 * ( normalmap.a - 0.5 );
	N.y = 2.0 * ( normalmap.g - 0.5 );
	N.z = 1.0 - saturate( dot( N.xy, N.xy ));
#elif defined( NORMAL_RG_PARABOLOID )
	N.x = 2.0 * ( normalmap.g - 0.5 );
	N.y = 2.0 * ( normalmap.r - 0.5 );
	N.z = 1.0 - saturate( dot( N.xy, N.xy ));
#elif defined( NORMAL_3DC_PARABOLOID )
	N.x = -2.0 * ( normalmap.g - 0.5 );
	N.y = -2.0 * ( normalmap.r - 0.5 );
	N.z = 1.0 - saturate( dot( N.xy, N.xy ));
#else
	// uncompressed normals
	N = ( 2.0 * ( normalmap.xyz - 0.5 ));
#endif
	N.y = -N.y; // !!!
	N = normalize( N );

	return N;
}

vec3 normalmap2DArray( sampler2DArray tex, const vec2 uv, const float layer )
{
	vec4 normalmap = texture( tex, vec3( uv, layer ));
	vec3 N;

#if defined( NORMAL_AG_PARABOLOID )
	N.x = 2.0 * ( normalmap.a - 0.5 );
	N.y = 2.0 * ( normalmap.g - 0.5 );
	N.z = 1.0 - saturate( dot( N.xy, N.xy ));
#elif defined( NORMAL_RG_PARABOLOID )
	N.x = 2.0 * ( normalmap.g - 0.5 );
	N.y = 2.0 * ( normalmap.r - 0.5 );
	N.z = 1.0 - saturate( dot( N.xy, N.xy ));
#elif defined( NORMAL_3DC_PARABOLOID )
	N.x = -2.0 * ( normalmap.g - 0.5 );
	N.y = -2.0 * ( normalmap.r - 0.5 );
	N.z = 1.0 - saturate( dot( N.xy, N.xy ));
#else
	// uncompressed normals
	N = ( 2.0 * ( normalmap.xyz - 0.5 ));
#endif
	N.y = -N.y; // !!!
	N = normalize( N );

	return N;
}

vec4 cubic(float x)
{
    float x2 = x * x;	
	mat4 mat = mat4(vec4(-0.166667 , 0.5 , -0.5 , 0.166667), 
					vec4(0.5 , -1.0 , 0.5 , 0.0), 
					vec4(-0.5 , 0.0 , 0.5 , 0.0), 
					vec4( 0.166667 , 0.666667 , 0.166667 , 0.0 ));	
	vec4 xx = vec4( x2 * x , x2 , x , 1.0);	
    return  mat * xx;
}

vec4 textureBicubic(sampler2D sampler, vec2 texCoords, float LOD, vec2 texSize)
{		
   vec2 pixelSize = vec2(1.0) / texSize;   
   
   texCoords = texCoords * texSize - vec2(0.5);
   
    vec2 fxy = fract(texCoords);
    texCoords -= fxy;

    vec4 xcubic = cubic(fxy.x);
    vec4 ycubic = cubic(fxy.y);

    vec4 c = texCoords.xxyy + vec2(-0.5, +1.5).xyxy;
    
    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
    vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;
    
    offset *= pixelSize.xxyy;
    
    vec4 sample0 = textureLod(sampler, offset.xz, LOD);
    vec4 sample1 = textureLod(sampler, offset.yz, LOD);
    vec4 sample2 = textureLod(sampler, offset.xw, LOD);
    vec4 sample3 = textureLod(sampler, offset.yw, LOD);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix(
       mix(sample3, sample2, sx), mix(sample1, sample0, sx)
    , sy);
}

#endif//TEXFETCH_H