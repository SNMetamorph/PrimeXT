//=======================================================================
//			Copyright (C) Shambler Team 2005
//		          mathlib.h - shared mathlib header
//=======================================================================
#ifndef MATHLIB_H
#define MATHLIB_H

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

#ifndef M_PI2
#define M_PI2		(float)6.28318530717958647692
#endif

#ifndef byte
typedef unsigned char byte;
#endif // byte

#ifndef vec_t
typedef float vec_t;
#endif

#include <math.h>
#include <vector.h>
#include "matrix.h"

// NOTE: PhysX mathlib is conflicted with standard min\max
#define Q_min( a, b )		(((a) < (b)) ? (a) : (b))
#define Q_max( a, b )		(((a) > (b)) ? (a) : (b))
#define Q_recip( a )		((float)(1.0f / (float)(a)))
#define Q_floor( a )		((float)(long)(a))
#define Q_ceil( a )			((float)(long)((a) + 1))
#define Q_round( x, y )		(floor( x / y + 0.5 ) * y )
#define Q_square( a )		((a) * (a))

#define bound( min, num, max )	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define saturate( val )		((val) >= 0 ? ((val) < 1 ? (val) : 1) : 0)
#define IS_NAN(x)			(((*(int *)&x) & (255<<23)) == (255<<23))

// some Physic Engine declarations
#define METERS_PER_INCH		0.0254f
#define METER2INCH( x )		(float)( x * ( 1.0f / METERS_PER_INCH ))
#define INCH2METER( x )		(float)( x * ( METERS_PER_INCH / 1.0f ))

// Keeps clutter down a bit, when using a float as a bit-vector
#define SetBits( iBitVector, bits )	((iBitVector) = (iBitVector) | (bits))
#define ClearBits( iBitVector, bits )	((iBitVector) = (iBitVector) & ~(bits))
#define FBitSet( iBitVector, bit )	((iBitVector) & (bit))

#define CHARSIGNBITSET( c )		(((const uint)(c)) >> 7)
#define CHARSIGNBITNOTSET( c )	((~((const uint)(c))) >> 7)

#define SHORTSIGNBITSET( s )		(((const uint)(s)) >> 15)
#define SHORTSIGNBITNOTSET( s )	((~((const uint)(s))) >> 15)

#define INTSIGNBITSET( i )		(((const uint)(i)) >> 31)
#define INTSIGNBITNOTSET( i )		((~((const uint)(i))) >> 31)

#define FLOATSIGNBITSET( f )		((*(const uint *)&(f)) >> 31)
#define FLOATSIGNBITNOTSET( f )	((~(*(const uint *)&(f))) >> 31)

#define KNEEMAX_EPSILON		0.9998f	// (0.9998 is about 1 degree)

// Used to represent sides of things like planes.
#define SIDE_FRONT			0
#define SIDE_BACK			1
#define SIDE_ON			2

#define PLANE_X			0	// 0 - 2 are axial planes
#define PLANE_Y			1	// 3 needs alternate calc
#define PLANE_Z			2
#define PLANE_NONAXIAL		3

#define PLANE_NORMAL_EPSILON		0.0001f
#define PLANE_DIST_EPSILON		0.04f

#define SMALL_FLOAT			1e-12

inline float Q_fabs( float f ) { int tmp = *( int *)&f; tmp &= 0x7FFFFFFF; return *( float *)&tmp; }
inline float anglemod( float a ) { return (360.0f / 65536) * ((int)(a * (65536 / 360.0f)) & 65535); }
void TransformAABB( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &absmin, Vector &absmax );
void TransformAABBLocal( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &outmins, Vector &outmaxs );
void CalcClosestPointOnAABB( const Vector &mins, const Vector &maxs, const Vector &point, Vector &closestOut );
bool PlanesGetIntersectionPoint( const struct mplane_s *plane1, const struct mplane_s *plane2, const struct mplane_s *plane3, Vector &out );
Vector PlaneIntersect( struct mplane_s *plane, const Vector& p0, const Vector& p1 );
void VectorAngles( const Vector &forward, Vector &angles );

