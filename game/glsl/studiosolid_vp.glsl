/*
StudioSolid_vp.glsl - vertex uber shader for all solid studio meshes
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

#if defined (STUDIO_VERTEX_LIGHTING)
attribute vec4		attr_LightColor;
#endif

uniform vec4		u_BoneQuaternion[MAXSTUDIOBONES];
uniform vec3		u_BonePosition[MAXSTUDIOBONES];

#if defined (STUDIO_VERTEX_LIGHTING)
uniform vec4		u_GammaTable[64];
uniform vec4		u_LightStyles;
#else
uniform vec3		u_LightDir;
uniform vec3		u_LightColor;
uniform float		u_LightAmbient;
uniform float		u_LightShade;
#endif

uniform vec3		u_ViewOrigin;
uniform vec3		u_ViewRight;

varying vec3		var_VertexLight;
varying vec2		var_TexDiffuse;

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 );
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
#elif ( MAXSTUDIOBONES == 1 )
	mat4 boneMatrix = Mat4FromOriginQuat( u_BoneQuaternion[0], u_BonePosition[0] );
#else
	int boneIndex0 = int( attr_BoneIndexes[0] );
	// compute hardware skinning without boneweighting
	mat4 boneMatrix = Mat4FromOriginQuat( u_BoneQuaternion[boneIndex0], u_BonePosition[boneIndex0] );
#endif
	vec4 worldpos = boneMatrix * position;
	vec3 N = (boneMatrix * vec4( attr_Normal, 0.0 )).xyz;

	// transform vertex position into homogenous clip-space
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

#if defined( STUDIO_FULLBRIGHT )
	var_VertexLight = vec3( 1.0 );	// just get fullbright
#elif defined (STUDIO_VERTEX_LIGHTING)
	vec3 lightmap = vec3( 0.0 );
	float gammaIndex;

	if( u_LightStyles.x != 0.0 )
		lightmap += UnpackVector( attr_LightColor.x ) * u_LightStyles.x;

	if( u_LightStyles.y != 0.0 )
		lightmap += UnpackVector( attr_LightColor.y ) * u_LightStyles.y;

	if( u_LightStyles.z != 0.0 )
		lightmap += UnpackVector( attr_LightColor.z ) * u_LightStyles.z;

	if( u_LightStyles.w != 0.0 )
		lightmap += UnpackVector( attr_LightColor.w ) * u_LightStyles.w;

	var_VertexLight = min(( lightmap * LIGHTMAP_SHIFT ), 1.0 );

	// apply screen gamma
	gammaIndex = (var_VertexLight.r * 255.0);
	var_VertexLight.r = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	gammaIndex = (var_VertexLight.g * 255.0);
	var_VertexLight.g = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	gammaIndex = (var_VertexLight.b * 255.0);
	var_VertexLight.b = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
#else
	float	illum = u_LightAmbient;

#if defined( STUDIO_LIGHT_FLATSHADE )
	illum += u_LightShade * 0.8;
#else
	vec3	L = normalize( u_LightDir );
	float	lightcos;

	lightcos = dot( normalize( N ), L );
	lightcos = min( lightcos, 1.0 );
	illum += u_LightShade;

	// do modified hemispherical lighting
	lightcos = (lightcos + ( SHADE_LAMBERT - 1.0 )) / SHADE_LAMBERT;
	if( lightcos > 0.0 )
		illum -= u_LightShade * lightcos; 
	illum = max( illum, 0.0 );	
#endif
	var_VertexLight = u_LightColor * (illum / 255.0);
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
}