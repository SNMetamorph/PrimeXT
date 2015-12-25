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

#define PITCH		0
#define YAW		1
#define ROLL		2

#define DIST_EPSILON	(1.0f / 32.0f)
#define STOP_EPSILON	0.1f
#define ON_EPSILON		0.1f

#define BIT( n )		(1<<( n ))
#define RAD2DEG( x )	((float)(x) * (float)(180.f / M_PI))
#define DEG2RAD( x )	((float)(x) * (float)(M_PI / 180.f))
#define Q_rint( x )		((x) < 0 ? ((int)((x)-0.5f)) : ((int)((x)+0.5f)))

#pragma warning( disable : 4244 )	// disable 'possible loss of data converting float to int' warning message
#pragma warning( disable : 4305 )	// disable 'truncation from 'const double' to 'float' warning message

class NxVec3;

inline void SinCos( float angle, float *sine, float *cosine ) 
{
	__asm
	{
		push	ecx
		fld	dword ptr angle
		fsincos
		mov	ecx, dword ptr[cosine]
		fstp      dword ptr [ecx]
		mov 	ecx, dword ptr[sine]
		fstp	dword ptr [ecx]
		pop	ecx
	}
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

#define IS_NAN(x)		(((*(int *)&x) & (255<<23)) == (255<<23))

inline float DotProduct(const Vector2D& a, const Vector2D& b) { return( a.x*b.x + a.y*b.y ); }
inline Vector2D operator*(float fl, const Vector2D& v) { return v * fl; }

class NxVec3;

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
	Vector(const NxVec3& v);

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
	const Vector& operator=(const NxVec3& v);
	
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
	
	// Methods
	inline void CopyToArray(float* rgfl) const	{ rgfl[0] = x, rgfl[1] = y, rgfl[2] = z;   }
	inline float Length(void) const		{ return sqrt(x*x + y*y + z*z); }
	operator float *()				{ return &x; } // Vectors will now automatically convert to float * when needed
	operator const float *() const		{ return &x; } 
		
	inline Vector Normalize(void) const
	{
		float flLen = Length();
		if (flLen == 0) return Vector(0,0,1); // ????
		flLen = 1 / flLen;
		return Vector(x * flLen, y * flLen, z * flLen);
	}

	float Dot( Vector const& vOther ) const
          {
          	return(x*vOther.x+y*vOther.y+z*vOther.z);
          }

	Vector Cross(const Vector &vOther) const
	{
		return Vector(y*vOther.z - z*vOther.y, z*vOther.x - x*vOther.z, x*vOther.y - y*vOther.x);
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

#define vec3_t Vector

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

	// equality
	bool operator==(const Vector4D& v) const { return v.x==x && v.y==y && v.z==z && v.w==w; }
	bool operator!=(const Vector4D& v) const { return !(*this==v); }
};

extern const Vector g_vecZero;

#endif//VECTOR_H
