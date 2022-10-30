//=======================================================================
//			Copyright (C) Shambler Team 2005
//		          mathlib.cpp - shared math library
//=======================================================================

#include "mathlib.h"
#include "const.h"
#include "com_model.h"
#include <math.h>

const Vector g_vecZero( 0, 0, 0 );
const Radian g_radZero( 0, 0, 0 );

/*
=================
SignbitsForPlane

fast box on planeside test
=================
*/
int SignbitsForPlane( const Vector &normal )
{
	int bits, i;
	for( bits = 0, i = 0; i < 3; i++ )
		if( normal[i] < 0.0f )
			bits |= 1<<i;
	return bits;
}

/*
=================
PlaneTypeForNormal
=================
*/
int PlaneTypeForNormal( const Vector &normal )
{
	if( normal.x == 1.0f )
		return PLANE_X;
	if( normal.y == 1.0 )
		return PLANE_Y;
	if( normal.z == 1.0 )
		return PLANE_Z;
	return PLANE_NONAXIAL;
}

/*
=================
SetPlane
=================
*/
void SetPlane( mplane_t *plane, const Vector &vecNormal, float flDist )
{
	plane->type = PlaneTypeForNormal( vecNormal );
	plane->signbits = SignbitsForPlane( vecNormal );
	plane->normal = vecNormal;
	plane->dist = flDist;
}
	
/*
=================
PlanesGetIntersectionPoint

=================
*/
bool PlanesGetIntersectionPoint( const mplane_t *plane1, const mplane_t *plane2, const mplane_t *plane3, Vector &out )
{
	Vector n1 = plane1->normal.Normalize();
	Vector n2 = plane2->normal.Normalize();
	Vector n3 = plane3->normal.Normalize();

	Vector n1n2 = CrossProduct( n1, n2 );
	Vector n2n3 = CrossProduct( n2, n3 );
	Vector n3n1 = CrossProduct( n3, n1 );

	float denom = DotProduct( n1, n2n3 );
	out = g_vecZero;

	// check if the denominator is zero (which would mean that no intersection is to be found
	if( denom == 0.0f )
	{
		// no intersection could be found, return <0,0,0>
		return false;
	}

	// compute intersection point
	out += n2n3 * plane1->dist;
	out += n3n1 * plane2->dist;
	out += n1n2 * plane3->dist;
	out *= (1.0f / denom );

	return true;
}

/*
=================
PlaneIntersect

find point where ray
was intersect with plane
=================
*/
Vector PlaneIntersect( mplane_t *plane, const Vector& p0, const Vector& p1 )
{
	float distToPlane = PlaneDiff( p0, plane );
	float planeDotRay = DotProduct( plane->normal, p1 );
	float sect = -(distToPlane) / planeDotRay;

	return p0 + p1 * sect;
}

/*
=================
VectorAngles
This version without stupid quake bug
=================
*/
void VectorAngles( const Vector &srcforward, Vector &angles )
{
	Vector	forward = srcforward;

	angles[ROLL] = 0.0f;

	if( forward.x || forward.y )
	{
		float tmp;

		angles[YAW] = RAD2DEG( atan2( forward.y, forward.x ));
		if( angles[YAW] < 0 ) angles[YAW] += 360;

		tmp = sqrt( forward.x * forward.x + forward.y * forward.y );
		angles[PITCH] = RAD2DEG( atan2( -forward.z, tmp ));
		if( angles[PITCH] < 0 ) angles[PITCH] += 360;
	}
	else
	{
		// fast case
		angles[YAW] = 0.0f;
		if( forward.z > 0 )
			angles[PITCH] = 270.0f;
		else angles[PITCH] = 90.0f;
	}
}

/*
=================
VectorAnglesSQB
This version with stupid quake bug
=================
*/
void VectorAnglesSQB(const Vector &forward, Vector &angles)
{
	angles[ROLL] = 0.0f;

	if (forward.x || forward.y)
	{
		float tmp;

		angles[YAW] = RAD2DEG(atan2(forward.y, forward.x));
		if (angles[YAW] < 0.0f) angles[YAW] += 360.0f;

		tmp = sqrt(forward.x * forward.x + forward.y * forward.y);
		angles[PITCH] = RAD2DEG(atan2(forward.z, tmp));
		if (angles[PITCH] < 0.0f) angles[PITCH] += 360.0f;
	}
	else
	{
		// fast case
		angles[YAW] = 0.0f;
		if (forward.z > 0.0f)
			angles[PITCH] = 270.0f;
		else angles[PITCH] = 90.0f;
	}
}

/*
=================
NearestPOW
=================
*/
int NearestPOW( int value, bool roundDown )
{
	int	n = 1;

	if( value <= 0 ) return 1;
	while( n < value ) n <<= 1;

	if( roundDown )
	{
		if( n > value ) n >>= 1;
	}
	return n;
}

void SetPlane(mplane_t *plane, const Vector &vecNormal, float flDist, int type)
{
	if (type == -1)
		plane->type = PlaneTypeForNormal(vecNormal);
	else plane->type = type;
	plane->signbits = SignbitsForPlane(vecNormal);
	plane->normal = vecNormal;
	plane->dist = flDist;
}

