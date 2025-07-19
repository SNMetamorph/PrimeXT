/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#ifndef __MATHLIB__
#define __MATHLIB__

// mathlib.h

#include <math.h>
#include <float.h>
#include "port.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DOUBLEVEC_T
typedef double	vec_t;
#else
typedef float	vec_t;
#endif

struct half
{
	unsigned short sh;

	half( ){};
	half( const float x );
	operator float () const;
};

typedef vec_t	vec2_t[2];	// x,y
typedef vec_t	vec3_t[3];	// x,y,z
typedef vec_t	vec4_t[4];	// x,y,z,w
typedef vec_t	vec5_t[5];	// rare case
typedef vec_t	matrix3x4[4][3];	// rad matrix
typedef vec_t	matrix4x4[4][4];	// rad matrix

typedef byte	bvec3_t[3];	// x,y,z
typedef short	svec3_t[3];
typedef half	hvec3_t[3];

// euler angle order
#define PITCH		0
#define YAW			1
#define ROLL		2

#ifndef M_PI
#define M_PI		(vec_t)3.14159265358979323846
#endif

#ifndef M_PI2
#define M_PI2		(vec_t)6.28318530717958647692
#endif

#define M_PI_F		((vec_t)(M_PI))
#define M_PI2_F		((vec_t)(M_PI2))

#define RAD2DEG( x )	((vec_t)(x) * (vec_t)(180 / M_PI))
#define DEG2RAD( x )	((vec_t)(x) * (vec_t)(M_PI / 180))

#define MINSPLIT_EPSILON	0.002
#define MAXSPLIT_EPSILON	0.2

#define SMALL_FLOAT		1e-12

#define SIDE_FRONT		0
#define SIDE_BACK		1
#define SIDE_ON			2
#define SIDE_CROSS		-2

#define PLANE_X		0	// 0 - 2 are axial planes
#define PLANE_Y		1	// 3 needs alternate calc
#define PLANE_Z		2
#define PLANE_ANY_X		3
#define PLANE_ANY_Y		4
#define PLANE_ANY_Z		5
#define PLANE_LAST_AXIAL	PLANE_Z
#define PLANE_NONAXIAL	3

#define Q_min( a, b )	(((a) < (b)) ? (a) : (b))
#define Q_max( a, b )	(((a) > (b)) ? (a) : (b))
#define Q_recip( a )	((vec_t)(1.0 / (vec_t)(a)))
#define Q_floor( a )	((vec_t)(intptr_t)(a))
#define Q_ceil( a )		((vec_t)(intptr_t)((a) + 1))
#define Q_round( a )	(floor(( a ) + 0.5))
#define Q_rounddn( x, y )	(floor( x / y + 0.5 ) * y )
#define Q_roundup( x, y )	(ceil( x / y ) * y )

#define Q_rint(x)		((x) < 0 ? ((intptr_t)((x) - 0.5)) : ((intptr_t)((x) + 0.5)))
#define IS_NAN(x)		(((*(int *)&x) & (255<<23)) == (255<<23))

#define AxisFromNormal( n )	( fabs( n[0] ) < NORMAL_EPSILON ) + ( fabs( n[1] ) < NORMAL_EPSILON ) + ( fabs( n[2] ) < NORMAL_EPSILON )
#define ARRAYSIZE( p )	(sizeof( p ) / sizeof( p[0] ))

