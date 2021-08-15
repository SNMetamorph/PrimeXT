/*
light_grass_vp.glsl - vertex uber shader for all dlight types for grass meshes
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
#include "matrix.h"
#include "grassdata.h"

attribute vec4	attr_Position;
attribute vec4	attr_Normal;

uniform mat4	u_ModelMatrix;
uniform mat4	u_LightViewProjMatrix;
uniform vec3	u_GrassParams;
uniform vec4	u_LightOrigin;
uniform vec3	u_ViewOrigin;
uniform float	u_RealTime;

varying vec2	var_TexDiffuse;
varying vec3	var_LightVec;
varying vec3	var_ViewVec;
varying vec3	var_Normal;

#if defined( LIGHT_SPOT )
varying vec4	var_ProjCoord;
#endif

#if defined( LIGHT_SPOT ) || defined( LIGHT_PROJ )
varying vec4	var_ShadowCoord;
#endif

void main( void )
{
#if defined( APPLY_FADE_DIST )
	float dist = distance( u_ViewOrigin, ( u_ModelMatrix * vec4( attr_Position.xyz, 1.0 )).xyz );
	float scale = clamp(( u_GrassParams.z - dist ) / u_GrassParams.y, 0.0, 1.0 );
#else
	float dist = GRASS_ANIM_DIST + 1.0;	// disable animation
	float scale = 1.0;			// keep constant size
#endif
	int vertexID = int( attr_Normal.w ) - int( 4.0 * floor( attr_Normal.w * 0.25 )); // equal to gl_VertexID & 3
	vec4 position = vec4( attr_Position.xyz + normalize( attr_Normal.xyz ) * ( attr_Position.w * scale ), 1.0 );	// in object space

	if( bool( dist < GRASS_ANIM_DIST ) && bool( vertexID == 1 || vertexID == 2 ))
	{
		position.x += sin( position.z + u_RealTime * 0.5 );
		position.y += cos( position.z + u_RealTime * 0.5 );
	}

	vec4 worldpos = u_ModelMatrix * position;
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	var_TexDiffuse = GetTexCoordsForVertex( int( attr_Normal.w ));
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

#if defined( LIGHT_SPOT )
	var_ProjCoord = ( Mat4Texture( -0.5 ) * u_LightViewProjMatrix ) * worldpos;
#if defined( APPLY_SHADOW )
	var_ShadowCoord = ( Mat4Texture( 0.5 ) * u_LightViewProjMatrix ) * worldpos;
#endif
#elif defined( LIGHT_PROJ )
	var_ShadowCoord = worldpos;
#endif
	// these things are in worldspace and not a normalized
	var_Normal = -GetNormalForVertex( int( attr_Normal.w ));
	var_LightVec = ( u_LightOrigin.xyz - worldpos.xyz );
	var_ViewVec = ( u_ViewOrigin.xyz - worldpos.xyz );
}