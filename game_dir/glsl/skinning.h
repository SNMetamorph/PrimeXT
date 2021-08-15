/*
skinning.h - skininng animated meshes
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

#ifndef SKINNING_H
#define SKINNING_H

// for 512 uniforms we has limitations for bonecount
// and can't use matrices in all cases
#if ( MAXSTUDIOBONES == 1 || MAXSTUDIOBONES == 128 )
#define STUDIO_BONES_MATRICES
#endif

#include "matrix.h"

attribute vec4		attr_BoneIndexes;
attribute vec4		attr_BoneWeights;

#if defined( STUDIO_BONES_MATRICES )
uniform vec4		u_BonesArray[MAXSTUDIOBONES*3];
#else
uniform vec4		u_BoneQuaternion[MAXSTUDIOBONES];
uniform vec3		u_BonePosition[MAXSTUDIOBONES];
#endif

#if defined( STUDIO_BONES_MATRICES )
mat4 Mat4FromVec4Array( int boneIndex, float weight )
{
	mat4 m;

	m[0][0] = u_BonesArray[boneIndex*3+0].x * weight;
	m[0][1] = u_BonesArray[boneIndex*3+0].y * weight;
	m[0][2] = u_BonesArray[boneIndex*3+0].z * weight;
	m[0][3] = 0.0;
	m[1][0] = u_BonesArray[boneIndex*3+1].x * weight;
	m[1][1] = u_BonesArray[boneIndex*3+1].y * weight;
	m[1][2] = u_BonesArray[boneIndex*3+1].z * weight;
	m[1][3] = 0.0;
	m[2][0] = u_BonesArray[boneIndex*3+2].x * weight;
	m[2][1] = u_BonesArray[boneIndex*3+2].y * weight;
	m[2][2] = u_BonesArray[boneIndex*3+2].z * weight;
	m[2][3] = 0.0;
	m[3][0] = u_BonesArray[boneIndex*3+0].w * weight;
	m[3][1] = u_BonesArray[boneIndex*3+1].w * weight;
	m[3][2] = u_BonesArray[boneIndex*3+2].w * weight;
	m[3][3] = weight;

	return m;
}

mat4 ComputeSkinningMatrix( void )
{
#if defined( APPLY_BONE_WEIGHTING )
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
	mat4 boneMatrix = Mat4FromVec4Array( boneIndex0, flWeight0 );
	if( boneIndex1 != -1 ) boneMatrix += Mat4FromVec4Array( boneIndex1, flWeight1 );
	if( boneIndex2 != -1 ) boneMatrix += Mat4FromVec4Array( boneIndex2, flWeight2 );
	if( boneIndex3 != -1 ) boneMatrix += Mat4FromVec4Array( boneIndex3, flWeight3 );
	return boneMatrix;
#elif ( MAXSTUDIOBONES == 1 )
	return Mat4FromVec4Array( 0, 1.0 );
#else
	return Mat4FromVec4Array( int( attr_BoneIndexes[0] ), 1.0 );
#endif
}
#else
mat4 ComputeSkinningMatrix( void )
{
#if defined( APPLY_BONE_WEIGHTING )
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
	return boneMatrix;
#elif ( MAXSTUDIOBONES == 1 )
	return Mat4FromOriginQuat( u_BoneQuaternion[0], u_BonePosition[0] );
#else
	int boneIndex0 = int( attr_BoneIndexes[0] );
	// compute hardware skinning without boneweighting
	return Mat4FromOriginQuat( u_BoneQuaternion[boneIndex0], u_BonePosition[boneIndex0] );
#endif
}
#endif

void CalcChrome( inout vec2 coord, const vec3 N, const vec3 vieworg, const vec3 bonepos, const vec3 viewright )
{
	// compute chrome texcoords
	vec3 origin = normalize( -vieworg + bonepos );
	vec3 chromeup = normalize( cross( origin, viewright ));
	vec3 chromeright = normalize( cross( origin, chromeup ));

	coord.x *= ( dot( N, chromeright ) + 1.0 ) * 32.0;	// calc s coord
	coord.y *= ( dot( N, chromeup ) + 1.0 ) * 32.0;	// calc t coord
}

#endif//SKINNING_H