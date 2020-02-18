/*
StudioDlight_vp.glsl - vertex uber shader for all dlight types for studio meshes
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

attribute vec3		attr_Position;
attribute vec2		attr_TexCoord0;
attribute vec3		attr_Normal;
attribute vec4		attr_BoneIndexes;
attribute vec4		attr_BoneWeights;

uniform vec4		u_BoneQuaternion[MAXSTUDIOBONES];
uniform vec3		u_BonePosition[MAXSTUDIOBONES];
uniform mat4		u_LightViewProjectionMatrix;		// lightProjection * lightView * modelTransform
uniform vec4		u_LightOrigin;			// local space of model
uniform vec4		u_LightDir;			// local space of model

uniform vec3		u_ViewOrigin;			// local space of model
uniform vec3		u_ViewRight;			// local space of model

varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;
varying vec3		var_Normal;

#if defined( STUDIO_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
#if defined( STUDIO_HAS_SHADOWS )
varying vec4		var_ShadowCoord;
#endif
#endif

void main( void )
{
#if defined( STUDIO_BONEWEIGHTING )
	int boneIndex0 = int( attr_BoneIndexes[0] );
	int boneIndex1 = int( attr_BoneIndexes[1] );
	int boneIndex2 = int( attr_BoneIndexes[2] );
	int boneIndex3 = int( attr_BoneIndexes[3] );
	float flWeight0 = attr_BoneWeights[0] / 255.0;
	float flWeight1 = attr_BoneWeights[1] / 255.0;
	float flWeight2 = attr_BoneWeights[2] / 255.0;
	float flWeight3 = attr_BoneWeights[3] / 255.0;
	float flTotal = flWeight0 + flWeight1 + flWeight2 + flWeight3;
	if( flTotal < 1.0 ) flWeight0 += 1.0 - flTotal;	// compensate rounding error

	// compute hardware skinning with boneweighting
	mat4 boneMatrix = Mat4FromOriginQuat( u_BoneQuaternion[boneIndex0], u_BonePosition[boneIndex0] ) * flWeight0;
	if( boneIndex1 != -1 ) boneMatrix += Mat4FromOriginQuat( u_BoneQuaternion[boneIndex1], u_BonePosition[boneIndex1] ) * flWeight1;
	if( boneIndex2 != -1 ) boneMatrix += Mat4FromOriginQuat( u_BoneQuaternion[boneIndex2], u_BonePosition[boneIndex2] ) * flWeight2;
	if( boneIndex3 != -1 ) boneMatrix += Mat4FromOriginQuat( u_BoneQuaternion[boneIndex3], u_BonePosition[boneIndex3] ) * flWeight3;
#else
	int boneIndex0 = int( attr_BoneIndexes[0] );

	// compute hardware skinning without boneweighting
	mat4 boneMatrix = Mat4FromOriginQuat( u_BoneQuaternion[boneIndex0], u_BonePosition[boneIndex0] );
#endif
	vec4 position = boneMatrix * vec4( attr_Position, 1.0 );
	vec3 N = (boneMatrix * vec4( attr_Normal, 0.0 )).xyz;

	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * position;
	gl_ClipVertex = gl_ModelViewMatrix * position;

#if defined( STUDIO_LIGHT_PROJECTION )
	var_ProjCoord = ( Mat4Texture( -0.5 ) * u_LightViewProjectionMatrix ) * position;
#if defined( STUDIO_HAS_SHADOWS )
	var_ShadowCoord = ( Mat4Texture( 0.5 ) * u_LightViewProjectionMatrix ) * position;
#endif
#endif
	var_TexDiffuse = attr_TexCoord0;

#if defined( STUDIO_HAS_CHROME )
	// compute chrome texcoords
	vec3 origin = normalize( -u_ViewOrigin + vec3( boneMatrix[3] ));
	vec3 chromeup = normalize( cross( origin, u_ViewRight ));
	vec3 chromeright = normalize( cross( origin, chromeup ));

	var_TexDiffuse.x *= ( dot( N, chromeright ) + 1.0 ) * 32.0;	// calc s coord
	var_TexDiffuse.y *= ( dot( N, chromeup ) + 1.0 ) * 32.0;	// calc t coord
#endif
	// these things are in worldspace and not a normalized
	var_LightVec = ( u_LightOrigin.xyz - position.xyz );
	var_Normal = N;
}