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

vec3 blendSoftLight( const vec3 a, const vec3 b )
{
	vec3 c = 2.0 * a * b * ( 1.0 + a * (  1.0 - b ) );
 
	vec3 a_sqrt = sqrt( a );
	vec3 d = ( a  +  b * ( a_sqrt - a )) * 2.0 - a_sqrt;

	return( length( b ) < 0.5 ) ? c : d;
}

void main( void )
{
	vec2 sunVec = saturate( u_LightOrigin.xy ) - var_TexCoord;
	vec2 tc = var_TexCoord;

	float sunDist = saturate( u_LightOrigin.z ) * saturate( 1.0 - saturate( length( sunVec ) * 0.2 ));
	sunVec *= 0.1 * u_LightOrigin.z;

	vec4 accum = texture2D( u_ColorMap, tc );
	float fShaftsMask = saturate( 1.00001 - accum.w );
	float fBlend = accum.w * 1.5;	// shaft brightness

	accum += texture2D( u_ColorMap, tc + sunVec.xy * 1.0 ) * (1.0 - 1.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + sunVec.xy * 2.0 ) * (1.0 - 2.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + sunVec.xy * 3.0 ) * (1.0 - 3.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + sunVec.xy * 4.0 ) * (1.0 - 4.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + sunVec.xy * 5.0 ) * (1.0 - 5.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + sunVec.xy * 6.0 ) * (1.0 - 6.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + sunVec.xy * 7.0 ) * (1.0 - 7.0 / 8.0);
	accum /= 8.0;

 	accum *= 2.0 * vec4( sunDist, sunDist, sunDist, 1.0 );
	
 	vec3 cScreen = texture2D( u_ScreenMap, var_TexCoord ).rgb;      
	vec3 sunColor = u_LightDiffuse;

	vec3 outColor = cScreen + accum.rgb * fBlend * sunColor * ( 1.0 - cScreen );
	outColor = blendSoftLight( outColor, sunColor * fShaftsMask * 0.5 + 0.5 );

	gl_FragColor = vec4( outColor, 1.0 );
}