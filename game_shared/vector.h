//=======================================================================
//			Copyright (C) Shambler Team 2005
//		         vector.h - shared vector operations 
//=======================================================================
#ifndef VECTOR_H
#define VECTOR_H

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include "port.h"
#include "mathlib_defs.h"

#define PITCH		0
#define YAW		1
#define ROLL		2

#define DIST_EPSILON	(1.0f / 32.0f)
#define STOP_EPSILON	0.1f
#define ON_EPSILON		0.1f

#define M_EPSILON		1.192092896e-07f
#define M_INFINITY		1e30f

#define BIT( n )		(1<<( n ))
#define RAD2DEG( x )	((float)(x) * (float)(180.f / M_PI))
#define DEG2RAD( x )	((float)(x) * (float)(M_PI / 180.f))
#define Q_rint( x )		((x) < 0 ? ((int)((x)-0.5f)) : ((int)((x)+0.5f)))

#pragma warning( disable : 4244 )	// disable 'possible loss of data converting float to int' warning message
#pragma warning( disable : 4305 )	// disable 'truncation from 'const double' to 'float' warning message

namespace physx {
	class PxVec3;
};
class Radian;

inline void SinCos( float angle, float *sine, float *cosine ) 
{
	/*__asm
	{
		push	ecx
		fld	dword ptr angle
		fsincos
		mov	ecx, dword ptr[cosine]
		fstp      dword ptr [ecx]
		mov 	ecx, dword ptr[sine]
		fstp	dword ptr [ecx]
		pop	ecx
	}*/
	*sine = sin(angle);
	*cosine = cos(angle);
}

//=========================================================
// 2DVector - used for many pathfinding and many other 
// operations that are treated as planar rather than 3d.
//=========================================================
class Vector2D
{
public:
	inline Vector2D(void) { }
	inline Vector2D(float X, float Y) { x = X; y = Y; }
	inline Vector2D( const float *rgfl ) { x = rgfl[0]; y = rgfl[1]; }
	inline Vector2D(float rgfl[2]) { x = rgfl[0]; y = rgfl[1];   }
	inline Vector2D operator+(const Vector2D& v) const { return Vector2D(x+v.x, y+v.y); }
	inline Vector2D operator-(const Vector2D& v) const { return Vector2D(x-v.x, y-v.y); }
	inline Vector2D operator*(float fl) const { return Vector2D(x*fl, y*fl); }
	inline Vector2D operator/(float fl) const { return Vector2D(x/fl, y/fl); }

	_forceinline Vector2D& operator+=(const Vector2D &v)
	{
		x+=v.x; y+=v.y;	
		return *this;
	}			
	_forceinline Vector2D& operator-=(const Vector2D &v)
	{
		x-=v.x; y-=v.y;	
		return *this;
	}		
	_forceinline Vector2D& operator*=(const Vector2D &v)
	{
		x *= v.x; y *= v.y;
		return *this;
	}
	_forceinline Vector2D& operator*=(float s)
	{
		x *= s; y *= s;
		return *this;
	}
	_forceinline Vector2D& operator/=(const Vector2D &v)
	{
		x /= v.x; y /= v.y;
		return *this;
	}		
	_forceinline Vector2D& operator/=(float s)
	{
		float oofl = 1.0f / s;
		x *= oofl; y *= oofl;
		return *this;
	}

	operator float *()		 { return &x; } // Vectors will now automatically convert to float * when needed
	operator const float *() const { return &x; } 
	
	inline float Length(void) const { return sqrt(x*x + y*y ); }
	inline Vector2D Normalize ( void ) const
	{
		Vector2D vec2;

		float flLen = Length();
		if ( flLen == 0 )
		{
			return Vector2D( 0, 0 );
		}
		else
		{
			flLen = 1 / flLen;
			return Vector2D( x * flLen, y * flLen );
		}
	}
	float x, y;
};
typedef Vector2D vec2_t;
#define IS_NAN(x)		(((*(int *)&x) & (255<<23)) == (255<<23))

inline float DotProduct(const Vector2D& a, const Vector2D& b) { return( a.x*b.x + a.y*b.y ); }
inline Vector2D operator*(float fl, const Vector2D& v) { return v * fl; }

