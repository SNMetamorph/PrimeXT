/*
StudioDiffuse_fp.glsl - draw solid and alpha-test surfaces
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

uniform vec4		u_RenderColor;
uniform vec4		u_FogParams;

varying vec3		var_VertexLight;
varying vec2		var_TexDiffuse;

void main( void )
{
	// compute the diffuse term
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );

	diffuse.rgb *= u_RenderColor.rgb; // kRenderTransColor
	diffuse.rgb *= var_VertexLight; // apply lighting

#if defined( STUDIO_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = Q_mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
//	diffuse.a = Q_mix( 0.0, diffuse.a, fogFactor );
#endif
	// compute final color
	gl_FragColor = vec4( diffuse.rgb, diffuse.a * u_RenderColor.a );
}