//
// Vector operations
//
#define Vector2Set(v, x, y) ((v)[0]=(x),(v)[1]=(y))
#define Vector2Scale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale))
#define Vector2Copy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1])
#define VectorIsNAN(v) (IS_NAN(v[0]) || IS_NAN(v[1]) || IS_NAN(v[2]))	
#define DotProduct(x,y) ((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define DotProductAbs(x,y) (abs((x)[0]*(y)[0])+abs((x)[1]*(y)[1])+abs((x)[2]*(y)[2]))
#define DotProductFabs(x,y) (fabs((x)[0]*(y)[0])+fabs((x)[1]*(y)[1])+fabs((x)[2]*(y)[2]))
#define CrossProduct(a,b,c) ((c)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1],(c)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2],(c)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0])
#define Vector2Cross(a,b) ((a)[0]*(b)[1]-(a)[1]*(b)[0])
#define VectorSubtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c) ((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorAddScalar(a,b,c) ((c)[0]=(a)[0]+(b),(c)[1]=(a)[1]+(b),(c)[2]=(a)[2]+(b))
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorScale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
#define VectorCompare(v1,v2)	((v1)[0]==(v2)[0] && (v1)[1]==(v2)[1] && (v1)[2]==(v2)[2])
#define VectorCompareMin(a,b,c) ((c)[0] = Q_min((a)[0], (b)[0]), (c)[1] = Q_min((a)[1], (b)[1]), (c)[2] = Q_min((a)[2], (b)[2]))
#define VectorCompareMax(a,b,c) ((c)[0] = Q_max((a)[0], (b)[0]), (c)[1] = Q_max((a)[1], (b)[1]), (c)[2] = Q_max((a)[2], (b)[2]))
#define VectorMultiply(a,b,c) ((c)[0]=(a)[0]*(b)[0],(c)[1]=(a)[1]*(b)[1],(c)[2]=(a)[2]*(b)[2])
#define VectorDivide( in, d, out ) VectorScale( in, (1.0 / (d)), out )
#define VectorFill( a, b ) ((a)[0] = (b), (a)[1] = (b), (a)[2] = (b))
#define VectorMaximum(a) (Q_max((a)[0], Q_max((a)[1], (a)[2] )))
#define VectorMinimum(a) (Q_min((a)[0], Q_min((a)[1], (a)[2] )))
#define VectorMax(a) ( Q_max((a)[0], Q_max((a)[1], (a)[2])) )
#define VectorAvg(a) ( ((a)[0] + (a)[1] + (a)[2]) / 3 )
#define VectorLength(a) ( sqrt( DotProduct( a, a )))
#define VectorLength2(a) (DotProduct( a, a ))
#define VectorDistance(a, b) (sqrt( VectorDistance2( a, b )))
#define VectorDistance2(a, b) (((a)[0] - (b)[0]) * ((a)[0] - (b)[0]) + ((a)[1] - (b)[1]) * ((a)[1] - (b)[1]) + ((a)[2] - (b)[2]) * ((a)[2] - (b)[2]))
#define VectorAverage(a,b,o)	((o)[0]=((a)[0]+(b)[0])*0.5,(o)[1]=((a)[1]+(b)[1])*0.5,(o)[2]=((a)[2]+(b)[2])*0.5)
#define VectorSet(v, x, y, z) ((v)[0]=(x),(v)[1]=(y),(v)[2]=(z))
#define VectorClear(x) ((x)[0]=(x)[1]=(x)[2]=0)
#define VectorNormalize2( v ) { vec_t ilength = sqrt( DotProduct( v, v )); if( ilength ) ilength = 1.0 / ilength; VectorScale( v, ilength, v ); }
#define VectorLerp( v1, lerp, v2, c ) ((c)[0] = (v1)[0] + (lerp) * ((v2)[0] - (v1)[0]), (c)[1] = (v1)[1] + (lerp) * ((v2)[1] - (v1)[1]), (c)[2] = (v1)[2] + (lerp) * ((v2)[2] - (v1)[2]))
#define VectorNegate(x, y) ((y)[0] = -(x)[0], (y)[1] = -(x)[1], (y)[2] = -(x)[2])
#define VectorRecip(x, y) ((y)[0] = 1.0 / (x)[0], (y)[1] = 1.0 / (x)[1], (y)[2] = 1.0 / (x)[2])
#define VectorMA(a, scale, b, c) ((c)[0] = (a)[0] + (scale) * (b)[0],(c)[1] = (a)[1] + (scale) * (b)[1],(c)[2] = (a)[2] + (scale) * (b)[2])
#define VectorMAM( scale1, b1, scale2, b2, c ) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2])
#define VectorIsNull( v ) ((v)[0] == 0.0 && (v)[1] == 0.0 && (v)[2] == 0.0)