//=========================================================
// 3D Vector
//=========================================================
class Vector	// same data-layout as engine's vec3_t,
{		// which is a float[3]
public:
	// Construction/destruction
	inline Vector(void)				{ }
	inline Vector(float X, float Y, float Z)	{ x = X; y = Y; z = Z;                     }
	inline Vector(const Vector& v)		{ x = v.x; y = v.y; z = v.z;		   }
	inline Vector( const float *rgfl )		{ x = rgfl[0]; y = rgfl[1]; z = rgfl[2];   }
	inline Vector(float rgfl[3])			{ x = rgfl[0]; y = rgfl[1]; z = rgfl[2];   }
	inline Vector( float fill )			{ x = fill; y = fill; z = fill;            }
	Vector(const physx::PxVec3& v);

	// Initialization
	void Init(float ix=0.0f, float iy=0.0f, float iz=0.0f){ x = ix; y = iy; z = iz; }

	// Operators
	inline Vector operator-(void) const		{ return Vector(-x,-y,-z);		   }
	inline int operator==(const Vector& v) const	{ return x==v.x && y==v.y && z==v.z;	   }
	inline int operator!=(const Vector& v) const	{ return !(*this==v);		   }
	inline Vector operator+(const Vector& v) const	{ return Vector(x+v.x, y+v.y, z+v.z);	   }
	inline Vector operator-(const Vector& v) const	{ return Vector(x-v.x, y-v.y, z-v.z);	   }
	inline Vector operator+(float fl) const		{ return Vector(x+fl, y+fl, z+fl);	   }
	inline Vector operator-(float fl) const		{ return Vector(x-fl, y-fl, z-fl);	   }
	inline Vector operator*(float fl) const		{ return Vector(x*fl, y*fl, z*fl);	   }
	inline Vector operator/(float fl) const		{ return Vector(x/fl, y/fl, z/fl);	   }
	inline Vector operator*(const Vector& v) const	{ return Vector(x*v.x, y*v.y, z*v.z);	   }
	operator float *()								{ return &x; } // Vectors will now automatically convert to float * when needed
	operator const float *() const					{ return &x; } 
	const Vector& operator=(const physx::PxVec3& v);
	operator physx::PxVec3() const;

	_forceinline Vector& operator+=(const Vector &v)
	{
		x+=v.x; y+=v.y; z += v.z;	
		return *this;
	}			
	_forceinline Vector& operator-=(const Vector &v)
	{
		x-=v.x; y-=v.y; z -= v.z;	
		return *this;
	}		
	_forceinline Vector& operator*=(const Vector &v)
	{
		x *= v.x; y *= v.y; z *= v.z;
		return *this;
	}
	_forceinline Vector& operator*=(float s)
	{
		x *= s; y *= s; z *= s;
		return *this;
	}
	_forceinline Vector& operator/=(const Vector &v)
	{
		x /= v.x; y /= v.y; z /= v.z;
		return *this;
	}		
	_forceinline Vector& operator/=(float s)
	{
		float oofl = 1.0f / s;
		x *= oofl; y *= oofl; z *= oofl;
		return *this;
	}

	_forceinline Vector& fixangle(void)
	{
		if (!IS_NAN( x ))
		{
			while ( x < 0 ) x += 360;
			while ( x > 360 ) x -= 360;
		}
		if (!IS_NAN( y ))
		{
			while ( y < 0 ) y += 360;
			while ( y > 360 ) y -= 360;
		}
		if (!IS_NAN( z ))
		{
			while ( z < 0 ) z += 360;
			while ( z > 360 ) z -= 360;
		}
		return *this;
	}

	_forceinline Vector MA(  float scale, const Vector &start, const Vector &direction ) const
	{
		return Vector(start.x + scale * direction.x, start.y + scale * direction.y, start.z + scale * direction.z) ;
	}

	_forceinline bool IsEqual( const Vector &vec, const float epsilon ) const
	{
		if( fabs( x - vec.x ) > epsilon ) return false;
		if( fabs( y - vec.y ) > epsilon ) return false;
		if( fabs( z - vec.z ) > epsilon ) return false;

		return true;
	}
	
	_forceinline Vector Abs(void) const
	{
		return Vector(fabs(x), fabs(y), fabs(z));
	}

	_forceinline float Average(void) const 
	{ 
		return (x + y + z) / 3.0f; 
	}

	_forceinline void Assign(float value)
	{
		x = value;
		y = value;
		z = value;
	}

	_forceinline bool IsNull(float epsilon = 0.001f) const
	{
		if (fabs(x) > epsilon) return false;
		if (fabs(y) > epsilon) return false;
		if (fabs(z) > epsilon) return false;
		return true;
	}

