/*
GrassSolid_vp.glsl - vertex uber shader for grass meshes
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

attribute vec4		attr_Position;	// gl_VertexID emulation (already preserved by & 15)
attribute vec4		attr_Normal;
attribute vec4		attr_LightColor;
attribute vec4		attr_LightStyles;

uniform mat4		u_ModelMatrix;
uniform float		u_LightStyleValues[MAX_LIGHTSTYLES];
uniform vec4		u_GammaTable[64];
uniform float		u_GrassFadeStart;
uniform float		u_GrassFadeDist;
uniform float		u_GrassFadeEnd;
uniform vec3		u_ViewOrigin;
uniform float		u_RealTime;

varying vec2		var_TexDiffuse;
varying vec3		var_VertexLight;

void main( void )
{
	float dist = distance( u_ViewOrigin, ( u_ModelMatrix * vec4( attr_Position.xyz, 1.0 )).xyz );
	float scale = clamp(( u_GrassFadeEnd - dist ) / u_GrassFadeDist, 0.0, 1.0 );
	int vertexID = int( attr_Position.w ) - int( 4.0 * floor( attr_Position.w * 0.25 ));		// equal to gl_VertexID & 3
	vec4 position = vec4( attr_Position.xyz + attr_Normal.xyz * ( attr_Normal.w * scale ), 1.0 );	// in object space

	if( bool( dist < GRASS_ANIM_DIST ) && bool( vertexID == 1 || vertexID == 2 ))
	{
		position.x += sin( position.z + u_RealTime * 0.5 );
		position.y += cos( position.z + u_RealTime * 0.5 );
	}

	vec4 worldpos = u_ModelMatrix * position;
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	var_TexDiffuse = GetTexCoordsForVertex( int( attr_Position.w ));
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

#if !defined( GRASS_FULLBRIGHT )
	var_VertexLight = vec3( 0.0 );

#if defined( GRASS_APPLY_STYLE0 )
	var_VertexLight += UnpackVector( attr_LightColor.x ) * u_LightStyleValues[int( attr_LightStyles[0] )];
#endif

#if defined( GRASS_APPLY_STYLE1 )
	var_VertexLight += UnpackVector( attr_LightColor.y ) * u_LightStyleValues[int( attr_LightStyles[1] )];
#endif

#if defined( GRASS_APPLY_STYLE2 )
	var_VertexLight += UnpackVector( attr_LightColor.z ) * u_LightStyleValues[int( attr_LightStyles[2] )];
#endif

#if defined( GRASS_APPLY_STYLE3 )
	var_VertexLight += UnpackVector( attr_LightColor.w ) * u_LightStyleValues[int( attr_LightStyles[3] )];
#endif
	var_VertexLight = min(( var_VertexLight * LIGHTMAP_SHIFT ), 1.0 );

	// apply screen gamma
	float gammaIndex = (var_VertexLight.r * 255.0);
	var_VertexLight.r = u_GammaTable[int(gammaIndex/4)][int(mod(gammaIndex, 4 ))];
	gammaIndex = (var_VertexLight.g * 255.0);
	var_VertexLight.g = u_GammaTable[int(gammaIndex/4)][int(mod(gammaIndex, 4 ))];
	gammaIndex = (var_VertexLight.b * 255.0);
	var_VertexLight.b = u_GammaTable[int(gammaIndex/4)][int(mod(gammaIndex, 4 ))];
#endif//GRASS_FULLBRIGHT
}