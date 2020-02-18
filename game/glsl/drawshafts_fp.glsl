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

uniform sampler2D		u_ColorMap;
uniform sampler2D		u_DepthMap;
uniform vec3		u_LightOrigin;
uniform vec3		u_LightDiffuse;
uniform float		u_zFar;

varying vec2       		var_TexCoord;
varying vec3 		var_Normal;

void main( void )
{
	vec2 sunVec = saturate( u_LightOrigin.xy ) - var_TexCoord;
	vec2 tc = var_TexCoord;

	float sunDist = saturate( u_LightOrigin.z ) * saturate( 1.0 - saturate( length( sunVec ) * 0.2 ));
	sunVec *= 0.1 * u_LightOrigin.z;

	vec4 accum = texture2D( u_DepthMap, tc );
	float fBlend = accum.w;

	tc += sunVec.xy;
	accum += texture2D( u_DepthMap, tc ) * 1.0;
	tc += sunVec.xy;
	accum += texture2D( u_DepthMap, tc ) * 0.875;
	tc += sunVec.xy;
	accum += texture2D( u_DepthMap, tc ) * 0.75;
	tc += sunVec.xy;
	accum += texture2D( u_DepthMap, tc ) * 0.625;
	tc += sunVec.xy;
	accum += texture2D( u_DepthMap, tc ) * 0.5;
	tc += sunVec.xy;
	accum += texture2D( u_DepthMap, tc ) * 0.375;
	tc += sunVec.xy;
	accum += texture2D( u_DepthMap, tc ) * 0.25;
	tc += sunVec.xy;
	accum += texture2D( u_DepthMap, tc ) * 0.125;

	accum /= 8.0;

 	accum *= 2.0 * vec4( sunDist, sunDist, sunDist, 1.0 );
	
 	vec4 cScreen = texture2D( u_ColorMap, var_TexCoord );      

	vec4 sunColor = vec4( u_LightDiffuse, 1.0 );

	gl_FragColor = cScreen + accum.xyzz * fBlend * sunColor * ( 1.0 - cScreen );
}