#if XASH_WIN32
#define VectorIsFinite( v ) ( _finite( (v)[0] ) && _finite( (v)[1] ) && _finite((v)[2] ))
#else
#define VectorIsFinite( v ) ( finite( (v)[0] ) && finite( (v)[1] ) && finite((v)[2] ))
#endif

#define PlaneDist(point,plane) ((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal))
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)
#define PlaneDist2(point,plane) (DotProduct((point), (plane)->normal))
#define PlaneDiff2(point,plane) ((DotProduct((point), (plane)->normal)) - (plane)->dist)
#define bound( min, num, max ) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define Vector4Set(v, a, b, c, d) ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c),(v)[3] = (d))
#define Vector4Copy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])
#define VectorSnap( a, b, c ) (c[0] = Q_rounddn( a[0], b ), c[1] = Q_rounddn( a[1], b ), c[2] = Q_rounddn( a[2], b ))

// return the smallest power of two >= x.
// returns 0 if x == 0 or x > 0x80000000 (ie numbers that would be negative if x was signed)
// NOTE: the old code took an int, and if you pass in an int of 0x80000000 casted to a uint,
//       you'll get 0x80000000, which is correct for uints, instead of 0, which was correct for ints
_forceinline uint SmallestPowerOfTwoGreaterOrEqual( uint x )
{
	x -= 1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

// Remap a value in the range [A,B] to [C,D].
inline float RemapVal( float val, float A, float B, float C, float D)
{
	if ( A == B )
		return val >= B ? D : C;
	return C + (D - C) * (val - A) / (B - A);
}

inline float RemapValClamped( float val, float A, float B, float C, float D)
{
	if ( A == B )
		return val >= B ? D : C;
	float cVal = (val - A) / (B - A);
	cVal = bound( 0.0f, cVal, 1.0f );

	return C + (D - C) * cVal;
}

inline vec_t VectorNormalize( vec3_t v )
{
	double	length;

	length = DotProduct( v, v );
	length = sqrt( length );

	if( length == 0.0 )
	{
		VectorClear( v );
		return 0.0;
	}

	v[0] /= length;
	v[1] /= length;
	v[2] /= length;

	return length;
}

#if XASH_WIN32
// disabled declaration for linux to avoid conflict
_forceinline int ffsl( uint32_t mask )
{
	if (mask == 0) 
		return 0;

	int	bit;
	for (bit = 1; !(mask & 1); bit++)
		mask = mask >> 1;

	return bit;
}
#endif

/*
==============
fix_coord

converts the reletive tex coords to absolute
==============
*/
_forceinline uint fix_coord( vec_t in, uint width )
{
	if( in > 0 ) return (uint)in % width;
	return width - ((uint)fabs( in ) % width);
}

bool VectorIsOnAxis( const vec3_t v );
bool VectorCompareEpsilon( const vec3_t vec1, const vec3_t vec2, vec_t epsilon );
bool VectorCompareEpsilon2( const vec3_t vec1, const float vec2[3], vec_t epsilon );
void CalcST( const vec3_t p0, const vec3_t p1, const vec3_t p2, const vec2_t t0, const vec2_t t1, const vec2_t t2, vec3_t s, vec3_t t, bool areaweight = false );
void VectorVectors( const vec3_t forward, vec3_t right, vec3_t up );
vec_t VectorNormalizeLength2( const vec3_t v, vec3_t out );
void SinCos( float radians, float *sine, float *cosine );
uint VertexHashKey( const vec3_t point, uint hashSize );
int PlaneTypeForNormal( const vec3_t normal );
int SignbitsForPlane( const vec3_t normal );
float RandomFloat( float flLow, float flHigh );
float ColorNormalize( const vec3_t in, vec3_t out );
unsigned short FloatToHalf( float v );
float HalfToFloat( unsigned short h );

float AcosFast( float x );
float Atan2Fast( float y, float x );
void TriangleIncenter( const vec3_t a, const vec3_t b, const vec3_t c, vec3_t out );
void WorldToTangent( const vec3_t v, const vec3_t t, const vec3_t b, const vec3_t n, vec3_t out );
void TangentToWorld( const vec3_t v, const vec3_t t, const vec3_t b, const vec3_t n, vec3_t out );

//
// Bounding Box operations
//
void ClearBounds( vec3_t mins, vec3_t maxs );
bool BoundsIsCleared( const vec3_t mins, const vec3_t maxs );
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );
bool BoundsIntersect( const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2, vec_t epsilon = 0.0 );
bool BoundsAndSphereIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t origin, vec_t radius );
bool SphereIntersect( const vec3_t vSphereCenter, vec_t fSphereRadiusSquared, const vec3_t vLinePt, const vec3_t vLineDir );
vec_t RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void ExpandBounds( vec3_t mins, vec3_t maxs, vec_t offset );
void COM_NormalizeAngles( vec3_t angles );

