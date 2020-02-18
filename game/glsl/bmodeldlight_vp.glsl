/*
BmodelDlight_vp.glsl - vertex uber shader for all dlight types for bmodel surfaces
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
#include "matrix.h"

attribute vec3		attr_Position;
attribute vec4		attr_TexCoord0;
attribute vec3		attr_Normal;

// model specific
uniform mat4		u_ModelMatrix;
uniform vec3		u_DetailScale;
uniform vec3		u_TexOffset;

// light specific
uniform vec4		u_LightOrigin;
uniform mat4		u_LightViewProjectionMatrix;

varying vec2		var_TexDiffuse;
varying vec2		var_TexDetail;
varying vec3		var_LightVec;
varying vec3		var_Normal;

#define u_RealTime		u_TexOffset.z
#define u_WaveHeight	u_DetailScale.z

#if defined( BMODEL_REFLECTION_PLANAR )
varying vec4		var_TexMirror;	// mirror coords
#endif

#if defined( BMODEL_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
varying vec4		var_ShadowCoord;
#endif

varying vec2		var_TexGlobal;

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 ); // in object space

#if defined( BMODEL_WAVEHEIGHT )
	float nv = r_turbsin( u_RealTime * 2.6 + attr_Position.y + attr_Position.x ) + 8.0;
	nv = ( r_turbsin( attr_Position.x * 5.0 + u_RealTime * 2.71 - attr_Position.y ) + 8.0 ) * 0.8 + nv;
	position.z = nv * u_WaveHeight + attr_Position.z;
#endif
	vec4 worldpos = u_ModelMatrix * position;

	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	var_TexDiffuse = ( attr_TexCoord0.xy + u_TexOffset.xy );
	var_TexDetail = ( attr_TexCoord0.xy + u_TexOffset.xy ) * u_DetailScale.xy;

#if defined( BMODEL_DRAWTURB )
	float os = var_TexDiffuse.x;
	float ot = var_TexDiffuse.y;
	// during the war sine values can reach 3.0!
	var_TexDiffuse.x = ( os + sin( ot * 0.125 + u_TexOffset.z ) * 4.0 ) * (1.0 / 64.0);
	var_TexDiffuse.y = ( ot + sin( os * 0.125 + u_TexOffset.z ) * 4.0 ) * (1.0 / 64.0);
#endif

#if defined( BMODEL_REFLECTION_PLANAR )
	var_TexMirror = gl_TextureMatrix[0] * worldpos;
#endif

#if defined( BMODEL_LIGHT_PROJECTION )
	var_ProjCoord = ( Mat4Texture( -0.5 ) * u_LightViewProjectionMatrix ) * worldpos;
#if defined( BMODEL_HAS_SHADOWS )
	var_ShadowCoord = ( Mat4Texture( 0.5 ) * u_LightViewProjectionMatrix ) * worldpos;
#endif
#endif
	// these things are in worldspace and not a normalized
	var_LightVec = ( u_LightOrigin.xyz - worldpos.xyz );
	var_Normal = ( u_ModelMatrix * vec4( attr_Normal, 0.0 )).xyz;

	var_TexGlobal = attr_TexCoord0.zw;
}