/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// mathlib.c -- math primitives
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"

vec3_t vec3_origin = { 0, 0, 0 };

#define IA		16807
#define IM		2147483647
#define IQ		127773
#define IR		2836
#define NTAB		32
#define NDIV		(1+(IM-1)/NTAB)
#define MAX_RANDOM_RANGE	0x7FFFFFFFUL

// fran1 -- return a random floating-point number on the interval [0,1)
//
#define AM (1.0/IM)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

static int m_idum = 0;
static int m_iy = 0;
static int m_iv[NTAB];

static int GenerateRandomNumber( void )
{
	int	j, k;
	
	if( m_idum <= 0 || !m_iy )
	{
		if( -(m_idum) < 1 ) 
			m_idum = 1;
		else  m_idum = -(m_idum);

		for( j = NTAB + 7; j >= 0; j-- )
		{
			k = (m_idum) / IQ;
			m_idum = IA * (m_idum - k * IQ) - IR * k;

			if( m_idum < 0 ) 
				m_idum += IM;

			if( j < NTAB )
				m_iv[j] = m_idum;
		}

		m_iy = m_iv[0];
	}

	k = (m_idum) / IQ;
	m_idum = IA * (m_idum - k * IQ) - IR * k;

	if( m_idum < 0 ) 
		m_idum += IM;
	j = m_iy / NDIV;

	// We're seeing some strange memory corruption in the contents of s_pUniformStream. 
	// Perhaps it's being caused by something writing past the end of this array? 
	// Bounds-check in release to see if that's the case.
	if( j >= NTAB || j < 0 )
	{
		MsgDev( D_WARN, "GenerateRandomNumber had an array overrun: tried to write to element %d of 0..31.\n", j );
		j = ( j % NTAB ) & 0x7fffffff;
	}

	m_iy = m_iv[j];
	m_iv[j] = m_idum;

	return m_iy;
}

float RandomFloat( float flLow, float flHigh )
{
	// float in [0,1)
	float fl = AM * GenerateRandomNumber();
	if( fl > RNMX ) fl = RNMX;

	return (fl * ( flHigh - flLow ) ) + flLow; // float in [low,high)
}

const matrix3x4 matrix3x4_identity =
{
{ 1.0f, 0.0f, 0.0f },	// PITCH
{ 0.0f, 1.0f, 0.0f },	// YAW
{ 0.0f, 0.0f, 1.0f },	// ROLL
{ 0.0f, 0.0f, 0.0f },	// ORG
};

half :: half( const float x )
{
	union
	{
		float floatI;
		unsigned int i;
	};

	floatI = x;
	int e = ((i >> 23) & 0xFF) - 112;
	int m =  i & 0x007FFFFF;

	sh = (i >> 16) & 0x8000;
	if( e <= 0 )
	{
		// Denorm
		m = ((m | 0x00800000) >> (1 - e)) + 0x1000;
		sh |= (m >> 13);
	}
	else if( e == 143 )
	{
		sh |= 0x7C00;
		if (m != 0){
			// NAN
			m >>= 13;
			sh |= m | (m == 0);
		}
	}
	else
	{
		m += 0x1000;

		if( m & 0x00800000 )
		{
			// Mantissa overflow
			m = 0;
			e++;
		}

		if( e >= 31 )
		{
			// Exponent overflow
			sh |= 0x7C00;
		}
		else
		{
			sh |= (e << 10) | (m >> 13);
		}
	}
}

half :: operator float() const
{
	union
	{
		unsigned int s;
		float result;
	};

	s = (sh & 0x8000) << 16;
	unsigned int e = (sh >> 10) & 0x1F;
	unsigned int m = sh & 0x03FF;

	if( e == 0 )
	{
		// +/- 0
		if( m == 0 ) return result;

		// Denorm
		while(( m & 0x0400 ) == 0 )
		{
			m += m;
			e--;
		}
		e++;
		m &= ~0x0400;
	}
	else if( e == 31 )
	{
		// INF / NAN
		s |= 0x7F800000 | (m << 13);
		return result;
	}

	s |= ((e + 112) << 23) | (m << 13);

	return result;
}

/*
=================
SinCos
=================
*/
void SinCos( float radians, float *sine, float *cosine )
{
#if _GNU_SOURCE && !XASH_ANDROID
	sincosf(radians, sine, cosine);
#else
	*sine = sinf(radians);
	*cosine = cosf(radians);
#endif
}

/*
=================
AcosFast
=================
*/
float AcosFast( float x )
{
	float res = 0.04617586081f * fabsf(x) - 0.20275862548f;
    res = res * fabsf(x) + 1.57079632679f;
    res *= sqrtf(1.0f - fabsf(x)); 
    return (x >= 0.0) ? res : M_PI - res; 
}

/*
=================
AtanFast
=================
*/
float Atan2Fast( float y, float x )
{
	if( x == 0.0f )
		return (y >= 0.0f) ? 1.57079632679f : - 1.57079632679f;

	float ydivx = y / x;	
	
	float res = AcosFast( 1.0f / sqrtf( 1.0f + ydivx * ydivx ) );
	res = ( ydivx >= 0.0f ) ? res : - res;

	if( x < 0.0f )
		res += (y >= 0.0f) ? M_PI : - M_PI;

	return res;
}

