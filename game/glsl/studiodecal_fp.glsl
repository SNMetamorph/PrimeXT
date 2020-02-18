/*
DecalStudio_fp.glsl - fragment uber shader for studio decals
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

uniform sampler2D	u_DecalMap;
uniform sampler2D	u_ColorMap;

uniform vec4	u_FogParams;

varying vec4	var_VertexColor;

void main( void )
{
	vec4 diffuse = texture2D( u_DecalMap, gl_TexCoord[0].xy );

#if defined( DECAL_ALPHATEST )
	if( texture2D( u_ColorMap, gl_TexCoord[0].zw ).a <= STUDIO_ALPHA_THRESHOLD )
		discard;
#endif

#if defined( DECAL_FOG_EXP )
	float fogFactor = clamp( exp2( -gl_Fog.density * ( gl_FragCoord.z / gl_FragCoord.w )), 0.0, 1.0 );
	diffuse.rgb = Q_mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
	diffuse.a = Q_mix( 0, diffuse.a, fogFactor );
#endif
	gl_FragColor = diffuse * var_VertexColor;
}