//-----------------------------------------------------------------------------
// Transforms a AABB into another space; which will inherently grow the box.
//-----------------------------------------------------------------------------
void TransformAABB( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &absmin, Vector &absmax )
{
	Vector localCenter = (mins + maxs) * 0.5f;
	Vector localExtents = maxs - localCenter;
	Vector worldCenter = world.VectorTransform( localCenter );
	matrix4x4 iworld = world.Transpose();
	Vector worldExtents;

	worldExtents.x = DotProductAbs( localExtents, iworld.GetForward( ));
	worldExtents.y = DotProductAbs( localExtents, iworld.GetRight( ));
	worldExtents.z = DotProductAbs( localExtents, iworld.GetUp( ));

	absmin = worldCenter - worldExtents;
	absmax = worldCenter + worldExtents;
}

/*
==================
TransformAABBLocal
==================
*/
void TransformAABBLocal( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &outmins, Vector &outmaxs )
{
	matrix4x4	im = world.Invert();
	ClearBounds( outmins, outmaxs );
	Vector p1, p2;
	int i;

	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
		p1.x = ( i & 1 ) ? mins.x : maxs.x;
		p1.y = ( i & 2 ) ? mins.y : maxs.y;
		p1.z = ( i & 4 ) ? mins.z : maxs.z;

		p2.x = DotProduct( p1, im[0] );
		p2.y = DotProduct( p1, im[1] );
		p2.z = DotProduct( p1, im[2] );

		if( p2.x < outmins.x ) outmins.x = p2.x;
		if( p2.x > outmaxs.x ) outmaxs.x = p2.x;
		if( p2.y < outmins.y ) outmins.y = p2.y;
		if( p2.y > outmaxs.y ) outmaxs.y = p2.y;
		if( p2.z < outmins.z ) outmins.z = p2.z;
		if( p2.z > outmaxs.z ) outmaxs.z = p2.z;
	}

	// sanity check
	for( i = 0; i < 3; i++ )
	{
		if( outmins[i] > outmaxs[i] )
		{
			outmins = g_vecZero;
			outmaxs = g_vecZero;
			return;
		}
	}
}

float CalcSqrDistanceToAABB(const Vector &mins, const Vector &maxs, const Vector &point)
{
	float flDelta;
	float flDistSqr = 0.0f;

	if (point.x < mins.x)
	{
		flDelta = (mins.x - point.x);
		flDistSqr += flDelta * flDelta;
	}
	else if (point.x > maxs.x)
	{
		flDelta = (point.x - maxs.x);
		flDistSqr += flDelta * flDelta;
	}

	if (point.y < mins.y)
	{
		flDelta = (mins.y - point.y);
		flDistSqr += flDelta * flDelta;
	}
	else if (point.y > maxs.y)
	{
		flDelta = (point.y - maxs.y);
		flDistSqr += flDelta * flDelta;
	}

	if (point.z < mins.z)
	{
		flDelta = (mins.z - point.z);
		flDistSqr += flDelta * flDelta;
	}
	else if (point.z > maxs.z)
	{
		flDelta = (point.z - maxs.z);
		flDistSqr += flDelta * flDelta;
	}

	return flDistSqr;
}

void RotatePointAroundVector(Vector &dst, const Vector &dir, const Vector &point, float degrees)
{
	float	t0, t1;
	float	angle, c, s;
	Vector	vr, vu, vf = dir;

	angle = DEG2RAD(degrees);
	SinCos(angle, &s, &c);
	VectorMatrix(vf, vr, vu);

	t0 = vr.x * c + vu.x * -s;
	t1 = vr.x * s + vu.x * c;
	dst.x = (t0 * vr.x + t1 * vu.x + vf.x * vf.x) * point.x
		+ (t0 * vr.y + t1 * vu.y + vf.x * vf.y) * point.y
		+ (t0 * vr.z + t1 * vu.z + vf.x * vf.z) * point.z;

	t0 = vr.y * c + vu.y * -s;
	t1 = vr.y * s + vu.y * c;
	dst.y = (t0 * vr.x + t1 * vu.x + vf.y * vf.x) * point.x
		+ (t0 * vr.y + t1 * vu.y + vf.y * vf.y) * point.y
		+ (t0 * vr.z + t1 * vu.z + vf.y * vf.z) * point.z;

	t0 = vr.z * c + vu.z * -s;
	t1 = vr.z * s + vu.z * c;
	dst.z = (t0 * vr.x + t1 * vu.x + vf.z * vf.x) * point.x
		+ (t0 * vr.y + t1 * vu.y + vf.z * vf.y) * point.y
		+ (t0 * vr.z + t1 * vu.z + vf.z * vf.z) * point.z;
}

// returns true if the sphere and cone intersect
// NOTE: cone sine/cosine are the half angle of the cone
bool IsSphereIntersectingCone(const Vector &sphereCenter, float sphereRadius, const Vector &coneOrigin, const Vector &coneNormal, float coneSine, float coneCosine)
{
	Vector backCenter = coneOrigin - (sphereRadius / coneSine) * coneNormal;
	Vector delta = sphereCenter - backCenter;
	float deltaLen = delta.Length();

	if (DotProduct(coneNormal, delta) >= deltaLen * coneCosine)
	{
		delta = sphereCenter - coneOrigin;
		deltaLen = delta.Length();
		if (-DotProduct(coneNormal, delta) >= deltaLen * coneSine)
			return (deltaLen <= sphereRadius) ? true : false;
		return true;
	}

	return false;
}

void CalcClosestPointOnAABB( const Vector &mins, const Vector &maxs, const Vector &point, Vector &closestOut )
{
	closestOut.x = bound( mins.x, point.x, maxs.x );
	closestOut.y = bound( mins.y, point.y, maxs.y );
	closestOut.z = bound( mins.z, point.z, maxs.z );
}

