/*
GenericDlight_vp.glsl - common vertex uber shader for all dlight types
Copyright (C) 2018 Uncle Mike

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
#include "matrix.h"

uniform mat4		u_LightViewProjectionMatrix;
uniform vec4		u_LightOrigin;

varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;

#if defined( GENERIC_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
#if defined( GENERIC_HAS_SHADOWS )
varying vec4		var_ShadowCoord;
#endif
#endif

void main( void )
{
	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;

#if defined( GENERIC_LIGHT_PROJECTION )
	var_ProjCoord = ( Mat4Texture( -0.5 ) * u_LightViewProjectionMatrix ) * gl_Vertex;
#if defined( GENERIC_HAS_SHADOWS )
	var_ShadowCoord = ( Mat4Texture( 0.5 ) * u_LightViewProjectionMatrix ) * gl_Vertex;
#endif
#endif
	var_LightVec = ( u_LightOrigin.xyz - gl_Vertex.xyz );
	var_TexDiffuse = gl_MultiTexCoord0.xy;
}