/*
decal_studio_vp.glsl - vertex uber shader for studio decals
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
#include "lightmodel.h"
#include "skinning.h"
#include "indirect.h"
#include "tnbasis.h"

attribute vec3	attr_Position;
attribute vec4	attr_TexCoord0;

#if defined (VERTEX_LIGHTING)
attribute vec4	attr_LightColor;
attribute vec4	attr_LightVecs;
#endif

uniform float	u_AmbientFactor;
uniform float	u_DiffuseFactor;
uniform vec3	u_ViewOrigin;
uniform float	u_Smoothness;
uniform vec3	u_LightDiffuse;
uniform vec2	u_LightShade;
uniform vec3	u_LightDir;

#if defined (VERTEX_LIGHTING)
uniform vec4	u_GammaTable[64];
uniform vec4	u_LightStyles;
#endif

varying vec4	var_TexDiffuse;
varying vec3	var_AmbientLight;
varying vec3	var_DiffuseLight;
varying vec3	var_Normal;

varying vec3	var_LightDir;
varying vec3	var_ViewDir;

#if defined( REFLECTION_CUBEMAP )
varying vec3	var_WorldNormal;
varying vec3	var_Position;
#endif

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );
	mat4 boneMatrix = ComputeSkinningMatrix();
	vec4 worldpos = boneMatrix * position;

	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	mat3 tbn = ComputeTBN( boneMatrix );
	vec3 srcV = ( u_ViewOrigin - worldpos.xyz );
	vec3 srcL = u_LightDir;
	vec3 srcN = tbn[2];

	var_AmbientLight = vec3( 0.0 );
	var_DiffuseLight = vec3( 0.0 );

// compute studio vertex lighting
#if !defined( LIGHTING_FULLBRIGHT )
#if defined (VERTEX_LIGHTING)
	vec3 lightmap = vec3( 0.0 );
	vec3 deluxmap = vec3( 0.0 );
	float gammaIndex;

	if( u_LightStyles.x != 0.0 )
	{
		lightmap += UnpackVector( attr_LightColor.x ) * u_LightStyles.x;
		deluxmap += UnpackNormal( attr_LightVecs.x ) * u_LightStyles.x;
	}

	if( u_LightStyles.y != 0.0 )
	{
		lightmap += UnpackVector( attr_LightColor.y ) * u_LightStyles.y;
		deluxmap += UnpackNormal( attr_LightVecs.y ) * u_LightStyles.y;
	}

	if( u_LightStyles.z != 0.0 )
	{
		lightmap += UnpackVector( attr_LightColor.z ) * u_LightStyles.z;
		deluxmap += UnpackNormal( attr_LightVecs.z ) * u_LightStyles.z;
	}

	if( u_LightStyles.w != 0.0 )
	{
		lightmap += UnpackVector( attr_LightColor.w ) * u_LightStyles.w;
		deluxmap += UnpackNormal( attr_LightVecs.w ) * u_LightStyles.w;
	}

	var_DiffuseLight = min(( lightmap * LIGHTMAP_SHIFT ), 1.0 );
	srcL = ( deluxmap * LIGHTMAP_SHIFT ); // get lightvector

	// apply screen gamma
	gammaIndex = (var_DiffuseLight.r * 255.0);
	var_DiffuseLight.r = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	gammaIndex = (var_DiffuseLight.g * 255.0);
	var_DiffuseLight.g = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	gammaIndex = (var_DiffuseLight.b * 255.0);
	var_DiffuseLight.b = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	var_DiffuseLight *= LIGHT_SCALE;
#else
	vec3 N = normalize( srcN );
	vec3 L = normalize( srcL );
	vec3 V = normalize( srcV );
#if !defined( HAS_NORMALMAP )
	float factor = u_LightShade.y;
	float NdotL = (( dot( N, -L ) + ( SHADE_LAMBERT - 1.0 )) / SHADE_LAMBERT );
	if( NdotL > 0.0 ) factor -= DiffuseBRDF( N, V, L, u_Smoothness, saturate( NdotL )) * u_LightShade.y;
	var_DiffuseLight = u_LightDiffuse * factor * LIGHT_SCALE;
#else
	var_DiffuseLight = u_LightDiffuse * LIGHT_SCALE;
#endif
	var_AmbientLight = AmbientLight( srcN ) * u_LightShade.x;
#endif
#endif//LIGHTING_FULLBRIGHT 
	var_TexDiffuse = attr_TexCoord0;

// NOTE: this mess is needed only for transparent models because
// refraction and aberration are ready to work only in tangent space
#if defined( HAS_NORMALMAP ) || defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
	// transform lightdir into tangent space
	var_LightDir = srcL * tbn;
	var_ViewDir = srcV * tbn;
	var_Normal = srcN * tbn;
#else	// leave in worldspace
	var_LightDir = srcL;
	var_ViewDir = srcV;
	var_Normal = srcN;
#endif

#if defined( REFLECTION_CUBEMAP )
	var_Position = worldpos.xyz;
	var_WorldNormal = srcN;
#endif//REFLECTION_CUBEMAP
}