/*
=================
TriangleArea
=================
*/
vec_t TriangleArea( const vec3_t a, const vec3_t b, const vec3_t c )
{
	vec3_t ba, ca, cro;
	VectorSubtract( b, a, ba );
	VectorSubtract( c, a, ca );
	CrossProduct( ba, ca, cro );
	return 0.5f * VectorLength( cro );
}

/*
=================
TriangleIncenter
=================
*/
void TriangleIncenter( const vec3_t a, const vec3_t b, const vec3_t c, vec3_t out )
{
	float	l, sum;

	l = VectorDistance( b, c );
	VectorScale( a, l, out );
	sum = l;

	l = VectorDistance( a, c );
	VectorMA( out, l, b, out );
	sum += l;

	l = VectorDistance( a, b );
	VectorMA( out, l, c, out );
	sum += l;

	if( sum > 0.0f )
		VectorScale( out, 1.0f / sum, out );
}

/*
=================
WorldToTangent
=================
*/
void WorldToTangent( const vec3_t v, const vec3_t t, const vec3_t b, const vec3_t n, vec3_t out )
{
	out[0] = v[0] * t[0] + v[1] * t[1] + v[2] * t[2];
	out[1] = v[0] * b[0] + v[1] * b[1] + v[2] * b[2];
	out[2] = v[0] * n[0] + v[1] * n[1] + v[2] * n[2];
}

/*
=================
TangentToWorld
=================
*/
void TangentToWorld( const vec3_t v, const vec3_t t, const vec3_t b, const vec3_t n, vec3_t out )
{
	out[0] = v[0] * t[0] + v[1] * b[0] + v[2] * n[0];
	out[1] = v[0] * t[1] + v[1] * b[1] + v[2] * n[1];
	out[2] = v[0] * t[2] + v[1] * b[2] + v[2] * n[2];
}

/*
=================
Halton
=================
*/
float Halton(int base, int index)
{
	float result = 0.0f;
	float f = 1.0f;
	while (index > 0)
	{
		f = f / float(base);
		result += f * float(index % base);
		index = index / base; 
	}
	return result;
}


/*
==============
ColorNormalize

==============
*/
float ColorNormalize( const vec3_t in, vec3_t out )
{
	float	scale;

	if(( scale = VectorMax( in )) == 0 )
		return 0;

	VectorScale( in, (1.0 / scale), out );

	return scale;
}

/*
==============
FloatToHalf

==============
*/
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

/*
==============
HalfToFloat

==============
*/
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

/*
==============
VectorCompareEpsilon

==============
*/
bool VectorCompareEpsilon( const vec3_t vec1, const vec3_t vec2, vec_t epsilon )
{
	vec_t	ax, ay, az;

	ax = fabs( vec1[0] - vec2[0] );
	ay = fabs( vec1[1] - vec2[1] );
	az = fabs( vec1[2] - vec2[2] );

	if(( ax <= epsilon ) && ( ay <= epsilon ) && ( az <= epsilon ))
		return true;
	return false;
}

bool VectorCompareEpsilon2( const vec3_t vec1, const float vec2[3], vec_t epsilon )
{
	vec_t	ax, ay, az;

	ax = fabs( vec1[0] - vec2[0] );
	ay = fabs( vec1[1] - vec2[1] );
	az = fabs( vec1[2] - vec2[2] );

	if(( ax <= epsilon ) && ( ay <= epsilon ) && ( az <= epsilon ))
		return true;
	return false;
}

/*
==================
VertexHashKey
==================
*/
uint VertexHashKey( const vec3_t point, uint hashSize )
{
	uint	hashKey = 0;

	hashKey ^= int( fabs( point[0] ));
	hashKey ^= int( fabs( point[1] ));
	hashKey ^= int( fabs( point[2] ));

	hashKey &= (hashSize - 1);

	return hashKey;
}

/*
================
VectorIsOnAxis
================
*/
bool VectorIsOnAxis( const vec3_t v )
{
	int	count = 0;

	for( int i = 0; i < 3; i++ )
	{
		if( v[i] == 0.0 )
			count++;
	}

	// the zero vector will be on axis.
	return (count > 1) ? true : false;
}

vec_t VectorNormalizeLength2( const vec3_t v, vec3_t out )
{
	vec_t	length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = sqrt( length );

	if( length )
	{
		ilength = 1.0 / length;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	}

	return length;
}

void VectorVectors( const vec3_t forward, vec3_t right, vec3_t up )
{
	VectorSet( right, forward[2], -forward[0], forward[1] );
	vec_t d = DotProduct( forward, right );
	VectorMA( right, -d, forward, right );
	VectorNormalize( right );
	CrossProduct( right, forward, up );
	VectorNormalize( up );
}

/*
=================
PlaneTypeForNormal
=================
*/
int PlaneTypeForNormal( const vec3_t normal )
{
	if( normal[0] == 1 )
		return PLANE_X;
	if( normal[1] == 1 )
		return PLANE_Y;
	if( normal[2] == 1 )
		return PLANE_Z;
	return PLANE_NONAXIAL;
}