//
// bounds operations
//
/*
=================
ClearBounds
=================
*/
void ClearBounds( Vector &mins, Vector &maxs )
{
	// make bogus range
	mins.x = mins.y = mins.z =  999999;
	maxs.x = maxs.y = maxs.z = -999999;
}

/*
=================
ClearBounds
=================
*/
void ClearBounds( Vector2D &mins, Vector2D &maxs )
{
	// make bogus range
	mins.x = mins.y =  999999;
	maxs.x = maxs.y = -999999;
}

bool BoundsIsCleared( const Vector &mins, const Vector &maxs )
{
	if( mins.x <= maxs.x || mins.y <= maxs.y || mins.z <= maxs.z )
		return false;
	return true;
}

bool BoundsIsNull(const Vector &mins, const Vector &maxs)
{
	if (mins.x != maxs.x || mins.y != maxs.y || mins.z != maxs.z)
		return false;
	return true;
}

/*
=================
ExpandBounds
=================
*/
void ExpandBounds( Vector &mins, Vector &maxs, float offset )
{
	mins[0] -= offset;
	mins[1] -= offset;
	mins[2] -= offset;
	maxs[0] += offset;
	maxs[1] += offset;
	maxs[2] += offset;
}

/*
=================
AddPointToBounds
=================
*/
void AddPointToBounds( const Vector &v, Vector &mins, Vector &maxs, float limit )
{
	if( limit )
	{
		for( int i = 0; i < 3; i++ )
		{
			if( v[i] < mins[i] ) mins[i] = Q_max( v[i], mins[i] - limit );
			if( v[i] > maxs[i] ) maxs[i] = Q_min( v[i], maxs[i] + limit );
		}
	}
	else
	{
		for( int i = 0; i < 3; i++ )
		{
			if( v[i] < mins[i] ) mins[i] = v[i];
			if( v[i] > maxs[i] ) maxs[i] = v[i];
		}
	}
}

/*
=================
AddPointToBounds
=================
*/
void AddPointToBounds( const Vector2D &v, Vector2D &mins, Vector2D &maxs )
{
	for( int i = 0; i < 2; i++ )
	{
		if( v[i] < mins[i] ) mins[i] = v[i];
		if( v[i] > maxs[i] ) maxs[i] = v[i];
	}
}

/*
=================
BoundsIntersect
=================
*/
bool BoundsIntersect( const Vector &mins1, const Vector &maxs1, const Vector &mins2, const Vector &maxs2 )
{
	if( mins1.x > maxs2.x || mins1.y > maxs2.y || mins1.z > maxs2.z )
		return false;
	if( maxs1.x < mins2.x || maxs1.y < mins2.y || maxs1.z < mins2.z )
		return false;
	return true;
}

/*
=================
BoundsIntersect
=================
*/
bool BoundsIntersect( const Vector2D &mins1, const Vector2D &maxs1, const Vector2D &mins2, const Vector2D &maxs2 )
{
	if( mins1.x > maxs2.x || mins1.y > maxs2.y )
		return false;
	if( maxs1.x < mins2.x || maxs1.y < mins2.y )
		return false;
	return true;
}

/*
=================
BoundsAndSphereIntersect
=================
*/
bool BoundsAndSphereIntersect( const Vector &mins, const Vector &maxs, const Vector &origin, float radius )
{
	if( mins.x > origin.x + radius || mins.y > origin.y + radius || mins.z > origin.z + radius )
		return false;
	if( maxs.x < origin.x - radius || maxs.y < origin.y - radius || maxs.z < origin.z - radius )
		return false;
	return true;
}