//
// Quaternion & matrices
//
#define Matrix3x4_LoadIdentity( mat )		Matrix3x4_Copy( mat, matrix3x4_identity )
#define Matrix3x4_Copy( out, in )		memcpy( out, in, sizeof( matrix3x4 ))
#define Matrix4x4_LoadIdentity( mat )		Matrix3x4_Copy( mat, matrix4x4_identity )
#define Matrix4x4_Copy( out, in )		memcpy( out, in, sizeof( matrix4x4 ))

void AngleQuaternion( const vec3_t angles, vec4_t q );
void Matrix3x4_CreateFromEntityScale3f( matrix3x4 out, const vec3_t angles, const vec3_t origin, const vec3_t scale );
void Matrix3x4_FromOriginQuat( matrix3x4 out, const vec4_t quaternion, const vec3_t origin );
void Matrix3x4_MatrixToEntityScale3f( const matrix3x4 in, vec3_t origin, vec3_t angles, vec3_t scale );
void Matrix3x4_ConcatTransforms( matrix3x4 out, const matrix3x4 in1, const matrix3x4 in2 );
void Matrix3x4_TransformStandardPlane( const matrix3x4 in, const vec3_t normal, float d, vec3_t out, float *dist );
void Matrix3x4_VectorITransform( const matrix3x4 in, const vec3_t v, vec3_t out );
void Matrix3x4_VectorTransform( const matrix3x4 in, const vec3_t v, vec3_t out );
void Matrix3x4_Vector4Transform( const matrix3x4 in, const vec4_t v, vec4_t out );
void Matrix3x4_VectorIRotate( const matrix3x4 in, const vec3_t v, vec3_t out );
void Matrix3x4_VectorRotate( const matrix3x4 in, const vec3_t v, vec3_t out );
void QuaternionSlerp( const vec4_t p, vec4_t q, vec_t t, vec4_t qt );
bool Matrix3x4_Invert_Full( matrix3x4 out, const matrix3x4 in );
vec_t Matrix3x4_CalcSign( const matrix3x4 in );

extern vec3_t vec3_origin;
extern const matrix3x4	matrix3x4_identity;
extern const matrix4x4	matrix4x4_identity;

bool Matrix4x4_Invert_Full( matrix4x4 out, const matrix4x4 in1 );
void Matrix4x4_Concat( matrix4x4 out, const matrix4x4 in1, const matrix4x4 in2 );
#ifdef __cplusplus
}
#endif

#endif
