/*
frustum.cpp - frustum test implementation class
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

#include "frustum.h"
#include "const.h"
#include <assert.h>
#include <mathlib.h>
#include <stringlib.h>

void CFrustum :: ClearFrustum( void )
{
	memset( planes, 0, sizeof( planes ));
	clipFlags = 0;
}

void CFrustum :: EnablePlane( int side )
{
	assert( side >= 0 && side < FRUSTUM_PLANES );

	// make sure what plane is ready
	if( planes[side].normal != g_vecZero )
		SetBits( clipFlags, BIT( side ));
}

void CFrustum :: DisablePlane( int side )
{
	assert( side >= 0 && side < FRUSTUM_PLANES );
	ClearBits( clipFlags, BIT( side ));
}

void CFrustum :: InitProjection( const matrix4x4 &view, float flZNear, float flZFar, float flFovX, float flFovY )
{
	float	xs, xc;
	Vector	normal;	

	// horizontal fov used for left and right planes
	SinCos( DEG2RAD( flFovX ) * 0.5f, &xs, &xc );

	// setup left plane
	normal = view.GetForward() * xs + view.GetRight() * -xc;
	SetPlane( FRUSTUM_LEFT, normal, DotProduct( view.GetOrigin(), normal ));

	// setup right plane
	normal = view.GetForward() * xs + view.GetRight() * xc;
	SetPlane( FRUSTUM_RIGHT, normal, DotProduct( view.GetOrigin(), normal ));

	// vertical fov used for top and bottom planes
	SinCos( DEG2RAD( flFovY ) * 0.5f, &xs, &xc );

	// setup bottom plane
	normal = view.GetForward() * xs + view.GetUp() * -xc;
	SetPlane( FRUSTUM_BOTTOM, normal, DotProduct( view.GetOrigin(), normal ));

	// setup top plane
	normal = view.GetForward() * xs + view.GetUp() * xc;
	SetPlane( FRUSTUM_TOP, normal, DotProduct( view.GetOrigin(), normal ));

	// setup far plane
	SetPlane( FRUSTUM_FAR, -view.GetForward(), DotProduct( -view.GetForward(), ( view.GetOrigin() + view.GetForward() * flZFar )));

	// no need to setup backplane for general view. It's only used for portals and mirrors
	if( flZNear == 0.0f ) return;

	// setup near plane
	SetPlane( FRUSTUM_NEAR, view.GetForward(), DotProduct( view.GetForward(), ( view.GetOrigin() + view.GetForward() * flZNear )));
}

void CFrustum :: InitOrthogonal( const matrix4x4 &view, float xLeft, float xRight, float yBottom, float yTop, float flZNear, float flZFar )
{
	// setup the near and far planes
	float orgOffset = DotProduct( view.GetOrigin(), view.GetForward() );

	SetPlane( FRUSTUM_FAR, -view.GetForward(), -flZNear - orgOffset );
	SetPlane( FRUSTUM_NEAR, view.GetForward(), flZFar + orgOffset );

	// setup left and right planes
	orgOffset = DotProduct( view.GetOrigin(), view.GetRight() );

	SetPlane( FRUSTUM_LEFT, view.GetRight(), xLeft + orgOffset );
	SetPlane( FRUSTUM_RIGHT, -view.GetRight(), -xRight - orgOffset );

	// setup top and buttom planes
	orgOffset = DotProduct( view.GetOrigin(), view.GetUp() );

	SetPlane( FRUSTUM_TOP, view.GetUp(), yTop + orgOffset );
	SetPlane( FRUSTUM_BOTTOM, -view.GetUp(), -yBottom - orgOffset );
}

void CFrustum :: InitBoxFrustum( const Vector &org, float radius )
{
	for( int i = 0; i < 6; i++ )
	{
		// setup normal for each direction
		Vector normal = g_vecZero;
		normal[((i >> 1) + 1) % 3] = (i & 1) ? 1.0f : -1.0f;
		SetPlane( i, normal, DotProduct( org, normal ) - radius );
	}
}

void CFrustum :: InitProjectionFromMatrix( const matrix4x4 &projection )
{
	// left
	planes[FRUSTUM_LEFT].normal[0] =	  projection[0][3] + projection[0][0];
	planes[FRUSTUM_LEFT].normal[1] =	  projection[1][3] + projection[1][0];
	planes[FRUSTUM_LEFT].normal[2] =	  projection[2][3] + projection[2][0];
	planes[FRUSTUM_LEFT].dist =		-(projection[3][3] + projection[3][0]);

	// right
	planes[FRUSTUM_RIGHT].normal[0] =	  projection[0][3] - projection[0][0];
	planes[FRUSTUM_RIGHT].normal[1] =	  projection[1][3] - projection[1][0];
	planes[FRUSTUM_RIGHT].normal[2] =	  projection[2][3] - projection[2][0];
	planes[FRUSTUM_RIGHT].dist =		-(projection[3][3] - projection[3][0]);

	// bottom
	planes[FRUSTUM_BOTTOM].normal[0] =	  projection[0][3] + projection[0][1];
	planes[FRUSTUM_BOTTOM].normal[1] =	  projection[1][3] + projection[1][1];
	planes[FRUSTUM_BOTTOM].normal[2] =	  projection[2][3] + projection[2][1];
	planes[FRUSTUM_BOTTOM].dist =		-(projection[3][3] + projection[3][1]);

	// top
	planes[FRUSTUM_TOP].normal[0]	=  	  projection[0][3] - projection[0][1];
	planes[FRUSTUM_TOP].normal[1]	=  	  projection[1][3] - projection[1][1];
	planes[FRUSTUM_TOP].normal[2]	=  	  projection[2][3] - projection[2][1];
	planes[FRUSTUM_TOP].dist =		-(projection[3][3] - projection[3][1]);

	// near
	planes[FRUSTUM_NEAR].normal[0] =	  projection[0][3] + projection[0][2];
	planes[FRUSTUM_NEAR].normal[1] =	  projection[1][3] + projection[1][2];
	planes[FRUSTUM_NEAR].normal[2] =	  projection[2][3] + projection[2][2];
	planes[FRUSTUM_NEAR].dist =		-(projection[3][3] + projection[3][2]);

	// far
	planes[FRUSTUM_FAR].normal[0]	=  	  projection[0][3] - projection[0][2];
	planes[FRUSTUM_FAR].normal[1]	=  	  projection[1][3] - projection[1][2];
	planes[FRUSTUM_FAR].normal[2]	=  	  projection[2][3] - projection[2][2];
	planes[FRUSTUM_FAR].dist =		-(projection[3][3] - projection[3][2]);

	for( int i = 0; i < FRUSTUM_PLANES; i++ )
	{
		NormalizePlane( i );
	}
}

void CFrustum :: SetPlane( int side, const Vector &vecNormal, float flDist )
{
	assert( side >= 0 && side < FRUSTUM_PLANES );

	planes[side].type = PlaneTypeForNormal( vecNormal );
	planes[side].signbits = SignbitsForPlane( vecNormal );
	planes[side].normal = vecNormal;
	planes[side].dist = flDist;

	clipFlags |= BIT( side );
}

void CFrustum :: NormalizePlane( int side )
{
	assert( side >= 0 && side < FRUSTUM_PLANES );

	// normalize
	float length = planes[side].normal.Length();

	if( length )
	{
		float ilength = (1.0f / length);
		planes[side].normal.x *= ilength;
		planes[side].normal.y *= ilength;
		planes[side].normal.z *= ilength;
		planes[side].dist *= ilength;
	}

	planes[side].type = PlaneTypeForNormal( planes[side].normal );
	planes[side].signbits = SignbitsForPlane( planes[side].normal );

	clipFlags |= BIT( side );
}

void CFrustum :: ComputeFrustumCorners( Vector corners[8] ) const
{
	memset( corners, 0, sizeof( Vector ) * 8 );

	PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_TOP], &planes[FRUSTUM_FAR], corners[0] );
	PlanesGetIntersectionPoint( &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_TOP], &planes[FRUSTUM_FAR], corners[1] );
	PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_BOTTOM], &planes[FRUSTUM_FAR], corners[2] );
	PlanesGetIntersectionPoint( &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_BOTTOM], &planes[FRUSTUM_FAR], corners[3] );

	if( FBitSet( clipFlags, BIT( FRUSTUM_NEAR )))
	{
		PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_TOP], &planes[FRUSTUM_NEAR], corners[4] );
		PlanesGetIntersectionPoint( &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_TOP], &planes[FRUSTUM_NEAR], corners[5] );
		PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_BOTTOM], &planes[FRUSTUM_NEAR], corners[6] );
		PlanesGetIntersectionPoint( &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_BOTTOM], &planes[FRUSTUM_NEAR], corners[7] );
	}
	else
	{
		PlanesGetIntersectionPoint( &planes[FRUSTUM_LEFT], &planes[FRUSTUM_RIGHT], &planes[FRUSTUM_TOP], corners[4] );
		corners[7] = corners[6] = corners[5] = corners[4];
	}
}

void CFrustum :: ComputeFrustumBounds( Vector &mins, Vector &maxs ) const
{
	Vector	corners[8];

	ComputeFrustumCorners( corners );

	ClearBounds( mins, maxs );

	for( int i = 0; i < 8; i++ )
		AddPointToBounds( corners[i], mins, maxs );
}

// faster implementation, but may fail in case when frustum smaller than bounds
bool CFrustum :: CullBoxFast( const Vector &mins, const Vector &maxs, int userClipFlags ) const
{
	int iClipFlags;

	if( userClipFlags != 0 )
		iClipFlags = userClipFlags;
	else iClipFlags = clipFlags;

	for( int i = 0; i < FRUSTUM_PLANES; i++ )
	{
		if( !FBitSet( iClipFlags, BIT( i )))
			continue;

		const mplane_t *p = &planes[i];

		switch( p->signbits )
		{
		case 0:
			if( p->normal.x * maxs.x + p->normal.y * maxs.y + p->normal.z * maxs.z < p->dist )
				return true;
			break;
		case 1:
			if( p->normal.x * mins.x + p->normal.y * maxs.y + p->normal.z * maxs.z < p->dist )
				return true;
			break;
		case 2:
			if( p->normal.x * maxs.x + p->normal.y * mins.y + p->normal.z * maxs.z < p->dist )
				return true;
			break;
		case 3:
			if( p->normal.x * mins.x + p->normal.y * mins.y + p->normal.z * maxs.z < p->dist )
				return true;
			break;
		case 4:
			if( p->normal.x * maxs.x + p->normal.y * maxs.y + p->normal.z * mins.z < p->dist )
				return true;
			break;
		case 5:
			if( p->normal.x * mins.x + p->normal.y * maxs.y + p->normal.z * mins.z < p->dist )
				return true;
			break;
		case 6:
			if( p->normal.x * maxs.x + p->normal.y * mins.y + p->normal.z * mins.z < p->dist )
				return true;
			break;
		case 7:
			if( p->normal.x * mins.x + p->normal.y * mins.y + p->normal.z * mins.z < p->dist )
				return true;
			break;
		default:
			return false;
		}
	}

	return false;
}

// https://iquilezles.org/articles/frustumcorrect/
bool CFrustum::CullBoxSafe( const CBoundingBox &bounds ) const
{
	const vec3_t &mins = bounds.GetMins();
	const vec3_t &maxs = bounds.GetMaxs();

	for (int32_t i = 0; i < FRUSTUM_PLANES; i++)
	{
		if (!FBitSet(clipFlags, BIT(i))) {
			continue;
		}

		// assumed that plane normal vectors directed inside frustum
		int32_t out = 0;
		const mplane_t *plane = &planes[i];
        out += (DotProduct(plane->normal, Vector(mins.x, mins.y, mins.z) - plane->dist) < 0.0) ? 1 : 0;
		out += (DotProduct(plane->normal, Vector(maxs.x, mins.y, mins.z) - plane->dist) < 0.0) ? 1 : 0;
		out += (DotProduct(plane->normal, Vector(mins.x, maxs.y, mins.z) - plane->dist) < 0.0) ? 1 : 0;
		out += (DotProduct(plane->normal, Vector(maxs.x, maxs.y, mins.z) - plane->dist) < 0.0) ? 1 : 0;
		out += (DotProduct(plane->normal, Vector(mins.x, mins.y, maxs.z) - plane->dist) < 0.0) ? 1 : 0;
		out += (DotProduct(plane->normal, Vector(maxs.x, mins.y, maxs.z) - plane->dist) < 0.0) ? 1 : 0;
		out += (DotProduct(plane->normal, Vector(mins.x, maxs.y, maxs.z) - plane->dist) < 0.0) ? 1 : 0;
		out += (DotProduct(plane->normal, Vector(maxs.x, maxs.y, maxs.z) - plane->dist) < 0.0) ? 1 : 0;

		if (out == 8) {
			return true;
		}
	}
	return false;
}

bool CFrustum :: CullSphere( const Vector &centre, float radius, int userClipFlags ) const
{
	int iClipFlags;

	if( userClipFlags != 0 )
		iClipFlags = userClipFlags;
	else iClipFlags = clipFlags;

	for( int i = 0; i < FRUSTUM_PLANES; i++ )
	{
		if( !FBitSet( iClipFlags, BIT( i )))
			continue;

		const mplane_t *p = &planes[i];

		if( DotProduct( centre, p->normal ) - p->dist <= -radius )
			return true;
	}

	return false;
}

// FIXME: could be optimized?
bool CFrustum :: CullFrustum( const CFrustum *frustum ) const
{
	Vector bbox[8];

	// check for self intersects other
	frustum->ComputeFrustumCorners( bbox );

	for (int i = 0; i < FRUSTUM_PLANES; i++)
	{
		if (!FBitSet(clipFlags, BIT(i)))
			continue;

		const mplane_t *p = &planes[i];
		bool pointsOutside = true;

		for (int j = 0; j < 8; j++)
		{
			// at least one corner point of other frustum lies in front of other frustum plane
			if (DotProduct(bbox[j], p->normal) - p->dist >= DIST_EPSILON) {
				pointsOutside = false;
				break;
			}
		}

		if (pointsOutside)
			return true; // found separating plane, there is no intersection
	}

	// check for other intersects self
	ComputeFrustumCorners( bbox );

	for (int i = 0; i < FRUSTUM_PLANES; i++)
	{
		if (!FBitSet(frustum->clipFlags, BIT(i)))
			continue;

		const mplane_t *p = &frustum->planes[i];
		bool pointsOutside = true;

		for (int j = 0; j < 8; j++)
		{
			// at least one corner point of other frustum lies in front of other frustum plane
			if (DotProduct(bbox[j], p->normal) - p->dist >= DIST_EPSILON) {
				pointsOutside = false;
				break;
			}
		}

		if (pointsOutside)
			return true; // found separating plane, there is no intersection
	}

	return false;
}
