/*
frustum.h - frustum test implementation class
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#include "matrix.h"
#include "bounding_box.h"
#include "com_model.h"
#include <stdint.h>

// don't change this order
#define FRUSTUM_LEFT	0
#define FRUSTUM_RIGHT	1
#define FRUSTUM_BOTTOM	2
#define FRUSTUM_TOP		3
#define FRUSTUM_FAR		4
#define FRUSTUM_NEAR	5
#define FRUSTUM_PLANES	6

class CFrustum
{
public:
	void InitProjection( const matrix4x4 &view, float flZNear, float flZFar, float flFovX, float flFovY );
	void InitOrthogonal( const matrix4x4 &view, float xLeft, float xRight, float yBottom, float yTop, float flZNear, float flZFar );
	void InitBoxFrustum( const Vector &org, float radius ); // used for pointlights
	void InitProjectionFromMatrix( const matrix4x4 &projection );
	void SetPlane( int side, const mplane_t *plane ) { planes[side] = *plane; }
	void SetPlane( int side, const Vector &vecNormal, float flDist );
	void NormalizePlane( int side );
	void ClearFrustum( void );

	// plane manipulating
	void EnablePlane( int side );
	void DisablePlane( int side );

	const mplane_t *GetPlane( int side ) const { return &planes[side]; }
	const mplane_t *GetPlanes( void ) const { return &planes[0]; }
	unsigned int GetClipFlags( void ) const { return clipFlags; }
	void ComputeFrustumBounds( Vector &mins, Vector &maxs ) const;
	void ComputeFrustumCorners( Vector bbox[8] ) const;

	// cull methods
	bool CullBoxFast( const Vector &mins, const Vector &maxs, int userClipFlags = 0 ) const;
	bool CullBoxSafe( const CBoundingBox &bounds ) const;
	bool CullSphere( const Vector &centre, float radius, int userClipFlags = 0 ) const;
	bool CullFrustum( const CFrustum *frustum ) const;

private:
	mplane_t	planes[FRUSTUM_PLANES];
	uint32_t	clipFlags;
};
