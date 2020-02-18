/*
StudioDepth_vp.glsl - studio shadow pass
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
#include "mathlib.h"

attribute vec3		attr_Position;
attribute vec2		attr_TexCoord0;
attribute vec4		attr_BoneIndexes;
attribute vec4		attr_BoneWeights;

uniform vec4		u_BoneQuaternion[MAXSTUDIOBONES];
uniform vec3		u_BonePosition[MAXSTUDIOBONES];

varying vec2		var_TexCoord;	// for alpha-testing

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
	vec4 worldpos = boneMatrix * vec4( attr_Position, 1.0 );

	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;
	var_TexCoord = attr_TexCoord0;
}