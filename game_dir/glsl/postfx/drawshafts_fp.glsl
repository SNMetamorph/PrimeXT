/*
drawshafts_fp.glsl - render sun shafts
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

#include "const.h"
#include "mathlib.h"

uniform sampler2D		u_ScreenMap;
uniform sampler2D		u_ColorMap;
uniform vec3		u_LightOrigin;
uniform vec3		u_LightDiffuse;

varying vec2       		var_TexCoord;

vec3 BlendSoftLight(const vec3 a, const vec3 b)
{
	vec3 c = 2.0 * a * b * ( 1.0 + a * ( 1.0 - b ) );
	vec3 a_sqrt = sqrt( max(a, 0.0001) );
	vec3 d = ( a + b * ( a_sqrt - a )) * 2.0 - a_sqrt;
	return( length( b ) < 0.5 ) ? c : d;
}

void main( void )
{
	vec2 tc = var_TexCoord;
	vec3 sunColor = u_LightDiffuse;
	vec2 sunVec = saturate( u_LightOrigin.xy ) - tc;
	float sunDist = saturate( u_LightOrigin.z ) * saturate( 1.0 - saturate( length( sunVec ) * 0.2 ));
	sunVec *= 0.1 * u_LightOrigin.z;

	vec4 colorMapSample = texture( u_ColorMap, tc );
	float fShaftsMask = saturate( 1.0001 - colorMapSample.w );
	float fBlend = colorMapSample.w * 0.85;	// shaft brightness
	float weight_sum = 1.0;
	vec3 accum = colorMapSample.rgb;

	for (float i = 1.0; i < 32.0; i++) 
	{
		vec2 offset = sunVec.xy * 0.25 * i;
		float weight = 1.0 / max(length(offset), 0.0001);
		accum += texture( u_ColorMap, tc + offset ).rgb * weight;
		weight_sum += weight;	
	}

	accum /= weight_sum;
 	accum *= 2.0 * sunDist;
	
 	vec3 cScreen = texture( u_ScreenMap, tc ).rgb;
	vec3 outColor = cScreen + accum.rgb * fBlend * sunColor * (1.0 - cScreen);
	outColor = BlendSoftLight( outColor, sunColor * fShaftsMask * 0.5 + 0.5 );
	gl_FragColor = vec4( outColor, 1.0 );
}
