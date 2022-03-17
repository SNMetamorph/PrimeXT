/*
scene_studio_vp.glsl - vertex uber shader for studio meshes with static lighting
Copyright (C) 2019 Uncle Mike

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
#include "mathlib.h"
#include "lightmodel.h"
#include "skinning.h"
#include "indirect.h"
#include "tnbasis.h"

attribute vec3	attr_Position;
attribute vec2	attr_TexCoord0;

#if defined (VERTEX_LIGHTING)
attribute vec4	attr_LightColor;
attribute vec4	attr_LightVecs;
#endif

#if defined (SURFACE_LIGHTING)
attribute vec4	attr_TexCoord1;	// lightmap 0-1
attribute vec4	attr_TexCoord2;	// lightmap 2-3
attribute vec4	attr_LightStyles;
#endif

uniform float	u_AmbientFactor;
uniform float	u_DiffuseFactor;
uniform vec2	u_DetailScale;
uniform vec3	u_ViewOrigin;
uniform vec3	u_ViewRight;
uniform float	u_Smoothness;
uniform vec3	u_LightDiffuse;
uniform vec2	u_LightShade; // x is ambientlight, y is shadelight
uniform vec3	u_LightDir;

uniform float	u_LightStyleValues[MAX_LIGHTSTYLES];
uniform float	u_LightGamma;
uniform vec4	u_LightStyles;

varying vec2	var_TexDiffuse;
varying vec2	var_TexDetail;
varying vec3	var_AmbientLight;
varying vec3	var_DiffuseLight;
varying vec3	var_Normal;

varying vec3	var_LightDir;
varying vec3	var_ViewDir;
varying vec3	var_Position;

#if defined (SURFACE_LIGHTING)
varying vec3	var_TexLight0;
varying vec3	var_TexLight1;
varying vec3	var_TexLight2;
varying vec3	var_TexLight3;
#endif

#if defined( REFLECTION_CUBEMAP )
varying vec3	var_WorldNormal;
varying mat3	var_MatrixTBN;
#endif

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
varying vec3	var_TangentViewDir;
#endif

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );
	mat4 boneMatrix = ComputeSkinningMatrix();
	vec4 worldpos = boneMatrix * position;

	// transform vertex position into homogenous clip-space
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

	// apply gamma-correction for vertex lighting
	var_DiffuseLight = pow(var_DiffuseLight, vec3(1.0 / u_LightGamma));
	var_DiffuseLight *= LIGHT_SCALE;

#elif defined (SURFACE_LIGHTING)
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
#else
	vec3 N = normalize( srcN );
	vec3 L = normalize( srcL );
	vec3 V = normalize( srcV );
#if !defined( HAS_NORMALMAP )
	var_DiffuseLight = u_LightDiffuse * u_LightShade.y * LIGHT_SCALE;
#else
	var_DiffuseLight = u_LightDiffuse * LIGHT_SCALE;
#endif
	var_AmbientLight = AmbientLight( srcN ) * u_LightShade.x;
#endif
#endif//LIGHTING_FULLBRIGHT 
	var_TexDiffuse = attr_TexCoord0;

#if defined( PARALLAX_SIMPLE ) || defined( PARALLAX_OCCLUSION )
	var_TangentViewDir = srcV * tbn;
#endif

// NOTE: this mess is needed only for transparent models because
// refraction and aberration are ready to work only in tangent space
// doing this because TBN generates only for models with normal map
#if defined( HAS_NORMALMAP )
	// transform lightdir into tangent space
	var_LightDir = srcL * tbn;
	var_ViewDir = srcV * tbn;
	var_Normal = srcN * tbn;
#else	// leave in worldspace
	var_LightDir = srcL;
	var_ViewDir = srcV;
	var_Normal = srcN;
#endif
	var_Position = worldpos.xyz;

#if defined( HAS_CHROME )
	CalcChrome( var_TexDiffuse, normalize( srcN ), u_ViewOrigin, vec3( boneMatrix[3] ), u_ViewRight );
#endif
	var_TexDetail = attr_TexCoord0.xy * u_DetailScale;
	
#if defined( REFLECTION_CUBEMAP )
	var_WorldNormal = srcN;
	var_MatrixTBN = tbn;
#endif // REFLECTION_CUBEMAP
}