/*
=================
SignbitsForPlane

fast box on planeside test
=================
*/
int SignbitsForPlane( const vec3_t normal )
{
	int bits = 0;
	for (int i = 0; i < 3; i++)
	{
		if (normal[i] < 0.0f)
			bits |= 1 << i;
	}
	return bits;
}

//
// bounds operations
//
/*
=================
ClearBounds
=================
*/
void ClearBounds( vec3_t mins, vec3_t maxs )
{
	// make bogus range
	mins[0] = mins[1] = mins[2] =  999999.0;
	maxs[0] = maxs[1] = maxs[2] = -999999.0;
}

/*
=================
AddPointToBounds
=================
*/
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs )
{
	vec_t	val;
	int	i;

	for( i = 0; i < 3; i++ )
	{
		val = v[i];
		if( val < mins[i] ) mins[i] = val;
		if( val > maxs[i] ) maxs[i] = val;
	}
}

/*
=================
ExpandBounds
=================
*/
void ExpandBounds( vec3_t mins, vec3_t maxs, vec_t offset )
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
RadiusFromBounds
=================
*/
vec_t RadiusFromBounds( const vec3_t mins, const vec3_t maxs )
{
	vec3_t	corner;
	int	i;

	for( i = 0; i < 3; i++ )
	{
		corner[i] = fabs( mins[i] ) > fabs( maxs[i] ) ? fabs( mins[i] ) : fabs( maxs[i] );
	}
	return VectorLength( corner );
}

/*
=================
BoundsIsCleared
=================
*/
bool BoundsIsCleared( const vec3_t mins, const vec3_t maxs )
{
	if( mins[0] <= maxs[0] || mins[1] <= maxs[1] || mins[2] <= maxs[2] )
		return false;
	return true;
}

/*
=================
BoundsIntersect
=================
*/
bool BoundsIntersect( const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2, vec_t epsilon )
{
	if( mins1[0] > maxs2[0] + epsilon || mins1[1] > maxs2[1] + epsilon || mins1[2] > maxs2[2] + epsilon )
		return false;
	if( maxs1[0] < mins2[0] - epsilon || maxs1[1] < mins2[1] - epsilon || maxs1[2] < mins2[2] - epsilon )
		return false;
	return true;
}

/*
=================
BoundsAndSphereIntersect
=================
*/
bool BoundsAndSphereIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t origin, vec_t radius )
{
	if( mins[0] > origin[0] + radius || mins[1] > origin[1] + radius || mins[2] > origin[2] + radius )
		return false;
	if( maxs[0] < origin[0] - radius || maxs[1] < origin[1] - radius || maxs[2] < origin[2] - radius )
		return false;
	return true;
}

/*
=================
SphereIntersect
=================
*/
bool SphereIntersect( const vec3_t vSphereCenter, vec_t fSphereRadiusSquared, const vec3_t vLinePt, const vec3_t vLineDir )
{
	vec_t	a, b, c, insideSqr;
	vec3_t	p;

	// translate sphere to origin.
	VectorSubtract( vLinePt, vSphereCenter, p );

	a = DotProduct( vLineDir, vLineDir );
	b = 2.0 * DotProduct( p, vLineDir );
	c = DotProduct( p, p ) - fSphereRadiusSquared;

	insideSqr = b * b - 4.0 * a * c;
	if( insideSqr <= 0.000001 )
		return false;
	return true;
}

void CalcST( const vec3_t p0, const vec3_t p1, const vec3_t p2, const vec2_t t0, const vec2_t t1, const vec2_t t2, vec3_t s, vec3_t t, bool areaweight )
{
	vec3_t	edge01, edge02, cross;

	// Compute the partial derivatives of X, Y, and Z with respect to S and T.
	VectorClear( s );
	VectorClear( t );

	// x, s, t
	VectorSet( edge01, p1[0] - p0[0], t1[0] - t0[0], t1[1] - t0[1] );
	VectorSet( edge02, p2[0] - p0[0], t2[0] - t0[0], t2[1] - t0[1] );
	CrossProduct( edge01, edge02, cross );

	if( fabs( cross[0] ) > SMALL_FLOAT )
	{
		s[0] += -cross[1] / cross[0];
		t[0] += -cross[2] / cross[0];
	}

	// y, s, t
	VectorSet( edge01, p1[1] - p0[1], t1[0] - t0[0], t1[1] - t0[1] );
	VectorSet( edge02, p2[1] - p0[1], t2[0] - t0[0], t2[1] - t0[1] );
	CrossProduct( edge01, edge02, cross );

	if( fabs( cross[0] ) > SMALL_FLOAT )
	{
		s[1] += -cross[1] / cross[0];
		t[1] += -cross[2] / cross[0];
	}

	// z, s, t
	VectorSet( edge01, p1[2] - p0[2], t1[0] - t0[0], t1[1] - t0[1] );
	VectorSet( edge02, p2[2] - p0[2], t2[0] - t0[0], t2[1] - t0[1] );
	CrossProduct( edge01, edge02, cross );

	if( fabs( cross[0] ) > SMALL_FLOAT )
	{
		s[2] += -cross[1] / cross[0];
		t[2] += -cross[2] / cross[0];
	}

	if( !areaweight )
	{
		// Normalize s and t
		VectorNormalize2( s );
		VectorNormalize2( t );
	}
}

