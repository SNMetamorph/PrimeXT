/*
shadow_proj.h - shadow for projection dlights and PCF filtering
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

#ifndef SHADOW_PROJ_H
#define SHADOW_PROJ_H

#define NUM_SAMPLES		4.0

#ifdef HAS_ARB_SHADOW
uniform sampler2DShadow	u_ShadowMap;

#define shadowval( center, xoff, yoff ) float( shadow2DOffset( u_ShadowMap, center, ivec2( xoff, yoff )))

float get_shadow_offset( float NdotL )
{
	float bias = 0.0005 * tan( acos( saturate( NdotL )));
	return clamp( bias, 0.0, 0.005 );
}

float ShadowProj( vec4 projection, vec2 texel, float NdotL )
{
	vec3 coord = vec3( projection.xyz / ( projection.w + 0.0005 )); // z-bias
	coord.s = float( clamp( float( coord.s ), texel.x, 1.0 - texel.x ));
	coord.t = float( clamp( float( coord.t ), texel.y, 1.0 - texel.y ));
	coord.r = float( clamp( float( coord.r ), 0.0, 1.0 ));
#if defined( GLSL_gpu_shader4 )
	// this required GL_EXT_gpu_shader4
	vec4 pcf4 = vec4( shadowval( coord, -0.4, 1.0 ), shadowval( coord, -1.0, -0.4 ), shadowval( coord, 0.4, -1.0 ), shadowval( coord, 1.0, 0.4 ));
	return dot( vec4( 0.25 ), pcf4 ); 
#else
	return shadow2D( u_ShadowMap, coord ).r; // non-filtered
#endif
}

#else
uniform sampler2D	u_ShadowMap;

// 1/512.0
#define invSize 0.001953125
#define size 512.0
float myshadow2D( sampler2D u_depthTex, vec3 suv)
{
#ifdef SHADOW_HQ
	vec2 p1 = suv.xy;
	vec2 p2 = suv.xy+vec2(0.0,invSize);
	vec2 p3 = suv.xy+vec2(invSize,0.0);
	vec2 p4 = suv.xy+vec2(invSize);
	float d = texture2D(u_depthTex,p1).r;
	float r = float(d>suv.z);
	d = texture2D(u_depthTex,p2).r;
	float r2 = float(d>suv.z);
	d = texture2D(u_depthTex,p3).r;
	float r3 = float(d>suv.z);
	d = texture2D(u_depthTex,p4).r;
	float r4 = float(d>suv.z);
	p1*=size;
	float a = p1.y-floor(p1.y);
	float b = p1.x-floor(p1.x);
	return mix(mix(r,r2,a),mix(r3,r4,a),b);
#else
	float d = texture2D(u_depthTex,suv.xy).r;
	return float(d>suv.z);
#endif
}

float ShadowProj( vec4 projection, vec2 texel, float NdotL )
{
	vec3 coord = vec3( projection.xyz / ( projection.w + 0.0005 )); // z-bias
	coord.s = float( clamp( float( coord.s ), texel.x, 1.0 - texel.x ));
	coord.t = float( clamp( float( coord.t ), texel.y, 1.0 - texel.y ));
	coord.r = float( clamp( float( coord.r ), 0.0, 1.0 ));

	return myshadow2D( u_ShadowMap, coord );
}
#endif

#endif//SHADOW_PROJ_H