	// Methods
	inline void CopyToArray( float *rgfl ) const	{ rgfl[0] = x, rgfl[1] = y, rgfl[2] = z; }
	inline float Length(void) const					{ return sqrt( x*x + y*y + z*z ); }
	inline float LengthSqr(void) const				{ return (x*x + y*y + z*z); }
	inline float MaxCoord() const					{ return Q_max(x, Q_max(y, z)); }
		
	inline Vector Normalize() const
	{
		float flLen = Length();

		if( flLen )
		{
			flLen = 1.0f / flLen;
			return Vector( x * flLen, y * flLen, z * flLen );
		}

		return *this; // can't normalize
	}

	// unsafe and not recommended to use, instead use Normalize()
	inline Vector NormalizeFast() const
	{
		float ilength = ReverseSqrt(x * x + y * y + z * z);
		return Vector(x * ilength, y * ilength, z * ilength);
	}

	inline float NormalizeLength( void )
	{
		float flLen = Length();

		if( flLen )
		{
			float flInvLen = 1.0f / flLen;
			x *= flInvLen, y *= flInvLen, z *= flInvLen;
		}

		return flLen;
	}

	float Dot( Vector const& vOther ) const
    {
		return x * vOther.x + y * vOther.y + z * vOther.z;
    }

	Vector Cross(const Vector &vOther) const
	{
		return Vector(y*vOther.z - z*vOther.y, z*vOther.x - x*vOther.z, x*vOther.y - y*vOther.x);
	}

	inline void Cross( const Vector &vec1, const Vector &vec2 )
	{
		x = vec1.y * vec2.z - vec1.z * vec2.y;
		y = vec1.z * vec2.x - vec1.x * vec2.z;
		z = vec1.x * vec2.y - vec1.y * vec2.x;
	}

	inline Vector2D Make2D ( void ) const
	{
		Vector2D	Vec2;
		Vec2.x = x;
		Vec2.y = y;
		return Vec2;
	}

	inline float Length2D(void) const { return sqrt(x*x + y*y); }

	// Members
	float x, y, z;
private:
	inline float ReverseSqrt(float number) const 
	{
		const float	threehalfs = 1.5F;
		float x2 = number * 0.5F;
		float y = number;
		int i = *(int *)&y;	// evil floating point bit level hacking
		i = 0x5f3759df - (i >> 1);	// what the fuck?
		y = *(float *)&i;
		y = y * (1.5F - (x2 * y * y));   // 1st iteration
		return y;
	}
};

inline Vector operator* ( float fl, const Vector& v ) { return v * fl; }
inline float DotProduct(const Vector& a, const Vector& b ) { return( a.x * b.x + a.y * b.y + a.z * b.z ); }
inline float DotProductAbs( const Vector& a, const Vector& b ) { return( fabs( a.x * b.x ) + fabs( a.y * b.y ) + fabs( a.z * b.z )); }
inline Vector CrossProduct( const Vector& a, const Vector& b ) { return Vector( a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x ); }
inline void VectorLerp( const Vector& src1, float t, const Vector& src2, Vector& dest )
{
	dest.x = src1.x + (src2.x - src1.x) * t;
	dest.y = src1.y + (src2.y - src1.y) * t;
	dest.z = src1.z + (src2.z - src1.z) * t;
}

inline void VectorMA(const float *veca, float scale, const float *vecb, float *vecc)
{
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}

typedef Vector vec3_t;

//=========================================================
// 4D Vector - for matrix operations
//=========================================================
class Vector4D					
{
public:
	// Members
	float x, y, z, w;

	// Construction/destruction
	inline Vector4D( void ) { }
	inline Vector4D( float X, float Y, float Z, float W ) { x = X; y = Y; z = Z; w = W; }
	inline Vector4D( const Vector4D& v ) { x = v.x; y = v.y; z = v.z, w = v.w; } 
	inline Vector4D( const float *pFloat ) { x = pFloat[0]; y = pFloat[1]; z = pFloat[2]; w = pFloat[3];}
	inline Vector4D( const Vector& v ) { x = v.x; y = v.y; z = v.z; w = 1.0f; }
	inline Vector4D( Radian const &angle );	// evil auto type promotion!!!

	// Initialization
	void Init( float ix = 0.0f, float iy = 0.0f, float iz = 0.0f, float iw = 0.0f )
	{
		x = ix; y = iy; z = iz; w = iw;
	}

