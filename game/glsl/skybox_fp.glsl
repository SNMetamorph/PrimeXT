/*
skybox_fp.glsl - draw sun & skycolor
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

uniform vec3		u_LightDir;
uniform vec3		u_LightDiffuse;
uniform vec3		u_ViewOrigin;
uniform vec4		u_FogParams;

varying vec4		var_Vertex;
varying vec2		var_TexCoord;

void main( void )
{
	vec4 sky_color = texture2D( u_ColorMap, var_TexCoord );
	
	vec3 eye = normalize( u_ViewOrigin - var_Vertex.xyz );

	vec3 sun = normalize( -u_LightDir );
		
	float day_factor = max( sun.z, 0.0 ) + 0.1;

	float dotv = max( -dot( eye, sun ), 0.0 );

	vec4 sun_color = vec4( u_LightDiffuse * 3.0, 1.0 );

	float pow_factor = day_factor * 512.0;

	float sun_factor = clamp( pow( dotv, 1536.0 ), 0.0, 1.0 );	// keep sun constant size

	// under horizon line
	if( sun.z < -0.5 ) sun_factor = 0.0;
#ifdef SKYBOX_DAYTIME
	sky_color *= day_factor;
#endif
	vec4 diffuse = sky_color + sun_color * sun_factor;

	if( bool( u_FogParams.w != 0.0 ))
	{
		float fogFactor = saturate( exp2(( -u_FogParams.w * 0.5 ) * ( gl_FragCoord.z / gl_FragCoord.w )));
		diffuse.rgb = Q_mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
	}

	gl_FragColor = diffuse;
}