/*
=============
COM_NormalizeAngles

=============
*/
void COM_NormalizeAngles( vec3_t angles )
{
	for( int i = 0; i < 3; i++ )
	{
		if( angles[i] > 180.0f )
			angles[i] -= 360.0f;
		else if( angles[i] < -180.0f )
			angles[i] += 360.0f;
	}
}

/*
====================
AngleQuaternion
====================
*/
void AngleQuaternion( const vec3_t angles, vec4_t quat )
{
	float	sr, sp, sy, cr, cp, cy;

	SinCos( angles[ROLL] * 0.5f, &sy, &cy );
	SinCos( angles[YAW] * 0.5f, &sp, &cp );
	SinCos( angles[PITCH] * 0.5f, &sr, &cr );

	quat[0] = sr * cp * cy - cr * sp * sy; // X
	quat[1] = cr * sp * cy + sr * cp * sy; // Y
	quat[2] = cr * cp * sy - sr * sp * cy; // Z
	quat[3] = cr * cp * cy + sr * sp * sy; // W
}

/*
====================
AngleMatrix
====================
*/
void Matrix3x4_CreateFromEntityScale3f( matrix3x4 out, const vec3_t angles, const vec3_t origin, const vec3_t scale )
{
	float	sr, sp, sy, cr, cp, cy;

	SinCos( DEG2RAD( angles[YAW] ), &sy, &cy );
	SinCos( DEG2RAD( angles[PITCH] ), &sp, &cp );
	SinCos( DEG2RAD( angles[ROLL] ), &sr, &cr );

	out[0][0] = (cp*cy) * scale[0];
	out[1][0] = (sr*sp*cy+cr*-sy) * scale[1];
	out[2][0] = (cr*sp*cy+-sr*-sy) * scale[2];
	out[3][0] = origin[0];
	out[0][1] = (cp*sy) * scale[0];
	out[1][1] = (sr*sp*sy+cr*cy) * scale[1];
	out[2][1] = (cr*sp*sy+-sr*cy) * scale[2];
	out[3][1] = origin[1];
	out[0][2] = (-sp) * scale[0];
	out[1][2] = (sr*cp) * scale[1];
	out[2][2] = (cr*cp) * scale[2];
	out[3][2] = origin[2];
}

/*
================
Matrix3x4_MatrixToEntityScale3f
================
*/
void Matrix3x4_MatrixToEntityScale3f( const matrix3x4 in, vec3_t origin, vec3_t angles, vec3_t scale )
{
	float xyDist = sqrt( in[0][0] * in[0][0] + in[0][1] * in[0][1] );

	if( xyDist > 0.001f )
	{
		// enough here to get angles?
		angles[0] = RAD2DEG( atan2( -in[0][2], xyDist ));
		angles[1] = RAD2DEG( atan2( in[0][1], in[0][0] ));
		angles[2] = RAD2DEG( atan2( in[1][2], in[2][2] ));
	}
	else
	{
		// forward is mostly Z, gimbal lock
		angles[0] = RAD2DEG( atan2( -in[0][2], xyDist ));
		angles[1] = RAD2DEG( atan2( -in[1][0], in[1][1] ));
		angles[2] = 0.0f;
	}

	origin[0] = in[3][0];
	origin[1] = in[3][1];
	origin[2] = in[3][2];

	scale[0] = VectorLength( in[0] );
	scale[1] = VectorLength( in[1] );
	scale[2] = VectorLength( in[2] );
}

/*
================
QuaternionMatrix
================
*/
void Matrix3x4_FromOriginQuat( matrix3x4 out, const vec4_t quaternion, const vec3_t origin )
{
	out[0][0] = 1.0f - 2.0f * quaternion[1] * quaternion[1] - 2.0f * quaternion[2] * quaternion[2];
	out[0][1] = 2.0f * quaternion[0] * quaternion[1] + 2.0f * quaternion[3] * quaternion[2];
	out[0][2] = 2.0f * quaternion[0] * quaternion[2] - 2.0f * quaternion[3] * quaternion[1];

	out[1][0] = 2.0f * quaternion[0] * quaternion[1] - 2.0f * quaternion[3] * quaternion[2];
	out[1][1] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[2] * quaternion[2];
	out[1][2] = 2.0f * quaternion[1] * quaternion[2] + 2.0f * quaternion[3] * quaternion[0];

	out[2][0] = 2.0f * quaternion[0] * quaternion[2] + 2.0f * quaternion[3] * quaternion[1];
	out[2][1] = 2.0f * quaternion[1] * quaternion[2] - 2.0f * quaternion[3] * quaternion[0];
	out[2][2] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[1] * quaternion[1];

	out[3][0] = origin[0];
	out[3][1] = origin[1];
	out[3][2] = origin[2];
}

