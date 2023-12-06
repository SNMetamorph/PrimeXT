/*
fitnormals.h - best fit normals by Crytek paper
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

#ifndef FITNORMALS_H
#define FITNORMALS_H

// comment this to compare quality
#define ALLOW_NORMALS_FITTING

uniform sampler2D		u_FitNormalMap;

void CompressUnsignedNormalToNormalsBuffer( inout vec3 vNormal )
{
#if defined( ALLOW_NORMALS_FITTING )
	// expand from unsigned
	vNormal.rgb = vNormal.rgb * 2.0 - 1.0;

	// renormalize (needed if any blending or interpolation happened before)
	vNormal.rgb = normalize( vNormal.rgb );

	// get unsigned normal for cubemap lookup (note the full float presision is required)
	vec3 vNormalUns = abs( vNormal.rgb );
	// get the main axis for cubemap lookup
	float maxNAbs = max( vNormalUns.z, max( vNormalUns.x, vNormalUns.y ));

	// get texture coordinates in a collapsed cubemap
	vec2 vTexCoord = vNormalUns.z < maxNAbs ? ( vNormalUns.y < maxNAbs ? vNormalUns.yz : vNormalUns.xz) : vNormalUns.xy;
	vTexCoord = vTexCoord.x < vTexCoord.y ? vTexCoord.yx : vTexCoord.xy;
	vTexCoord.y /= vTexCoord.x;
	// fit normal into the edge of unit cube
	vNormal.rgb /= maxNAbs;

	// look-up fitting length and scale the normal to get the best fit
	float fFittingScale = texture( u_FitNormalMap, vTexCoord ).r;

	// scale the normal to get the best fit
	vNormal.rgb *= fFittingScale;

	// squeeze back to unsigned
	vNormal.rgb = vNormal.rgb * 0.5 + 0.5;
#endif
}

#endif//FITNORMALS_H