/*
=================
BoundsAndSphereIntersect
=================
*/
bool BoundsAndSphereIntersect( const Vector2D &mins, const Vector2D &maxs, const Vector2D &origin, float radius )
{
	if( mins.x > origin.x + radius || mins.y > origin.y + radius )
		return false;
	if( maxs.x < origin.x - radius || maxs.y < origin.y - radius )
		return false;
	return true;
}
/*
=================
PointInBounds
=================
*/
bool PointInBounds( const Vector& pt, const Vector& boxMin, const Vector& boxMax )
{
	if(( pt[0] > boxMax[0] ) || ( pt[0] < boxMin[0] ))
		return false;
	if(( pt[1] > boxMax[1] ) || ( pt[1] < boxMin[1] ))
		return false;
	if(( pt[2] > boxMax[2] ) || ( pt[2] < boxMin[2] ))
		return false;
	return true;
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds( const Vector &mins, const Vector &maxs )
{
	Vector	corner;

	for( int i = 0; i < 3; i++ )
	{
		corner[i] = fabs( mins[i] ) > fabs( maxs[i] ) ? fabs( mins[i] ) : fabs( maxs[i] );
	}
	return corner.Length();
}

//
// quaternion operations
//
/*
====================
AngleQuaternion

degrees euler XYZ version
====================
*/
void AngleQuaternion( const Vector &angles, Vector4D &quat )
{
	float sr, sp, sy, cr, cp, cy;

	SinCos( DEG2RAD( angles.y ) * 0.5f, &sy, &cy );
	SinCos( DEG2RAD( angles.x ) * 0.5f, &sp, &cp );
	SinCos( DEG2RAD( angles.z ) * 0.5f, &sr, &cr );

	float srXcp = sr * cp, crXsp = cr * sp;
	quat.x = srXcp * cy - crXsp * sy; // X
	quat.y = crXsp * cy + srXcp * sy; // Y

	float crXcp = cr * cp, srXsp = sr * sp;
	quat.z = crXcp * sy - srXsp * cy; // Z
	quat.w = crXcp * cy + srXsp * sy; // W (real component)
}

/*
====================
AngleQuaternion

radian euler YZX version
====================
*/
void AngleQuaternion( const Radian &angles, Vector4D &quat )
{
	float sr, sp, sy, cr, cp, cy;

	SinCos( angles.z * 0.5f, &sy, &cy );
	SinCos( angles.y * 0.5f, &sp, &cp );
	SinCos( angles.x * 0.5f, &sr, &cr );

	float srXcp = sr * cp, crXsp = cr * sp;
	quat.x = srXcp * cy - crXsp * sy; // X
	quat.y = crXsp * cy + srXcp * sy; // Y

	float crXcp = cr * cp, srXsp = sr * sp;
	quat.z = crXcp * sy - srXsp * cy; // Z
	quat.w = crXcp * cy + srXsp * sy; // W (real component)
}

/*
====================
QuaternionAngle

====================
*/
void QuaternionAngle( const Vector4D &quat, Vector &angles )
{
	// g-cont. it's incredible stupid way but...
	matrix3x3	temp( quat );
	temp.GetAngles( angles );
}

/*
====================
QuaternionAngle

====================
*/
void QuaternionAngle( const Vector4D &quat, Radian &angles )
{
	// g-cont. it's incredible stupid way but...
	matrix3x3	temp( quat );
	temp.GetAngles( angles );
}

/*
====================
QuaternionAlign

make sure quaternions are within 180 degrees of one another,
if not, reverse q
====================
*/
void QuaternionAlign( const Vector4D &p, const Vector4D &q, Vector4D &qt )
{
	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;
	int i;

	for( i = 0; i < 4; i++ ) 
	{
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}

	if( a > b ) 
	{
		for( i = 0; i < 4; i++ ) 
		{
			qt[i] = -q[i];
		}
	}
	else if( &qt != &q )
	{
		for( i = 0; i < 4; i++ ) 
		{
			qt[i] = q[i];
		}
	}
}

/*
====================
QuaternionSlerpNoAlign
====================
*/
void QuaternionSlerpNoAlign( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt )
{
	float omega, cosom, sinom, sclp, sclq;

	// 0.0 returns p, 1.0 return q.
	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if(( 1.0f + cosom ) > 0.000001f )
	{
		if(( 1.0f - cosom ) > 0.000001f )
		{
			omega = acos( cosom );
			sinom = sin( omega );
			sclp = sin( (1.0f - t) * omega) / sinom;
			sclq = sin( t * omega ) / sinom;
		}
		else
		{
			// TODO: add short circuit for cosom == 1.0f?
			sclp = 1.0f - t;
			sclq = t;
		}

		for( int i = 0; i < 4; i++ )
		{
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = sin(( 1.0f - t ) * ( 0.5f * M_PI ));
		sclq = sin( t * ( 0.5f * M_PI ));

		for( int i = 0; i < 3; i++ )
		{
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}

/*
====================
QuaternionSlerp

Quaternion sphereical linear interpolation
====================
*/
void QuaternionSlerp( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt )
{
	Vector4D	q2;

	// 0.0 returns p, 1.0 return q.
	// decide if one of the quaternions is backwards
	QuaternionAlign( p, q, q2 );

	QuaternionSlerpNoAlign( p, q2, t, qt );
}

/*
====================
QuaternionSlerp

Quaternion sphereical linear interpolation
====================
*/
void QuaternionSlerp( const Radian &r0, const Radian &r1, float t, Radian &r2 )
{
	Vector4D	q0, q1, q2;

	AngleQuaternion( r0, q0 );
	AngleQuaternion( r1, q1 );
	QuaternionSlerp( q0, q1, t, q2 );
	QuaternionAngle( q2, r2 );
}

/*
====================
QuaternionBlend
====================
*/
void QuaternionBlend( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt )
{
	// decide if one of the quaternions is backwards
	Vector4D	q2;

	QuaternionAlign( p, q, q2 );
	QuaternionBlendNoAlign( p, q2, t, qt );
}

/*
====================
QuaternionBlendNoAlign
====================
*/
void QuaternionBlendNoAlign( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt )
{
	float	sclp, sclq;

	// 0.0 returns p, 1.0 return q.
	sclp = 1.0f - t;
	sclq = t;

	for( int i = 0; i < 4; i++ )
	{
		qt[i] = sclp * p[i] + sclq * q[i];
	}

	qt = qt.Normalize();
}

void QuaternionAdd( const Vector4D &p, const Vector4D &q, Vector4D &qt )
{
	Vector4D	q2;
	QuaternionAlign( p, q, q2 );
	qt = p + q2;
}

/*
====================
QuaternionMultiply

multiply two quaternions
====================
*/
void QuaternionMultiply( const Vector4D &q1, const Vector4D &q2, Vector4D &out )
{
	out[0] = q1[3] * q2[0] + q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1];
	out[1] = q1[3] * q2[1] + q1[1] * q2[3] + q1[2] * q2[0] - q1[0] * q2[2];
	out[2] = q1[3] * q2[2] + q1[2] * q2[3] + q1[0] * q2[1] - q1[1] * q2[0];
	out[3] = q1[3] * q2[3] - q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2];
}

/*
====================
QuaternionVectorTransform

transform vector by quaternion
====================
*/
void QuaternionVectorTransform( const Vector4D &q, const Vector &v, Vector &out )
{
	float	wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	// 9 muls, 3 adds
	x2 = q[0] + q[0]; y2 = q[1] + q[1]; z2 = q[2] + q[2];
	xx = q[0] * x2; xy = q[0] * y2; xz = q[0] * z2;
	yy = q[1] * y2; yz = q[1] * z2; zz = q[2] * z2;
	wx = q[3] * x2; wy = q[3] * y2; wz = q[3] * z2;

	// 9 muls, 9 subs, 9 adds
	out[0] = ( 1.0f - yy - zz ) * v[0] + ( xy - wz ) * v[1] + ( xz + wy ) * v[2];
	out[1] = ( xy + wz ) * v[0] + ( 1.0f - xx - zz ) * v[1] + ( yz - wx ) * v[2];
	out[2] = ( xz - wy ) * v[0] + ( yz + wx ) * v[1] + ( 1.0f - xx - yy ) * v[2];
}

/*
====================
QuaternionConcatTransforms

transform quat\vector by another quat\vector
====================
*/
void QuaternionConcatTransforms( const Vector4D &q1, const Vector &v1, const Vector4D &q2, const Vector &v2, Vector4D &q, Vector &v )
{
	QuaternionMultiply( q1, q2, q );
	QuaternionVectorTransform( q1, v2, v );
	v += v1;
}

void QuaternionMult( const Vector4D &p, const Vector4D &q, Vector4D &qt )
{
	if( &p == &qt )
	{
		Vector4D	p2 = p;
		QuaternionMult( p2, q, qt );
		return;
	}

	// decide if one of the quaternions is backwards
	Vector4D	q2;

	QuaternionAlign( p, q, q2 );
	qt.x =  p.x * q2.w + p.y * q2.z - p.z * q2.y + p.w * q2.x;
	qt.y = -p.x * q2.z + p.y * q2.w + p.z * q2.x + p.w * q2.y;
	qt.z =  p.x * q2.y - p.y * q2.x + p.z * q2.w + p.w * q2.z;
	qt.w = -p.x * q2.x - p.y * q2.y - p.z * q2.z + p.w * q2.w;
}

void QuaternionScale( const Vector4D &p, float t, Vector4D &q )
{
	float r;

	// FIXME: nick, this isn't overly sensitive to accuracy, and it may be faster to 
	// use the cos part (w) of the quaternion (sin(omega)*N,cos(omega)) to figure the new scale.
#if 1
	Vector ps = Vector( p.x, p.y, p.z );
	float sinom = ps.Length(); // !!!
#else
	float sinom = p.Length(); // !!!
#endif
	sinom = Q_min( sinom, 1.0f );
	float sinsom = sin( asin( sinom ) * t );

	t = sinsom / (sinom + FLT_EPSILON);
	q.x = p.x * t;
	q.y = p.y * t;
	q.z = p.z * t;

	// rescale rotation
	r = 1.0f - sinsom * sinsom;

	if( r < 0.0f ) 
		r = 0.0f;
	r = sqrt( r );

	// keep sign of rotation
	if( p.w < 0 ) q.w = -r;
	else q.w = r;
}

//-----------------------------------------------------------------------------
// Purpose: Converts an exponential map (ang/axis) to a quaternion
//-----------------------------------------------------------------------------
void AxisAngleQuaternion( const Vector &axis, float angle, Vector4D &q )
{
	float	sa, ca;

	SinCos( DEG2RAD( angle ) * 0.5f, &sa, &ca );

	q.x = axis.x * sa;
	q.y = axis.y * sa;
	q.z = axis.z * sa;
	q.w = ca;
}

//-----------------------------------------------------------------------------
// Purpose: qt = ( s * p ) * q
//-----------------------------------------------------------------------------
void QuaternionSM( float s, const Vector4D &p, const Vector4D &q, Vector4D &qt )
{
	Vector4D	p1, q1;

	QuaternionScale( p, s, p1 );
	QuaternionMult( p1, q, q1 );
	qt = q1.Normalize();
}

//-----------------------------------------------------------------------------
// Purpose: qt = p * ( s * q )
//-----------------------------------------------------------------------------
void QuaternionMA( const Vector4D &p, float s, const Vector4D &q, Vector4D &qt )
{
	Vector4D p1, q1;

	QuaternionScale( q, s, q1 );
	QuaternionMult( p, q1, p1 );
	qt = p1.Normalize();
}

//-----------------------------------------------------------------------------
// Purpose: qt = p + s * q
//-----------------------------------------------------------------------------
void QuaternionAccumulate( const Vector4D &p, float s, const Vector4D &q, Vector4D &qt )
{
	Vector4D	q2;

	QuaternionAlign( p, q, q2 );
	qt[0] = p[0] + s * q2[0];
	qt[1] = p[1] + s * q2[1];
	qt[2] = p[2] + s * q2[2];
	qt[3] = p[3] + s * q2[3];
}

void QuaternionConjugate( const Vector4D &p, Vector4D &q )
{
	q.x = -p.x;
	q.y = -p.y;
	q.z = -p.z;
	q.w = p.w;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the angular delta between the two normalized quaternions in degrees.
//-----------------------------------------------------------------------------
float QuaternionAngleDiff( const Vector4D &p, const Vector4D &q )
{
	Vector4D qInv, diff;
	QuaternionConjugate( q, qInv );
	QuaternionMult( p, qInv, diff );

	float sinang = sqrt( diff.x * diff.x + diff.y * diff.y + diff.z * diff.z );
	float angle = RAD2DEG( 2 * asin( sinang ) );
	return angle;
}

//
// lerping stuff
//

/*
===================
InterpolateOrigin

Interpolate position.
Frac is 0.0 to 1.0
===================
*/
void InterpolateOrigin( const Vector& start, const Vector& end, Vector& output, float frac, bool back )
{
	if( back ) output += frac * ( end - start );
	else output = start + frac * ( end - start );
}

/*
===================
InterpolateAngles

Interpolate Euler angles.
Frac is 0.0 to 1.0 ( i.e., should probably be clamped, but doesn't have to be )
===================
*/
void InterpolateAngles( const Vector& start, const Vector& end, Vector& output, float frac, bool back )
{
#if 0
	Vector4D src, dest;

	// convert to quaternions
	AngleQuaternion( start, src );
	AngleQuaternion( end, dest );

	Vector4D result;
	Vector out;

	// slerp
	QuaternionSlerp( src, dest, frac, result );

	// convert to euler
	QuaternionAngle( result, out );

	if( back ) output += out;
	else output = out;
#else
	for( int i = 0; i < 3; i++ )
	{
		float	ang1, ang2;

		ang1 = end[i];
		ang2 = start[i];

		float d = ang1 - ang2;

		if( d > 180 ) d -= 360;
		else if( d < -180 ) d += 360;

		output[i] += d * frac;
	}
#endif
}

void NormalizeAngles( Vector &angles )
{
	for( int i = 0; i < 3; i++ )
	{
		if( angles[i] > 180.0f )
		{
			angles[i] -= 360.0f;
		}
		else if( angles[i] < -180.0f )
		{
			angles[i] += 360.0f;
		}
	}
}

/*
===================
AngleDiff

===================
*/
float AngleDiff( float destAngle, float srcAngle )
{
	float delta;

	delta = fmodf( destAngle - srcAngle, 360.0f );

	if( destAngle > srcAngle )
	{
		if( delta >= 180 )
			delta -= 360;
	}
	else
	{
		if( delta <= -180 )
			delta += 360;
	}

	return delta;
}

/*
===================
AngleNormalize

===================
*/
float AngleNormalize( float angle )
{
	angle = fmodf( angle, 360.0f );

	if( angle > 180 ) 
	{
		angle -= 360;
	}

	if( angle < -180 )
	{
		angle += 360;
	}

	return angle;
}

/*
===================
AngleBetweenVectors

===================
*/
float AngleBetweenVectors( const Vector v1, const Vector v2 )
{
	float l1 = v1.Length();
	float l2 = v2.Length();

	if( !l1 || !l2 )
		return 0.0f;

	float angle = acos( DotProduct( v1, v2 )) / (l1 * l2);

	return RAD2DEG( angle );
}

void VectorMatrix( Vector &forward, Vector &right, Vector &up )
{
	if( forward.x || forward.y )
	{
		right = Vector( forward.y, -forward.x, 0 );
		right = right.Normalize();
		up = CrossProduct( forward, right );
	}
	else
	{
		right = Vector( 1.0f, 0.0f, 0.0f );
		up = Vector( 0.0f, 1.0f, 0.0f );
	}
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide( const Vector &emins, const Vector &emaxs, const mplane_t *p )
{
	float	dist1, dist2;
	int	sides = 0;

	// general case
	switch( p->signbits )
	{
	case 0:
		dist1 = p->normal.x * emaxs.x + p->normal.y * emaxs.y + p->normal.z * emaxs.z;
		dist2 = p->normal.x * emins.x + p->normal.y * emins.y + p->normal.z * emins.z;
		break;
	case 1:
		dist1 = p->normal.x * emins.x + p->normal.y * emaxs.y + p->normal.z * emaxs.z;
		dist2 = p->normal.x * emaxs.x + p->normal.y * emins.y + p->normal.z * emins.z;
		break;
	case 2:
		dist1 = p->normal.x * emaxs.x + p->normal.y * emins.y + p->normal.z * emaxs.z;
		dist2 = p->normal.x * emins.x + p->normal.y * emaxs.y + p->normal.z * emins.z;
		break;
	case 3:
		dist1 = p->normal.x * emins.x + p->normal.y * emins.y + p->normal.z * emaxs.z;
		dist2 = p->normal.x * emaxs.x + p->normal.y * emaxs.y + p->normal.z * emins.z;
		break;
	case 4:
		dist1 = p->normal.x * emaxs.x + p->normal.y * emaxs.y + p->normal.z * emins.z;
		dist2 = p->normal.x * emins.x + p->normal.y * emins.y + p->normal.z * emaxs.z;
		break;
	case 5:
		dist1 = p->normal.x * emins.x + p->normal.y * emaxs.y + p->normal.z * emins.z;
		dist2 = p->normal.x * emaxs.x + p->normal.y * emins.y + p->normal.z * emaxs.z;
		break;
	case 6:
		dist1 = p->normal.x * emaxs.x + p->normal.y * emins.y + p->normal.z * emins.z;
		dist2 = p->normal.x * emins.x + p->normal.y * emaxs.y + p->normal.z * emaxs.z;
		break;
	case 7:
		dist1 = p->normal.x * emins.x + p->normal.y * emins.y + p->normal.z * emins.z;
		dist2 = p->normal.x * emaxs.x + p->normal.y * emaxs.y + p->normal.z * emaxs.z;
		break;
	default:
		// shut up compiler
		dist1 = dist2 = 0;
		break;
	}

	if( dist1 >= p->dist )
		sides = 1;
	if( dist2 < p->dist )
		sides |= 2;

	return sides;
}

/*
=====================
PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
bool PlaneFromPoints( const Vector triangle[3], mplane_t *plane )
{
	Vector v1 = triangle[1] - triangle[0];
	Vector v2 = triangle[2] - triangle[0];
	plane->normal = CrossProduct( v2, v1 );

	if( plane->normal.Length() == 0.0f )
	{
		plane->normal = g_vecZero;
		return false;
	}

	plane->normal = plane->normal.Normalize();

	plane->dist = DotProduct( triangle[0], plane->normal );
	return true;
}

/*
=================
CategorizePlane

A slightly more complex version of SignbitsForPlane and PlaneTypeForNormal,
which also tries to fix possible floating point glitches (like -0.00000 cases)
=================
*/
void CategorizePlane( mplane_t *plane )
{
	plane->signbits = 0;
	plane->type = PLANE_NONAXIAL;

	for( int i = 0; i < 3; i++ )
	{
		if( plane->normal[i] < 0 )
		{
			plane->signbits |= (1<<i);

			if( plane->normal[i] == -1.0f )
			{
				plane->signbits = (1<<i);
				plane->normal = g_vecZero;
				plane->normal[i] = -1.0f;
				break;
			}
		}
		else if( plane->normal[i] == 1.0f )
		{
			plane->type = i;
			plane->signbits = 0;
			plane->normal = g_vecZero;
			plane->normal[i] = 1.0f;
			break;
		}
	}
}

/*
=================
ComparePlanes
=================
*/
bool ComparePlanes( mplane_t *plane, const Vector &normal, float dist )
{
	if( fabs( plane->normal.x - normal.x ) < PLANE_NORMAL_EPSILON
	 && fabs( plane->normal.y - normal.y ) < PLANE_NORMAL_EPSILON
	 && fabs( plane->normal.z - normal.z ) < PLANE_NORMAL_EPSILON
	 && fabs( plane->dist - dist ) < PLANE_DIST_EPSILON )
		return true;
	return false;
}

/*
==================
SnapVectorToGrid

==================
*/
void SnapVectorToGrid( Vector &normal )
{
	for( int i = 0; i < 3; i++ )
	{
		if( fabs( normal[i] - 1.0f ) < PLANE_NORMAL_EPSILON )
		{
			normal = g_vecZero;
			normal[i] = 1.0f;
			break;
		}

		if( fabs( normal[i] - -1.0f ) < PLANE_NORMAL_EPSILON )
		{
			normal = g_vecZero;
			normal[i] = -1.0f;
			break;
		}
	}
}

/*
==============
SnapPlaneToGrid

==============
*/
void SnapPlaneToGrid( mplane_t *plane )
{
	SnapVectorToGrid( plane->normal );

	if( fabs( plane->dist - Q_rint( plane->dist )) < PLANE_DIST_EPSILON )
		plane->dist = Q_rint( plane->dist );
}

/*
==============
VectorCompareEpsilon

==============
*/
bool VectorCompareEpsilon( const Vector &vec1, const Vector &vec2, float epsilon )
{
	float	ax, ay, az;

	ax = fabs( vec1.x - vec2.x );
	ay = fabs( vec1.y - vec2.y );
	az = fabs( vec1.z - vec2.z );

	if(( ax < epsilon ) && ( ay < epsilon ) && ( az < epsilon ))
		return true;
	return false;
}

bool RadianCompareEpsilon( const Radian &vec1, const Radian &vec2, float epsilon )
{
	for( int i = 0; i < 3; i++ )
	{
		// clamp to 2pi
		float a1 = fmod( vec1[i], (float)(M_PI * 2));
		float a2 = fmod( vec2[i], (float)(M_PI * 2));
		float delta =  fabs( a1 - a2 );
		
		// use the smaller angle (359 == 1 degree off)
		if( delta > M_PI )
		{
			delta = 2 * M_PI - delta;
		}

		if( delta > epsilon )
			return 0;
	}
	return 1;
}

// rotate a vector around the Z axis (YAW)
Vector VectorYawRotate( const Vector &in, float flYaw )
{
	Vector	out;
	float	sy, cy;

	SinCos( DEG2RAD(flYaw), &sy, &cy );

	out.x = in.x * cy - in.y * sy;
	out.y = in.x * sy + in.y * cy;
	out.z = in.z;

	return out;
}

bool VectorIsOnAxis(const Vector &v)
{
	int	count = 0;

	for (int i = 0; i < 3; i++)
	{
		if (fabs(v[i]) < 0.0000001f)
			count++;
	}

	// the zero vector will be on axis.
	return (count > 1) ? true : false;
}

// solve a x^2 + b x + c = 0
bool SolveQuadratic( float a, float b, float c, float &root1, float &root2 )
{
	if( a == 0 )
	{
		if( b != 0 )
		{
			// no x^2 component, it's a linear system
			root1 = root2 = -c / b;
			return true;
		}

		if( c == 0 )
		{
			// all zero's
			root1 = root2 = 0;
			return true;
		}
		return false;
	}

	float tmp = b * b - 4.0f * a * c;

	if( tmp < 0 )
	{
		// imaginary number, bah, no solution.
		return false;
	}

	tmp = sqrt( tmp );
	root1 = (-b + tmp) / (2.0f * a);
	root2 = (-b - tmp) / (2.0f * a);

	return true;
}

// solves for "a, b, c" where "a x^2 + b x + c = y", return true if solution exists
bool SolveInverseQuadratic( float x1, float y1, float x2, float y2, float x3, float y3, float &a, float &b, float &c )
{
	float det = (x1 - x2) * (x1 - x3) * (x2 - x3);

	// FIXME: check with some sort of epsilon
	if( det == 0.0 ) return false;

	a = (x3*(-y1 + y2) + x2*(y1 - y3) + x1*(-y2 + y3)) / det;

	b = (x3*x3*(y1 - y2) + x1*x1*(y2 - y3) + x2*x2*(-y1 + y3)) / det;

	c = (x1*x3*(-x1 + x3)*y2 + x2*x2*(x3*y1 - x1*y3) + x2*(-(x3*x3*y1) + x1*x1*y3)) / det;

	return true;
}

void CalcTBN(const Vector &p0, const Vector &p1, const Vector &p2, const Vector2D &t0, const Vector2D &t1, const Vector2D &t2, Vector &s, Vector &t, bool areaweight)
{
	Vector deltaPos1 = p1 - p0;
	Vector deltaPos2 = p2 - p0;
	Vector2D deltaUV1 = t1 - t0;
	Vector2D deltaUV2 = t2 - t0;

	float r = 1.0f;
	if ((deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x) < 0.0f)
		r = -1.0f;
	
	s = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
	t = (deltaPos1 * deltaUV2.x - deltaPos2 * deltaUV1.x) * r;	//binormal is inverted compared to the opengl one, probably due to quake left-handed coord system

	if (!areaweight)
	{
		// Normalize s and t
		s = s.Normalize();
		t = t.Normalize();
	}
}

float ColorNormalize( const Vector &in, Vector &out )
{
	float	max, scale;

	max = in.x;
	if( in.y > max )
		max = in.y;
	if( in.z > max )
		max = in.z;

	if( max == 0.0f )
		return 0.0f;

	scale = 1.0f / max;
	out = in * scale;

	return max;
}

/*
====================
ColorToNormal

pack XYZ into single float
====================
*/
float NormalToFloat(const Vector &normal)
{
	int	inormal[3];
	Vector	n = normal.Normalize();

	inormal[0] = n.x * 127 + 128;
	inormal[1] = n.y * 127 + 128;
	inormal[2] = n.z * 127 + 128;

	int pack = (inormal[0] << 16) | (inormal[1] << 8) | inormal[2];
	return (float)((double)pack / (double)(1 << 24));
}

unsigned short FloatToHalf( float v )
{
	unsigned int	i = *((unsigned int *)&v);
	unsigned int	e = (i >> 23) & 0x00ff;
	unsigned int	m = i & 0x007fffff;
	unsigned short	h;

	if( e <= 127 - 15 )
		h = ((m | 0x00800000) >> (127 - 14 - e)) >> 13;
	else h = (i >> 13) & 0x3fff;

	h |= (i >> 16) & 0xc000;

	return h;
}

float HalfToFloat( unsigned short h )
{
	unsigned int	f = (h << 16) & 0x80000000;
	unsigned int	em = h & 0x7fff;

	if( em > 0x03ff )
	{
		f |= (em << 13) + ((127 - 15) << 23);
	}
	else
	{
		unsigned int m = em & 0x03ff;

		if( m != 0 )
		{
			unsigned int e = (em >> 10) & 0x1f;

			while(( m & 0x0400 ) == 0 )
			{
				m <<= 1;
				e--;
			}

			m &= 0x3ff;
			f |= ((e + (127 - 14)) << 23) | (m << 13);
		}
	}

	return *((float *)&f);
}

signed char FloatToChar(float v)
{
	float in = v * 127.0f;
	float out = bound(-128.0f, in, 127.0f);
	return (char)out;
}

/*
====================
V_CalcFov
====================
*/
float V_CalcFov(float &fov_x, float width, float height)
{
	float x, half_fov_y;

	if (fov_x < 1 || fov_x > 170)
		fov_x = 90;

	x = width / tan(DEG2RAD(fov_x) * 0.5f);
	half_fov_y = atan(height / x);

	return RAD2DEG(half_fov_y) * 2;
}

/*
====================
V_AdjustFov
====================
*/
void V_AdjustFov(float &fov_x, float &fov_y, float width, float height, bool lock_x)
{
	float x, y;

	if ((width * 3) == (4 * height) || (width * 4) == (height * 5))
	{
		// 4:3 or 5:4 ratio
		return;
	}

	if (lock_x)
	{
		fov_y = 2 * atan((width * 3) / (height * 4) * tan(fov_y * M_PI / 360.0 * 0.5)) * 360 / M_PI;
		return;
	}

	y = V_CalcFov(fov_x, 640, 480);
	x = fov_x;

	fov_x = V_CalcFov(y, height, width);

	if (fov_x < x)
		fov_x = x;
	else fov_y = y;
}

/*
==================
UTIL_MoveBounds
==================
*/
void UTIL_MoveBounds(const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, Vector &outmins, Vector &outmaxs)
{
	for (int i = 0; i < 3; i++)
	{
		if (end[i] > start[i])
		{
			outmins[i] = start[i] + mins[i] - 1;
			outmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			outmins[i] = end[i] + mins[i] - 1;
			outmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}