/*
================
ConcatTransforms
================
*/
void Matrix3x4_ConcatTransforms( matrix3x4 out, const matrix3x4 in1, const matrix3x4 in2 )
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[1][0] * in2[0][1] + in1[2][0] * in2[0][2];
	out[1][0] = in1[0][0] * in2[1][0] + in1[1][0] * in2[1][1] + in1[2][0] * in2[1][2];
	out[2][0] = in1[0][0] * in2[2][0] + in1[1][0] * in2[2][1] + in1[2][0] * in2[2][2];
	out[3][0] = in1[0][0] * in2[3][0] + in1[1][0] * in2[3][1] + in1[2][0] * in2[3][2] + in1[3][0];
	out[0][1] = in1[0][1] * in2[0][0] + in1[1][1] * in2[0][1] + in1[2][1] * in2[0][2];
	out[1][1] = in1[0][1] * in2[1][0] + in1[1][1] * in2[1][1] + in1[2][1] * in2[1][2];
	out[2][1] = in1[0][1] * in2[2][0] + in1[1][1] * in2[2][1] + in1[2][1] * in2[2][2];
	out[3][1] = in1[0][1] * in2[3][0] + in1[1][1] * in2[3][1] + in1[2][1] * in2[3][2] + in1[3][1];
	out[0][2] = in1[0][2] * in2[0][0] + in1[1][2] * in2[0][1] + in1[2][2] * in2[0][2];
	out[1][2] = in1[0][2] * in2[1][0] + in1[1][2] * in2[1][1] + in1[2][2] * in2[1][2];
	out[2][2] = in1[0][2] * in2[2][0] + in1[1][2] * in2[2][1] + in1[2][2] * in2[2][2];
	out[3][2] = in1[0][2] * in2[3][0] + in1[1][2] * in2[3][1] + in1[2][2] * in2[3][2] + in1[3][2];
}

/*
====================
TransformStandardPlane
====================
*/
void Matrix3x4_TransformStandardPlane( const matrix3x4 in, const vec3_t normal, float d, vec3_t out, float *dist )
{
#if 0
	float	scale = sqrt( in[0][0] * in[0][0] + in[0][1] * in[0][1] + in[0][2] * in[0][2] );
	float	iscale = 1.0f / scale;

	out[0] = (normal[0] * in[0][0] + normal[1] * in[1][0] + normal[2] * in[2][0]) * iscale;
	out[1] = (normal[0] * in[0][1] + normal[1] * in[1][1] + normal[2] * in[2][1]) * iscale;
	out[2] = (normal[0] * in[0][2] + normal[1] * in[1][2] + normal[2] * in[2][2]) * iscale;
	*dist = d * scale + ( out[0] * in[3][0] + out[1] * in[3][1] + out[2] * in[3][2] );
#else
	// g-cont. because matrix is already inverted
	out[0] = (normal[0] * in[0][0] + normal[1] * in[1][0] + normal[2] * in[2][0]);
	out[1] = (normal[0] * in[0][1] + normal[1] * in[1][1] + normal[2] * in[2][1]);
	out[2] = (normal[0] * in[0][2] + normal[1] * in[1][2] + normal[2] * in[2][2]);
	*dist = -((normal[0] * in[3][0] + normal[1] * in[3][1] + normal[2] * in[3][2]) - d);
#endif
}

/*
====================
VectorTransform
====================
*/
void Matrix3x4_VectorTransform( const matrix3x4 in, const vec3_t v, vec3_t out )
{
	vec3_t	iv;

	// in case v == out
	iv[0] = v[0];
	iv[1] = v[1];
	iv[2] = v[2];

	out[0] = iv[0] * in[0][0] + iv[1] * in[1][0] + iv[2] * in[2][0] + in[3][0];
	out[1] = iv[0] * in[0][1] + iv[1] * in[1][1] + iv[2] * in[2][1] + in[3][1];
	out[2] = iv[0] * in[0][2] + iv[1] * in[1][2] + iv[2] * in[2][2] + in[3][2];
}

/*
====================
VectorTransform
====================
*/
void Matrix3x4_Vector4Transform( const matrix3x4 in, const vec4_t v, vec4_t out )
{
	vec4_t	iv;

	// in case v == out
	iv[0] = v[0];
	iv[1] = v[1];
	iv[2] = v[2];
	iv[3] = v[3];

	out[0] = iv[0] * in[0][0] + iv[1] * in[1][0] + iv[2] * in[2][0] + iv[3] * in[3][0];
	out[1] = iv[0] * in[0][1] + iv[1] * in[1][1] + iv[2] * in[2][1] + iv[3] * in[3][1];
	out[2] = iv[0] * in[0][2] + iv[1] * in[1][2] + iv[2] * in[2][2] + iv[3] * in[3][2];
}

/*
====================
VectorITransform
====================
*/
void Matrix3x4_VectorITransform( const matrix3x4 in, const vec3_t v, vec3_t out )
{
	vec3_t	iv;

	iv[0] = v[0] - in[3][0];
	iv[1] = v[1] - in[3][1];
	iv[2] = v[2] - in[3][2];

	out[0] = iv[0] * in[0][0] + iv[1] * in[0][1] + iv[2] * in[0][2];
	out[1] = iv[0] * in[1][0] + iv[1] * in[1][1] + iv[2] * in[1][2];
	out[2] = iv[0] * in[2][0] + iv[1] * in[2][1] + iv[2] * in[2][2];
}