// Remap a value in the range [A,B] to [C,D].
inline float RemapVal( float val, float A, float B, float C, float D)
{
	if( A == B ) return val >= B ? D : C;
	return C + (D - C) * (val - A) / (B - A);
}

inline float RemapValClamped( float val, float A, float B, float C, float D)
{
	if( A == B ) return val >= B ? D : C;
	float cVal = (val - A) / (B - A);
	cVal = bound( 0.0f, cVal, 1.0f );

	return C + (D - C) * cVal;
}

// Returns A + (B-A)*flPercent.
// float Lerp( float flPercent, float A, float B );
template <class T>
_forceinline T Lerp( float flPercent, T const &A, T const &B )
{
	return A + (B - A) * flPercent;
}

int PlaneTypeForNormal( const Vector &normal );
int SignbitsForPlane( const Vector &normal );
int NearestPOW( int value, bool roundDown );
void SetPlane( struct mplane_s *plane, const Vector &vecNormal, float flDist );

//
// bounds operations
//
void ClearBounds( Vector &mins, Vector &maxs );
void ClearBounds( Vector2D &mins, Vector2D &maxs );
bool BoundsIsCleared( const Vector &mins, const Vector &maxs );
void ExpandBounds( Vector &mins, Vector &maxs, float offset );
void AddPointToBounds( const Vector &v, Vector &mins, Vector &maxs, float limit = 0.0f );
void AddPointToBounds( const Vector2D &v, Vector2D &mins, Vector2D &maxs );
bool BoundsIntersect( const Vector &mins1, const Vector &maxs1, const Vector &mins2, const Vector &maxs2 );
bool BoundsIntersect( const Vector2D &mins1, const Vector2D &maxs1, const Vector2D &mins2, const Vector2D &maxs2 );
bool BoundsAndSphereIntersect( const Vector &mins, const Vector &maxs, const Vector &origin, float radius );
bool BoundsAndSphereIntersect( const Vector2D &mins, const Vector2D &maxs, const Vector2D &origin, float radius );
float RadiusFromBounds( const Vector &mins, const Vector &maxs );

//
// quaternion operations
//
void AngleQuaternion( const Vector &angles, Vector4D &quat );
void AngleQuaternion( const Radian &angles, Vector4D &quat );
void AxisAngleQuaternion( const Vector &axis, float angle, Vector4D &q );
void QuaternionAngle( const Vector4D &quat, Vector &angles );
void QuaternionAngle( const Vector4D &quat, Radian &angles );
void QuaternionAlign( const Vector4D &p, const Vector4D &q, Vector4D &qt );
void QuaternionSlerp( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt );
void QuaternionSlerp( const Radian &r0, const Radian &r1, float t, Radian &r2 );
void QuaternionBlend( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt );
void QuaternionSlerpNoAlign( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt );
void QuaternionBlendNoAlign( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt );
void QuaternionMultiply( const Vector4D &q1, const Vector4D &q2, Vector4D &out );
void QuaternionVectorTransform( const Vector4D &q, const Vector &v, Vector &out );
void QuaternionConcatTransforms( const Vector4D &q1, const Vector &v1, const Vector4D &q2, const Vector &v2, Vector4D &q, Vector &v );
void QuaternionSM( float s, const Vector4D &p, const Vector4D &q, Vector4D &qt );
void QuaternionMA( const Vector4D &p, float s, const Vector4D &q, Vector4D &qt );
void QuaternionAccumulate( const Vector4D &p, float s, const Vector4D &q, Vector4D &qt );
void QuaternionMult( const Vector4D &p, const Vector4D &q, Vector4D &qt );
void QuaternionAdd( const Vector4D &p, const Vector4D &q, Vector4D &qt );
float QuaternionAngleDiff( const Vector4D &p, const Vector4D &q );
void QuaternionScale( const Vector4D &p, float t, Vector4D &q );
void QuaternionConjugate( const Vector4D &p, Vector4D &q );

