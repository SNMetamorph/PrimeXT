/*
cubemap.h - handle reflection probes
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

#ifndef CUBEMAP_H
#define CUBEMAP_H

#include "fresnel.h"

uniform samplerCube		u_EnvMap0;
uniform samplerCube		u_EnvMap1;
uniform samplerCube		u_SpecularMapIBL0;
uniform samplerCube 	u_SpecularMapIBL1;

uniform vec3		u_BoxMins[2];
uniform vec3		u_BoxMaxs[2];
uniform vec3		u_CubeOrigin[2];
uniform vec2		u_CubeMipCount;
uniform float		u_LerpFactor;

vec3 CubemapBoxParallaxCorrected( const vec3 vReflVec, const vec3 vPosition, const vec3 vCubePos, const vec3 vBoxExtentsMin, const vec3 vBoxExtentsMax )
{
	// parallax correction for local cubemaps using a box as geometry proxy

	// min/max intersection
	vec3 vBoxIntersectionMax = (vBoxExtentsMax - vPosition) / vReflVec;
	vec3 vBoxIntersectionMin = (vBoxExtentsMin - vPosition) / vReflVec;
	
	// intersection test
	vec3 vFurthestPlane;
	vFurthestPlane.x = (vReflVec.x > 0.0) ? vBoxIntersectionMax.x : vBoxIntersectionMin.x;
	vFurthestPlane.y = (vReflVec.y > 0.0) ? vBoxIntersectionMax.y : vBoxIntersectionMin.y;
	vFurthestPlane.z = (vReflVec.z > 0.0) ? vBoxIntersectionMax.z : vBoxIntersectionMin.z;	
	float fDistance = min( min( vFurthestPlane.x, vFurthestPlane.y ), vFurthestPlane.z );

	// apply new reflection position
	vec3 vInterectionPos = vPosition + vReflVec * fDistance;
	return (vInterectionPos - vCubePos);
}

vec3 CubemapProbeInternal( const vec3 vPos, const vec3 vView, const vec3 nWorld, const float smoothness, samplerCube cubemap0, samplerCube cubemap1 )
{
	vec3 I = normalize( vPos - vView ); // in world space
	vec3 NW = normalize( nWorld );
	vec3 wRef = normalize( reflect( I, NW ));
	vec3 R1 = CubemapBoxParallaxCorrected( wRef, vPos, u_CubeOrigin[0], u_BoxMins[0], u_BoxMaxs[0] );
	vec3 R2 = CubemapBoxParallaxCorrected( wRef, vPos, u_CubeOrigin[1], u_BoxMins[1], u_BoxMaxs[1] );
	vec3 srcColor0 = textureLod( cubemap0, R1, u_CubeMipCount.x - smoothness * u_CubeMipCount.x ).rgb;
	vec3 srcColor1 = textureLod( cubemap1, R2, u_CubeMipCount.y - smoothness * u_CubeMipCount.y ).rgb;
	vec3 reflectance = mix( srcColor0, srcColor1, u_LerpFactor );
	return reflectance;
}

vec3 CubemapReflectionProbe( const vec3 vPos, const vec3 vView, const vec3 nWorld, const float smoothness )
{
	return CubemapProbeInternal(vPos, vView, nWorld, smoothness, u_EnvMap0, u_EnvMap1);
}

vec3 CubemapSpecularIBLProbe( const vec3 vPos, const vec3 vView, const vec3 nWorld, const float smoothness )
{
	return CubemapProbeInternal(vPos, vView, nWorld, smoothness, u_SpecularMapIBL0, u_SpecularMapIBL1);
}

#endif//CUBEMAP_H