	// Vectors will now automatically convert to float * when needed
	operator float *()					{ return &x; }
	operator const float *() const			{ return &x; } 

	// Vectors will now automatically convert to Vector when needed
	operator Vector()					{ return Vector( x, y, z ); }
	operator const Vector() const				{ return Vector( x, y, z ); } 

	inline float Length(void) const		{ return sqrt( x*x + y*y + z*z + w*w); }
	inline float LengthSqr(void) const		{ return (x*x + y*y + z*z + w*w); }

	inline Vector4D Normalize( void ) const
	{
		float flLen = Length();

		if( flLen )
		{
			flLen = 1.0f / flLen;
			return Vector4D( x * flLen, y * flLen, z * flLen, w * flLen );
		}

		return *this; // can't normalize
	}

	// equality
	bool operator==(const Vector4D& v) const { return v.x==x && v.y==y && v.z==z && v.w==w; }
	bool operator!=(const Vector4D& v) const { return !(*this==v); }
	inline Vector4D operator+(const Vector4D& v) const { return Vector4D(x+v.x, y+v.y, z+v.z, w+v.w); }
	inline Vector4D operator-(const Vector4D& v) const { return Vector4D(x-v.x, y-v.y, z-v.z, w-v.w); }
};

inline float DotProduct( const Vector4D& a, const Vector4D& b ) { return( a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w ); }

typedef Vector4D vec4_t;

class Radian
{
public:
	inline Radian( void ) { }
	inline Radian( float X, float Y, float Z )	{ x = X; y = Y; z = Z; }
	inline Radian( Vector4D const &q );	// evil auto type promotion!!!

	// initialization
	inline void Init( float ix = 0.0f, float iy = 0.0f, float iz = 0.0f )	{ x = ix; y = iy; z = iz; }

	// Vectors will now automatically convert to float * when needed
	operator float *()				{ return &x; }
	operator const float *() const		{ return &x; } 

	// Operators
	inline Radian operator-(void) const		{ return Radian(-x,-y,-z);		   }
	inline int operator==(const Radian& v) const	{ return x==v.x && y==v.y && z==v.z;	   }
	inline int operator!=(const Radian& v) const	{ return !(*this==v);		   }
	inline Radian operator+(const Radian& v) const	{ return Radian(x+v.x, y+v.y, z+v.z);	   }
	inline Radian operator-(const Radian& v) const	{ return Radian(x-v.x, y-v.y, z-v.z);	   }
	inline Radian operator+(float fl) const		{ return Radian(x+fl, y+fl, z+fl);	   }
	inline Radian operator-(float fl) const		{ return Radian(x-fl, y-fl, z-fl);	   }
	inline Radian operator*(float fl) const		{ return Radian(x*fl, y*fl, z*fl);	   }
	inline Radian operator/(float fl) const		{ return Radian(x/fl, y/fl, z/fl);	   }
	inline Radian operator*(const Radian& v) const	{ return Radian(x*v.x, y*v.y, z*v.z);	   }
	const Radian& operator=(const physx::PxVec3& v);
	
	_forceinline Radian& operator+=(const Radian &v)
	{
		x+=v.x; y+=v.y; z += v.z;	
		return *this;
	}			
	_forceinline Radian& operator-=(const Radian &v)
	{
		x-=v.x; y-=v.y; z -= v.z;	
		return *this;
	}		
	_forceinline Radian& operator*=(const Radian &v)
	{
		x *= v.x; y *= v.y; z *= v.z;
		return *this;
	}
	_forceinline Radian& operator*=(float s)
	{
		x *= s; y *= s; z *= s;
		return *this;
	}
	_forceinline Radian& operator/=(const Radian &v)
	{
		x /= v.x; y /= v.y; z /= v.z;
		return *this;
	}		
	_forceinline Radian& operator/=(float s)
	{
		float oofl = 1.0f / s;
		x *= oofl; y *= oofl; z *= oofl;
		return *this;
	}

	float x, y, z;
};

extern void AngleQuaternion( Radian const &angles, Vector4D &qt );
extern void QuaternionAngle( Vector4D const &q, Radian &angles );

inline Radian :: Radian( Vector4D const &q )
{
	QuaternionAngle( q, *this );
}

inline Vector4D :: Vector4D( Radian const &angle )
{
	AngleQuaternion( angle, *this );
}

extern const Vector g_vecZero;
extern const Radian g_radZero;

#endif//VECTOR_H
