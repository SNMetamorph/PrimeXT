/*
scene_grass_fp.glsl - fragment uber shader for grass meshes
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
#include "texfetch.h"
#include "fog.h"

uniform sampler2D	u_ColorMap;
uniform vec4		u_FogParams;
uniform vec3		u_ViewOrigin;

varying vec3		var_Position;
varying vec2		var_TexDiffuse;
varying vec3		var_FrontLight;
varying vec3		var_BackLight;

void main( void )
{
	vec4 diffuse = colormap2D( u_ColorMap, var_TexDiffuse );

#if !defined( LIGHTING_FULLBRIGHT )
#if defined( LIGHTMAP_DEBUG ) || defined( LIGHTVEC_DEBUG )
	if( bool( gl_FrontFacing ))
		diffuse.rgb = var_FrontLight;
	else diffuse.rgb = var_BackLight;
#else
	if( bool( gl_FrontFacing ))
		diffuse.rgb *= var_FrontLight;
	else diffuse.rgb *= var_BackLight;
#endif
#endif//LIGHTING_FULLBRIGHT

#if defined( APPLY_FOG_EXP )
	diffuse.rgb = CalculateFog(diffuse.rgb, u_FogParams, gl_FragCoord.z / gl_FragCoord.w);
#endif
	gl_FragColor = diffuse;
}