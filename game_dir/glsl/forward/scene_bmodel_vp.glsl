/*
scene_bmodel_vp.glsl - vertex uber shader for brush surfaces with static lighting
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
attribute vec4	attr_TexCoord0;	// diffuse\terrain
attribute vec4	attr_TexCoord1;	// lightmap 0-1
attribute vec4	attr_TexCoord2;	// lightmap 2-3
attribute vec4	attr_LightStyles;

uniform float	u_LightStyleValues[MAX_LIGHTSTYLES];
uniform mat4	u_ModelMatrix;
uniform mat4	u_ReflectMatrix;
uniform vec3	u_ViewOrigin;	// already in modelspace
uniform vec2	u_DetailScale;
uniform vec2	u_TexOffset;	// conveyor stuff

centroid varying vec2	var_TexDiffuse;
centroid varying vec3	var_TexLight0;
centroid varying vec3	var_TexLight1;
centroid varying vec3	var_TexLight2;
centroid varying vec3	var_TexLight3;
centroid varying vec2	var_TexDetail;
centroid varying vec2	var_TexGlobal;
centroid varying vec3	var_ViewDir;
centroid varying vec3	var_Position;

#if defined( PLANAR_REFLECTION ) || defined( PORTAL_SURFACE )
varying vec4	var_TexMirror;	// mirror coords
#endif

#if defined( REFLECTION_CUBEMAP )
centroid varying mat3	var_MatrixTBN;
#endif

#if !defined( HAS_NORMALMAP )
varying vec3	var_Normal;
#endif

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
centroid varying vec3	var_TangentViewDir;
#endif

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 ); // in object space
	vec4 worldpos = u_ModelMatrix * position;

	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	mat3 tbn = ComputeTBN( u_ModelMatrix );
	vec3 N = tbn[2];

	// used for diffuse, normalmap, specular and height map
	var_TexDiffuse = ( attr_TexCoord0.xy + u_TexOffset );
	var_TexDetail = var_TexDiffuse * u_DetailScale;
	var_TexGlobal = attr_TexCoord0.zw;
	var_Position = worldpos.xyz;

	// setup lightstyles
#if defined( APPLY_STYLE0 )
	var_TexLight0.xy = attr_TexCoord1.xy;
	var_TexLight0.z = u_LightStyleValues[int( attr_LightStyles[0] )];
#endif

#if defined( APPLY_STYLE1 )
	var_TexLight1.xy = attr_TexCoord1.zw;
	var_TexLight1.z = u_LightStyleValues[int( attr_LightStyles[1] )];
#endif

#if defined( APPLY_STYLE2 )
	var_TexLight2.xy = attr_TexCoord2.xy;
	var_TexLight2.z = u_LightStyleValues[int( attr_LightStyles[2] )];
#endif

#if defined( APPLY_STYLE3 )
	var_TexLight3.xy = attr_TexCoord2.zw;
	var_TexLight3.z = u_LightStyleValues[int( attr_LightStyles[3] )];
#endif
	// transform viewdir into tangent space
	vec3 V = ( u_ViewOrigin - worldpos.xyz ); // don't normalize!
	var_ViewDir = V * tbn;

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
	var_TangentViewDir = (u_ViewOrigin - worldpos.xyz) * tbn;
#endif

#if !defined( HAS_NORMALMAP )
	var_Normal = N * tbn;
#endif

#if defined( PLANAR_REFLECTION )
	var_TexMirror = ( Mat4Texture( 0.5 ) * u_ReflectMatrix ) * position;
#elif defined( PORTAL_SURFACE )
	var_TexMirror = ( Mat4Texture( 0.5 ) * u_ReflectMatrix ) * worldpos;
#endif

#if defined( REFLECTION_CUBEMAP )
	var_MatrixTBN = tbn;
#endif
}