/*
====================
VectorRotate
====================
*/
void Matrix3x4_VectorRotate( const matrix3x4 in, const vec3_t v, vec3_t out )
{
	vec3_t	iv;

	// in case v == out
	iv[0] = v[0];
	iv[1] = v[1];
	iv[2] = v[2];

	out[0] = iv[0] * in[0][0] + iv[1] * in[1][0] + iv[2] * in[2][0];
	out[1] = iv[0] * in[0][1] + iv[1] * in[1][1] + iv[2] * in[2][1];
	out[2] = iv[0] * in[0][2] + iv[1] * in[1][2] + iv[2] * in[2][2];
}

/*
====================
VectorIRotate
====================
*/
void Matrix3x4_VectorIRotate( const matrix3x4 in, const vec3_t v, vec3_t out )
{
	vec3_t	iv;

	// in case v == out
	iv[0] = v[0];
	iv[1] = v[1];
	iv[2] = v[2];

	out[0] = iv[0] * in[0][0] + iv[1] * in[0][1] + iv[2] * in[0][2];
	out[1] = iv[0] * in[1][0] + iv[1] * in[1][1] + iv[2] * in[1][2];
	out[2] = iv[0] * in[2][0] + iv[1] * in[2][1] + iv[2] * in[2][2];
}

/*
====================
CalcSign
====================
*/
vec_t Matrix3x4_CalcSign( const matrix3x4 in )
{
	vec3_t	out;

	out[0] = in[0][1] * in[1][2] - in[0][2] * in[1][1];
	out[1] = in[0][2] * in[1][0] - in[0][0] * in[1][2];
	out[2] = in[0][0] * in[1][1] - in[0][1] * in[1][0];

	return ( out[0] * in[2][0] + out[1] * in[2][1] + out[2] * in[2][2] );
}

/*
====================
Invert_Full
====================
*/
bool Matrix3x4_Invert_Full( matrix3x4 out, const matrix3x4 in )
{
	double	texplanes[2][4];
	double	faceplane[4];
	double	texaxis[2][3];
	double	normalaxis[3];
	double	det, sqrlen1, sqrlen2, sqrlen3;
	double	dot_epsilon = NORMAL_EPSILON * NORMAL_EPSILON;
	double	texorg[3];

	for( int i = 0; i < 4; i++ )
	{
		texplanes[0][i] = in[i][0];
		texplanes[1][i] = in[i][1];
		faceplane[i] = in[i][2];
	}

	sqrlen1 = DotProduct( texplanes[0], texplanes[0] );
	sqrlen2 = DotProduct( texplanes[1], texplanes[1] );
	sqrlen3 = DotProduct( faceplane, faceplane );

	if( sqrlen1 <= dot_epsilon || sqrlen2 <= dot_epsilon || sqrlen3 <= dot_epsilon )
	{
		// s gradient, t gradient or face normal is too close to 0
		return false;
	}

	CrossProduct( texplanes[0], texplanes[1], normalaxis );
	det = DotProduct( normalaxis, faceplane );

	if( det * det <= sqrlen1 * sqrlen2 * sqrlen3 * dot_epsilon )
	{
		// s gradient, t gradient and face normal are coplanar
		return false;
	}

	VectorScale( normalaxis, 1.0 / det, normalaxis );
	CrossProduct( texplanes[1], faceplane, texaxis[0] );
	VectorScale( texaxis[0], 1.0 / det, texaxis[0] );

	CrossProduct( faceplane, texplanes[0], texaxis[1] );
	VectorScale( texaxis[1], 1.0 / det, texaxis[1] );

	VectorScale( normalaxis, -faceplane[3], texorg );
	VectorMA( texorg, -texplanes[0][3], texaxis[0], texorg );
	VectorMA( texorg, -texplanes[1][3], texaxis[1], texorg );
	
	VectorCopy( texaxis[0], out[0] );
	VectorCopy( texaxis[1], out[1] );
	VectorCopy( normalaxis, out[2] );
	VectorCopy( texorg, out[3] );

	return true;
}

const matrix4x4 matrix4x4_identity =
{
{ 1.0f, 0.0f, 0.0f, 0.0f },	// PITCH
{ 0.0f, 1.0f, 0.0f, 0.0f },	// YAW
{ 0.0f, 0.0f, 1.0f, 0.0f },	// ROLL
{ 0.0f, 0.0f, 0.0f, 1.0f },	// ORIGIN
};

