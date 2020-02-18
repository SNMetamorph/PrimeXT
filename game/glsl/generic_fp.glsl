/*
generic_fp.glsl - generic shader
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

#if defined( GENERIC_FOG_EXP )
uniform vec4		u_FogParams;
#endif

varying vec2	var_TexCoord;
varying vec3	var_VertexColor;

void main( void )
{
	vec4 diffuse = texture2D( u_ColorMap, var_TexCoord );

#if defined( GENERIC_FOG_EXP )
	diffuse.rgb *= var_VertexColor;
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = Q_mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
#endif
	gl_FragColor = diffuse;
}