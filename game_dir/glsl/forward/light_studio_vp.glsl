/*
light_studio_vp.glsl - studio forward lighting
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
#include "skinning.h"
#include "tnbasis.h"

attribute vec3	attr_Position;
attribute vec2	attr_TexCoord0;

uniform mat4	u_LightViewProjMatrix;
uniform vec4	u_LightOrigin;
uniform vec2	u_DetailScale;
uniform vec3	u_ViewOrigin;
uniform vec3	u_ViewRight;
uniform float	u_SwayHeight;
uniform float	u_RealTime;

varying vec2	var_TexDiffuse;
varying vec3	var_LightVec;
varying vec3	var_ViewVec;

#if defined( HAS_DETAIL )
varying vec2	var_TexDetail;
#endif

#if defined( HAS_NORMALMAP )
varying mat3	var_WorldMat;
#else
varying vec3	var_Normal;
#endif

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
varying vec3	var_TangentViewDir;
#endif

#if defined( LIGHT_SPOT )
varying vec4	var_ProjCoord;
#endif

#if defined( LIGHT_SPOT ) || defined( LIGHT_PROJ )
varying vec4	var_ShadowCoord;
#endif

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );

	if( bool( u_SwayHeight != 0.0 ) && position.z > u_SwayHeight )
    {
        position.x += position.z * 0.025 * sin( position.z + u_RealTime * 0.25 );
        position.y += position.z * 0.025 * cos( position.z + u_RealTime * 0.25 );
    }

	mat4 boneMatrix = ComputeSkinningMatrix();
	vec4 worldpos = boneMatrix * position;

	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	mat3 tbn = ComputeTBN( boneMatrix );
	vec3 N = tbn[2];

#if defined( LIGHT_SPOT )
	var_ProjCoord = ( Mat4Texture( -0.5 ) * u_LightViewProjMatrix ) * worldpos;
#if defined( APPLY_SHADOW )
	var_ShadowCoord = ( Mat4Texture( 0.5 ) * u_LightViewProjMatrix ) * worldpos;
#endif
#elif defined( LIGHT_PROJ )
	var_ShadowCoord = worldpos;
#endif
	var_TexDiffuse = attr_TexCoord0;

#if defined( HAS_CHROME )
	CalcChrome( var_TexDiffuse, N, u_ViewOrigin, vec3( boneMatrix[3] ), u_ViewRight );
#endif
#if defined( HAS_DETAIL )
	var_TexDetail = attr_TexCoord0.xy * u_DetailScale;
#endif

	// these things are in worldspace and not a normalized
	var_LightVec = ( u_LightOrigin.xyz - worldpos.xyz );
	var_ViewVec = ( u_ViewOrigin.xyz - worldpos.xyz );

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
	var_TangentViewDir = var_ViewVec * tbn;
#endif

#if defined( HAS_NORMALMAP )
	var_WorldMat = tbn;
#else
	var_Normal = N;
#endif
}