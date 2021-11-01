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

void main( void )
{
	vec2 sunVec = saturate( u_LightOrigin.xy ) - var_TexCoord;
	vec2 tc = var_TexCoord;

	float sunDist = saturate( u_LightOrigin.z ) * saturate( 1.0 - saturate( length( sunVec ) * 0.2 ));
	sunVec *= 0.1 * u_LightOrigin.z;

	vec4 accum = texture2D( u_ColorMap, tc );
	float fShaftsMask = saturate( 1.00001 - accum.w );
	float fBlend = accum.w * 0.85;	// shaft brightness

	float weight_sum = 1.0;
	for (float i = 1.0; i < 32.0; i++) {
		vec2 offset = sunVec.xy * 0.25 * i;
		float weight = 1.0 / max(length(offset), 0.00001);
		accum += texture2D( u_ColorMap, tc + offset ).rgba * weight;
		weight_sum += weight;	
	}
	accum /= weight_sum;

 	accum *= 2.0 * vec4( sunDist, sunDist, sunDist, 1.0 );
	
 	vec3 cScreen = texture2D( u_ScreenMap, var_TexCoord ).rgb;      
	vec3 sunColor = u_LightDiffuse;

	vec3 outColor = cScreen + accum.rgb * fBlend * sunColor * max(1.0 - cScreen, 0.0);

	gl_FragColor = vec4( outColor, 1.0 );
}