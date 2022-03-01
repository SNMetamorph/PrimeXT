/*
light_bmodel_vp.glsl - vertex uber shader for all dlight types for bmodel surfaces
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

#include "matrix.h"
#include "tnbasis.h"

attribute vec3	attr_Position;
attribute vec4	attr_TexCoord0;

uniform mat4	u_LightViewProjMatrix;
uniform vec4	u_LightOrigin;
uniform vec2	u_DetailScale;
uniform vec3	u_ViewOrigin;
uniform mat4	u_ModelMatrix;
uniform mat4	u_ReflectMatrix;
uniform vec2	u_TexOffset;

varying vec2	var_TexDiffuse;
varying vec3	var_LightVec;
varying vec3	var_ViewVec;

#if defined( HAS_DETAIL )
varying vec2	var_TexDetail;
#endif

#if defined( APPLY_TERRAIN )
varying vec2	var_TexGlobal;
#endif

#if defined( HAS_NORMALMAP )
varying mat3	var_WorldMat;
#else
varying vec3	var_Normal;
#endif

#if defined( PLANAR_REFLECTION )
varying vec4	var_TexMirror;	// mirror coords
#endif

#if defined( LIGHT_SPOT )
varying vec4	var_ProjCoord;
#endif

#if defined( LIGHT_SPOT ) || defined( LIGHT_PROJ )
varying vec4	var_ShadowCoord;
#endif

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
varying vec3	var_TangentViewDir;
varying vec3	var_TangentLightDir;
#endif

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 ); // in object space
	vec4 worldpos = u_ModelMatrix * position;

	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	mat3 tbn = ComputeTBN( u_ModelMatrix );

	// used for diffuse, normalmap, specular and height map
	var_TexDiffuse = ( attr_TexCoord0.xy + u_TexOffset );
#if defined( HAS_DETAIL )
	var_TexDetail = var_TexDiffuse * u_DetailScale;
#endif

#if defined( APPLY_TERRAIN )
	var_TexGlobal = attr_TexCoord0.zw;
#endif

#if defined( LIGHT_SPOT )
	var_ProjCoord = ( Mat4Texture( -0.5 ) * u_LightViewProjMatrix ) * worldpos;
#if defined( APPLY_SHADOW )
	var_ShadowCoord = ( Mat4Texture( 0.5 ) * u_LightViewProjMatrix ) * worldpos;
#endif
#elif defined( LIGHT_PROJ )
	var_ShadowCoord = worldpos;
#endif

	// these things are in worldspace and not a normalized
	var_LightVec = ( u_LightOrigin.xyz - worldpos.xyz );
	var_ViewVec = ( u_ViewOrigin.xyz - worldpos.xyz );

#if defined( HAS_NORMALMAP )
	var_WorldMat = tbn;
#else
	var_Normal = tbn[2];
#endif

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
	var_TangentViewDir = (u_ViewOrigin.xyz - worldpos.xyz) * tbn;
	var_TangentLightDir = (u_LightOrigin.xyz - worldpos.xyz) * tbn;
#endif

#if defined( PLANAR_REFLECTION )
	var_TexMirror = ( Mat4Texture( 0.5 ) * u_ReflectMatrix ) * worldpos;
#endif
}