/*
========================================================================

		Matrix4x4 operations

========================================================================
*/
void Matrix4x4_Concat( matrix4x4 out, const matrix4x4 in1, const matrix4x4 in2 )
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[1][0] * in2[0][1] + in1[2][0] * in2[0][2] + in1[3][0] * in2[0][3];
	out[1][0] = in1[0][0] * in2[1][0] + in1[1][0] * in2[1][1] + in1[2][0] * in2[1][2] + in1[3][0] * in2[1][3];
	out[2][0] = in1[0][0] * in2[2][0] + in1[1][0] * in2[2][1] + in1[2][0] * in2[2][2] + in1[3][0] * in2[2][3];
	out[3][0] = in1[0][0] * in2[3][0] + in1[1][0] * in2[3][1] + in1[2][0] * in2[3][2] + in1[3][0] * in2[3][3];
	out[0][1] = in1[0][1] * in2[0][0] + in1[1][1] * in2[0][1] + in1[2][1] * in2[0][2] + in1[3][1] * in2[0][3];
	out[1][1] = in1[0][1] * in2[1][0] + in1[1][1] * in2[1][1] + in1[2][1] * in2[1][2] + in1[3][1] * in2[1][3];
	out[2][1] = in1[0][1] * in2[2][0] + in1[1][1] * in2[2][1] + in1[2][1] * in2[2][2] + in1[3][1] * in2[2][3];
	out[3][1] = in1[0][1] * in2[3][0] + in1[1][1] * in2[3][1] + in1[2][1] * in2[3][2] + in1[3][1] * in2[3][3];
	out[0][2] = in1[0][2] * in2[0][0] + in1[1][2] * in2[0][1] + in1[2][2] * in2[0][2] + in1[3][2] * in2[0][3];
	out[1][2] = in1[0][2] * in2[1][0] + in1[1][2] * in2[1][1] + in1[2][2] * in2[1][2] + in1[3][2] * in2[1][3];
	out[2][2] = in1[0][2] * in2[2][0] + in1[1][2] * in2[2][1] + in1[2][2] * in2[2][2] + in1[3][2] * in2[2][3];
	out[3][2] = in1[0][2] * in2[3][0] + in1[1][2] * in2[3][1] + in1[2][2] * in2[3][2] + in1[3][2] * in2[3][3];
	out[0][3] = in1[0][3] * in2[0][0] + in1[1][3] * in2[0][1] + in1[2][3] * in2[0][2] + in1[3][3] * in2[0][3];
	out[1][3] = in1[0][3] * in2[1][0] + in1[1][3] * in2[1][1] + in1[2][3] * in2[1][2] + in1[3][3] * in2[1][3];
	out[2][3] = in1[0][3] * in2[2][0] + in1[1][3] * in2[2][1] + in1[2][3] * in2[2][2] + in1[3][3] * in2[2][3];
	out[3][3] = in1[0][3] * in2[3][0] + in1[1][3] * in2[3][1] + in1[2][3] * in2[3][2] + in1[3][3] * in2[3][3];
}

