/*
GrassSolid_fp.glsl - fragment uber shader for grass meshes
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

#include "const.h"
#include "mathlib.h"

uniform sampler2D		u_ColorMap;

uniform vec4		u_FogParams;

varying vec2		var_TexDiffuse;
varying vec3		var_VertexLight;

void main( void )
{
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );

#if !defined( GRASS_FULLBRIGHT )
	diffuse.rgb *= var_VertexLight;
#endif//GRASS_FULLBRIGHT

#if defined( GRASS_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
#endif
	gl_FragColor = diffuse;
}