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

#include <math.h>
#include <vector.h>
#include "matrix.h"

// NOTE: PhysX mathlib is conflicted with standard min\max
#define Q_min( a, b )		(((a) < (b)) ? (a) : (b))
#define Q_max( a, b )		(((a) > (b)) ? (a) : (b))

#define bound( min, num, max )	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define IS_NAN(x)			(((*(int *)&x) & (255<<23)) == (255<<23))

// some Physic Engine declarations
#define METERS_PER_INCH		0.0254f
#define METER2INCH( x )		(float)( x * ( 1.0f / METERS_PER_INCH ))
#define INCH2METER( x )		(float)( x * ( METERS_PER_INCH / 1.0f ))

// Keeps clutter down a bit, when using a float as a bit-vector
#define SetBits( iBitVector, bits )	((iBitVector) = (iBitVector) | (bits))
#define ClearBits( iBitVector, bits )	((iBitVector) = (iBitVector) & ~(bits))
#define FBitSet( iBitVector, bit )	((iBitVector) & (bit))

// Used to represent sides of things like planes.
#define SIDE_FRONT			0
#define SIDE_BACK			1
#define SIDE_ON			2

#define PLANE_X			0	// 0 - 2 are axial planes
#define PLANE_Y			1	// 3 needs alternate calc
#define PLANE_Z			2
#define PLANE_NONAXIAL		3

#define PLANE_NORMAL_EPSILON		0.00001f
#define PLANE_DIST_EPSILON		0.01f

inline float anglemod( float a ) { return (360.0f / 65536) * ((int)(a * (65536 / 360.0f)) & 65535); }
void TransformAABB( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &absmin, Vector &absmax );
void TransformAABBLocal( const matrix4x4& world, const Vector &mins, const Vector &maxs, Vector &outmins, Vector &outmaxs );
void CalcClosestPointOnAABB( const Vector &mins, const Vector &maxs, const Vector &point, Vector &closestOut );
void RotatePointAroundVector( Vector &dst, const Vector &dir, const Vector &point, float degrees );
void VectorAngles( const Vector &forward, Vector &angles );
int SignbitsForPlane( const Vector &normal );
int NearestPOW( int value, bool roundDown );

//
// bounds operations
//
void ClearBounds( Vector &mins, Vector &maxs );
void AddPointToBounds( const Vector &v, Vector &mins, Vector &maxs );
bool BoundsIntersect( const Vector &mins1, const Vector &maxs1, const Vector &mins2, const Vector &maxs2 );
bool BoundsAndSphereIntersect( const Vector &mins, const Vector &maxs, const Vector &origin, float radius );
float RadiusFromBounds( const Vector &mins, const Vector &maxs );
void PerpendicularVector( Vector &dst, const Vector &src );

//
// quaternion operations
//
void AngleQuaternion( const Vector &angles, Vector4D &quat, bool degrees = false );
void QuaternionAngle( const Vector4D &quat, Vector &angles, bool degrees = false );
void QuaternionAlign( const Vector4D &p, const Vector4D &q, Vector4D &qt );
void QuaternionSlerp( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt );
void QuaternionSlerpNoAlign( const Vector4D &p, const Vector4D &q, float t, Vector4D &qt );

//
// lerping stuff
//
void InterpolateOrigin( const Vector& start, const Vector& end, Vector& output, float frac, bool back = false );
void InterpolateAngles( const Vector& start, const Vector& end, Vector& output, float frac, bool back = false );
void NormalizeAngles( Vector &angles );

struct mplane_s;

void VectorMatrix( Vector &forward, Vector &right, Vector &up );
float ColorNormalize( const Vector &in, Vector &out );
float rsqrt( float number );

bool PlaneFromPoints( const Vector triangle[3], struct mplane_s *plane );
bool ComparePlanes( struct mplane_s *plane, const Vector &normal, float dist );
void CategorizePlane( struct mplane_s *plane );
void SnapPlaneToGrid( struct mplane_s *plane );
void SnapVectorToGrid( Vector &normal );

int BoxOnPlaneSide( const Vector emins, const Vector emaxs, const struct mplane_s *plane );
#define BOX_ON_PLANE_SIDE(emins, emaxs, p) (((p)->type < 3)?(((p)->dist <= (emins)[(p)->type])? 1 : (((p)->dist >= (emaxs)[(p)->type])? 2 : 3)):BoxOnPlaneSide( (emins), (emaxs), (p)))

#define PlaneDist(point,plane) ((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal))
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

unsigned short FloatToHalf( float v );
float HalfToFloat( unsigned short h );

// fast math for non-class vestors
#define DotProductFast( x,y )	((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorMAFast( a, scale, b, c ) ((c)[0] = (a)[0] + (scale) * (b)[0],(c)[1] = (a)[1] + (scale) * (b)[1],(c)[2] = (a)[2] + (scale) * (b)[2])
#define CrossProductFast(a,b,c) ((c)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1],(c)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2],(c)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0])
#define VectorScaleFast(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
#define VectorSubtractFast(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAddFast(a,b,c) ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopyFast(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])

#define round( x, y )	(floor( x / y + 0.5 ) * y )
#define recip( x )		((float)( 1.0f / (x) ))
#define square( a )		((a) * (a))

// remap a value in the range [A,B] to [C,D].
#define RemapVal( val, A, B, C, D )	(C + (D - C) * (val - A) / (B - A))

#endif//MATHLIB_H