//
// lerping stuff
//
void InterpolateOrigin( const Vector& start, const Vector& end, Vector& output, float frac, bool back = false );
void InterpolateAngles( const Vector& start, const Vector& end, Vector& output, float frac, bool back = false );
void NormalizeAngles( Vector &angles );

struct mplane_s;

void VectorMatrix( Vector &forward, Vector &right, Vector &up );
float ColorNormalize( const Vector &in, Vector &out );
// solve for "x" where "a x^2 + b x + c = 0", return true if solution exists
bool SolveQuadratic( float a, float b, float c, float &root1, float &root2 );
// solves for "a, b, c" where "a x^2 + b x + c = y", return true if solution exists
bool SolveInverseQuadratic( float x1, float y1, float x2, float y2, float x3, float y3, float &a, float &b, float &c );
bool PlaneFromPoints( const Vector triangle[3], struct mplane_s *plane );
bool ComparePlanes( struct mplane_s *plane, const Vector &normal, float dist );
bool VectorCompareEpsilon( const Vector &vec1, const Vector &vec2, float epsilon );
bool RadianCompareEpsilon( const Radian &vec1, const Radian &vec2, float epsilon );
void CategorizePlane( struct mplane_s *plane );
void SnapPlaneToGrid( struct mplane_s *plane );
void SnapVectorToGrid( Vector &normal );


int BoxOnPlaneSide( const Vector &emins, const Vector &emaxs, const struct mplane_s *plane );
#define BOX_ON_PLANE_SIDE(emins, emaxs, p) (((p)->type < 3)?(((p)->dist <= (emins)[(p)->type])? 1 : (((p)->dist >= (emaxs)[(p)->type])? 2 : 3)):BoxOnPlaneSide( (emins), (emaxs), (p)))

#define PlaneDist(point,plane) ((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal))
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)
#define VectorNormalizeFast( v ) { float ilength = (float)rsqrt( DotProduct( v,v )); v[0] *= ilength; v[1] *= ilength; v[2] *= ilength; }
#define VectorDistance(a, b) (sqrt( VectorDistance2( a, b )))
#define VectorDistance2(a, b) (((a)[0] - (b)[0]) * ((a)[0] - (b)[0]) + ((a)[1] - (b)[1]) * ((a)[1] - (b)[1]) + ((a)[2] - (b)[2]) * ((a)[2] - (b)[2]))
Vector VectorYawRotate( const Vector &in, float flYaw );

// FIXME: get rid of this
#define VectorCopy(a,b)	((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define DotProductA( x,y )	((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])

#define TriangleNormal(a,b,c,n) \
( \
	(n)[0] = ((a)[1] - (b)[1]) * ((c)[2] - (b)[2]) - ((a)[2] - (b)[2]) * ((c)[1] - (b)[1]), \
	(n)[1] = ((a)[2] - (b)[2]) * ((c)[0] - (b)[0]) - ((a)[0] - (b)[0]) * ((c)[2] - (b)[2]), \
	(n)[2] = ((a)[0] - (b)[0]) * ((c)[1] - (b)[1]) - ((a)[1] - (b)[1]) * ((c)[0] - (b)[0])  \
)

/*
=================
rsqrt
=================
*/
inline float rsqrt( float number )
{
	if( number == 0.0f )
		return 0.0f;

	int x = number * 0.5f;
	int i = *(int *)&number;	// evil floating point bit level hacking
	i = 0x5f3759df - (i >> 1);	// what the fuck?
	int y = *(float *)&i;
	y = y * (1.5f - (x * y * y));	// first iteration

	return y;
}

// hermite basis function for smooth interpolation
// Similar to Gain() above, but very cheap to call
// value should be between 0 & 1 inclusive
inline float SimpleSpline( float value )
{
	float valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return (3 * valueSquared - 2 * valueSquared * value);
}

inline float SimpleSplineRemapVal( float val, float A, float B, float C, float D)
{
	if ( A == B )
		return val >= B ? D : C;
	float cVal = (val - A) / (B - A);
	return C + (D - C) * SimpleSpline( cVal );
}

unsigned short FloatToHalf( float v );
float HalfToFloat( unsigned short h );

#endif//MATHLIB_H