bool Matrix4x4_Invert_Full( matrix4x4 out, const matrix4x4 in1 )
{
	float	*temp;
	float	*r[4];
	float	rtemp[4][8];
	float	m[4];
	float	s;

	r[0] = rtemp[0];
	r[1] = rtemp[1];
	r[2] = rtemp[2];
	r[3] = rtemp[3];

	r[0][0] = in1[0][0];
	r[0][1] = in1[1][0];
	r[0][2] = in1[2][0];
	r[0][3] = in1[3][0];
	r[0][4] = 1.0f;
	r[0][5] = 0.0f;
	r[0][6] = 0.0f;
	r[0][7] = 0.0f;

	r[1][0] = in1[0][1];
	r[1][1] = in1[1][1];
	r[1][2] = in1[2][1];
	r[1][3] = in1[3][1];
	r[1][5] = 1.0f;
	r[1][4] =	0.0f;
	r[1][6] =	0.0f;
	r[1][7] = 0.0f;

	r[2][0] = in1[0][2];
	r[2][1] = in1[1][2];
	r[2][2] = in1[2][2];
	r[2][3] = in1[3][2];
	r[2][6] = 1.0f;
	r[2][4] =	0.0f;
	r[2][5] =	0.0f;
	r[2][7] = 0.0f;

	r[3][0] = in1[0][3];
	r[3][1] = in1[1][3];
	r[3][2] = in1[2][3];
	r[3][3] = in1[3][3];
	r[3][4] =	0.0f;
	r[3][5] =	0.0f;
	r[3][6] = 0.0f;
	r[3][7] = 1.0f;	

	if( fabs( r[3][0] ) > fabs( r[2][0] ))
	{
		temp = r[3];
		r[3] = r[2];
		r[2] = temp;
	}

	if( fabs( r[2][0] ) > fabs( r[1][0] ))
	{
		temp = r[2];
		r[2] = r[1];
		r[1] = temp;
	}

	if( fabs( r[1][0] ) > fabs( r[0][0] ))
	{
		temp = r[1];
		r[1] = r[0];
		r[0] = temp;
	}

	if( r[0][0] )
	{
		m[1] = r[1][0] / r[0][0];
		m[2] = r[2][0] / r[0][0];
		m[3] = r[3][0] / r[0][0];

		s = r[0][1];
		r[1][1] -= m[1] * s;
		r[2][1] -= m[2] * s;
		r[3][1] -= m[3] * s;

		s = r[0][2];
		r[1][2] -= m[1] * s;
		r[2][2] -= m[2] * s;
		r[3][2] -= m[3] * s;

		s = r[0][3];
		r[1][3] -= m[1] * s;
		r[2][3] -= m[2] * s;
		r[3][3] -= m[3] * s;

		s = r[0][4];
		if( s )
		{
			r[1][4] -= m[1] * s;
			r[2][4] -= m[2] * s;
			r[3][4] -= m[3] * s;
		}

		s = r[0][5];
		if( s )
		{
			r[1][5] -= m[1] * s;
			r[2][5] -= m[2] * s;
			r[3][5] -= m[3] * s;
		}

		s = r[0][6];
		if( s )
		{
			r[1][6] -= m[1] * s;
			r[2][6] -= m[2] * s;
			r[3][6] -= m[3] * s;
		}

		s = r[0][7];
		if( s )
		{
			r[1][7] -= m[1] * s;
			r[2][7] -= m[2] * s;
			r[3][7] -= m[3] * s;
		}

		if( fabs( r[3][1] ) > fabs( r[2][1] ))
		{
			temp = r[3];
			r[3] = r[2];
			r[2] = temp;
		}

		if( fabs( r[2][1] ) > fabs( r[1][1] ))
		{
			temp = r[2];
			r[2] = r[1];
			r[1] = temp;
		}

		if( r[1][1] )
		{
			m[2] = r[2][1] / r[1][1];
			m[3] = r[3][1] / r[1][1];
			r[2][2] -= m[2] * r[1][2];
			r[3][2] -= m[3] * r[1][2];
			r[2][3] -= m[2] * r[1][3];
			r[3][3] -= m[3] * r[1][3];

			s = r[1][4];
			if( s )
			{
				r[2][4] -= m[2] * s;
				r[3][4] -= m[3] * s;
			}

			s = r[1][5];
			if( s )
			{
				r[2][5] -= m[2] * s;
				r[3][5] -= m[3] * s;
			}

			s = r[1][6];
			if( s )
			{
				r[2][6] -= m[2] * s;
				r[3][6] -= m[3] * s;
			}

			s = r[1][7];
			if( s )
			{
				r[2][7] -= m[2] * s;
				r[3][7] -= m[3] * s;
			}

			if( fabs( r[3][2] ) > fabs( r[2][2] ))
			{
				temp = r[3];
				r[3] = r[2];
				r[2] = temp;
			}

			if( r[2][2] )
			{
				m[3] = r[3][2] / r[2][2];
				r[3][3] -= m[3] * r[2][3];
				r[3][4] -= m[3] * r[2][4];
				r[3][5] -= m[3] * r[2][5];
				r[3][6] -= m[3] * r[2][6];
				r[3][7] -= m[3] * r[2][7];

				if( r[3][3] )
				{
					s = 1.0f / r[3][3];
					r[3][4] *= s;
					r[3][5] *= s;
					r[3][6] *= s;
					r[3][7] *= s;

					m[2] = r[2][3];
					s = 1.0f / r[2][2];
					r[2][4] = s * (r[2][4] - r[3][4] * m[2]);
					r[2][5] = s * (r[2][5] - r[3][5] * m[2]);
					r[2][6] = s * (r[2][6] - r[3][6] * m[2]);
					r[2][7] = s * (r[2][7] - r[3][7] * m[2]);

					m[1] = r[1][3];
					r[1][4] -= r[3][4] * m[1];
					r[1][5] -= r[3][5] * m[1];
					r[1][6] -= r[3][6] * m[1];
					r[1][7] -= r[3][7] * m[1];

					m[0] = r[0][3];
					r[0][4] -= r[3][4] * m[0];
					r[0][5] -= r[3][5] * m[0];
					r[0][6] -= r[3][6] * m[0];
					r[0][7] -= r[3][7] * m[0];

					m[1] = r[1][2];
					s = 1.0f / r[1][1];
					r[1][4] = s * (r[1][4] - r[2][4] * m[1]);
					r[1][5] = s * (r[1][5] - r[2][5] * m[1]);
					r[1][6] = s * (r[1][6] - r[2][6] * m[1]);
					r[1][7] = s * (r[1][7] - r[2][7] * m[1]);

					m[0] = r[0][2];
					r[0][4] -= r[2][4] * m[0];
					r[0][5] -= r[2][5] * m[0];
					r[0][6] -= r[2][6] * m[0];
					r[0][7] -= r[2][7] * m[0];

					m[0] = r[0][1];
					s = 1.0f / r[0][0];
					r[0][4] = s * (r[0][4] - r[1][4] * m[0]);
					r[0][5] = s * (r[0][5] - r[1][5] * m[0]);
					r[0][6] = s * (r[0][6] - r[1][6] * m[0]);
					r[0][7] = s * (r[0][7] - r[1][7] * m[0]);

					out[0][0]	= r[0][4];
					out[0][1]	= r[1][4];
					out[0][2]	= r[2][4];
					out[0][3]	= r[3][4];
					out[1][0]	= r[0][5];
					out[1][1]	= r[1][5];
					out[1][2]	= r[2][5];
					out[1][3]	= r[3][5];
					out[2][0]	= r[0][6];
					out[2][1]	= r[1][6];
					out[2][2]	= r[2][6];
					out[2][3]	= r[3][6];
					out[3][0]	= r[0][7];
					out[3][1]	= r[1][7];
					out[3][2]	= r[2][7];
					out[3][3]	= r[3][7];

					return true;
				}
			}
		}
	}

	return false;
}
