//=======================================================================
//			Copyright (C) XashXT Group 2011
//=======================================================================

#ifndef MATRIX_H
#define MATRIX_H

#include <vector.h>

#ifndef M_PI
#define M_PI	3.14159265358979323846	// matches value in gcc v2 math.h
#endif

class matrix3x4;
class matrix4x4;
struct mplane_s;

class matrix3x3
{
public:
	matrix3x3();
	matrix3x3( float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22 )
	{
		mat[0][0] = m00;
		mat[0][1] = m01;
		mat[0][2] = m02;
		mat[1][0] = m10;
		mat[1][1] = m11;
		mat[1][2] = m12;
		mat[2][0] = m20;
		mat[2][1] = m21;
		mat[2][2] = m22;
	}

	// init from quaternion
	_forceinline matrix3x3( const Vector4D &quaternion )
	{
		mat[0][0] = 1.0f - 2.0f * (quaternion.y * quaternion.y + quaternion.z * quaternion.z);
		mat[1][0] = 2.0f * (quaternion.x * quaternion.y - quaternion.z * quaternion.w);
		mat[2][0] = 2.0f * (quaternion.x * quaternion.z + quaternion.y * quaternion.w);
		mat[0][1] = 2.0f * (quaternion.x * quaternion.y + quaternion.z * quaternion.w);
		mat[1][1] = 1.0f - 2.0f * (quaternion.x * quaternion.x + quaternion.z * quaternion.z);
		mat[2][1] = 2.0f * (quaternion.y * quaternion.z - quaternion.x * quaternion.w);
		mat[0][2] = 2.0f * (quaternion.x * quaternion.z - quaternion.y * quaternion.w);
		mat[1][2] = 2.0f * (quaternion.y * quaternion.z + quaternion.x * quaternion.w);
		mat[2][2] = 1.0f - 2.0f * (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
	}

	// init from angles
	_forceinline matrix3x3( const Vector &angles )
	{
		float	angle, sr, sp, sy, cr, cp, cy;

		if( angles[ROLL] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );
			angle = angles[ROLL] * (M_PI*2 / 360);
			SinCos( angle, &sr, &cr );

			mat[0][0] = (cp*cy);
			mat[1][0] = (sr*sp*cy+cr*-sy);
			mat[2][0] = (cr*sp*cy+-sr*-sy);
			mat[0][1] = (cp*sy);
			mat[1][1] = (sr*sp*sy+cr*cy);
			mat[2][1] = (cr*sp*sy+-sr*cy);
			mat[0][2] = (-sp);
			mat[1][2] = (sr*cp);
			mat[2][2] = (cr*cp);
		}
		else if( angles[PITCH] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );

			mat[0][0] = (cp*cy);
			mat[1][0] = (-sy);
			mat[2][0] = (sp*cy);
			mat[0][1] = (cp*sy);
			mat[1][1] = (cy);
			mat[2][1] = (sp*sy);
			mat[0][2] = (-sp);
			mat[1][2] = 0;
			mat[2][2] = (cp);
		}
		else if( angles[YAW] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );

			mat[0][0] = (cy);
			mat[1][0] = (-sy);
			mat[2][0] = 0;
			mat[0][1] = (sy);
			mat[1][1] = (cy);
			mat[2][1] = 0;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = 1;
		}
		else
		{
			mat[0][0] = 1;
			mat[1][0] = 0;
			mat[2][0] = 0;
			mat[0][1] = 0;
			mat[1][1] = 1;
			mat[2][1] = 0;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = 1;
		}
	}

	void Identity();

	inline bool IsIdentity( void ) const
	{
		if( mat[0][0] != 1.0f || mat[0][1] != 0.0f || mat[0][2] != 0.0f )
			return false;
		if( mat[1][0] != 0.0f || mat[1][1] != 1.0f || mat[1][2] != 0.0f )
			return false;
		if( mat[2][0] != 0.0f || mat[2][1] != 0.0f || mat[2][2] != 1.0f )
			return false;
		return true;
	}

	// array access
	float* operator[]( int i ) { return mat[i]; }
	float const* operator[]( int i ) const { return mat[i]; }
	operator float *() { return (float *)&mat[0][0]; }
	operator const float *() const { return (float *)&mat[0][0]; } 

	_forceinline int operator == ( const matrix3x3& mat2 ) const
	{
		if( mat[0][0] != mat2[0][0] || mat[0][1] != mat2[0][1] || mat[0][2] != mat2[0][2] )
			return false;
		if( mat[1][0] != mat2[1][0] || mat[1][1] != mat2[1][1] || mat[1][2] != mat2[1][2] )
			return false;
		if( mat[2][0] != mat2[2][0] || mat[2][1] != mat2[2][1] || mat[2][2] != mat2[2][2] )
			return false;
		return true;
	}

	_forceinline int operator != ( const matrix3x3& mat2 ) const { return !(*this == mat2 ); }
	matrix3x3& operator=(const matrix3x4 &vOther);
	matrix3x3& operator=(const matrix4x4 &vOther);

	// Access the basis vectors.
	Vector	GetForward() const { return mat[0]; };
	Vector	GetRight() const { return mat[1]; };
	Vector	GetUp() const { return mat[2]; };
	Vector	GetRow( int i ) const { return mat[i]; }

	void	SetForward( const Vector &vForward ) { mat[0] = vForward; };
	void	SetRight( const Vector &vRight ) { mat[1] = vRight; };
	void	SetUp( const Vector &vUp ) { mat[2] = vUp; };

	void	FromVector( const Vector &forward );

	Vector	GetAngles( void )
	{
		float xyDist = sqrt( mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1] );
		Vector angles;

		// enough here to get angles?
		if( xyDist > 0.001f )
		{
			angles[0] = RAD2DEG( atan2( -mat[0][2], xyDist ) );
			angles[1] = RAD2DEG( atan2( mat[0][1], mat[0][0] ) );
			angles[2] = RAD2DEG( atan2( mat[1][2], mat[2][2] ) );
		}
		else	// forward is mostly Z, gimbal lock
		{
			angles[0] = RAD2DEG( atan2( -mat[0][2], xyDist ) );
			angles[1] = RAD2DEG( atan2( -mat[1][0], mat[1][1] ) );
			angles[2] = 0;
		}

		return angles;
	}

	void	GetAngles( Vector &angles ) { angles = GetAngles(); }

	void	GetAngles( Radian &angles )
	{
		float xyDist = sqrt( mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1] );

		// enough here to get angles?
		if( xyDist > 0.001f )
		{
			angles.y = atan2( -mat[0][2], xyDist );
			angles.z = atan2( mat[0][1], mat[0][0] );
			angles.x = atan2( mat[1][2], mat[2][2] );
		}
		else	// forward is mostly Z, gimbal lock
		{
			angles.y = atan2( -mat[0][2], xyDist );
			angles.z = atan2( -mat[1][0], mat[1][1] );
			angles.x = 0.0f;
		}
	}

	Vector4D	GetQuaternion( void );
	void	GetQuaternion( Vector4D &quat ) { quat = GetQuaternion(); }

	// Transpose.
	matrix3x3	Transpose() const
	{
		return matrix3x3(
			mat[0][0], mat[1][0], mat[2][0],
			mat[0][1], mat[1][1], mat[2][1],
			mat[0][2], mat[1][2], mat[2][2] );
	}

	Vector VectorRotate( const Vector &v ) const;
	Vector VectorIRotate( const Vector &v ) const;

	// copy as OpenGl matrix
	inline void CopyToArray( float *rgfl ) const
	{
		rgfl[ 0] = mat[0][0];
		rgfl[ 1] = mat[0][1];
		rgfl[ 2] = mat[0][2];
		rgfl[ 3] = 0.0f;
		rgfl[ 4] = mat[1][0];
		rgfl[ 5] = mat[1][1];
		rgfl[ 6] = mat[1][2];
		rgfl[ 7] = 0.0f;
		rgfl[ 8] = mat[2][0];
		rgfl[ 9] = mat[2][1];
		rgfl[10] = mat[2][2];
		rgfl[11] = 0.0f;
		rgfl[12] = 0.0f;
		rgfl[13] = 0.0f;
		rgfl[14] = 0.0f;
		rgfl[15] = 1.0f;
	}

	matrix3x3 Concat( const matrix3x3 mat2 );

	Vector mat[3];
};

class matrix3x4
{
public:
	matrix3x4();
	matrix3x4( float m00, float m01, float m02,
		 float m10, float m11, float m12,
		 float m20, float m21, float m22,
		 float m30, float m31, float m32 )
	{
		mat[0][0] = m00;
		mat[0][1] = m01;
		mat[0][2] = m02;
		mat[1][0] = m10;
		mat[1][1] = m11;
		mat[1][2] = m12;
		mat[2][0] = m20;
		mat[2][1] = m21;
		mat[2][2] = m22;
		mat[3][0] = m30;
		mat[3][1] = m31;
		mat[3][2] = m32;
	}

	// init from entity
	_forceinline matrix3x4( const Vector &origin, const Vector &angles, const Vector &scale )
	{
		float	angle, sr, sp, sy, cr, cp, cy;

		if( angles[ROLL] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );
			angle = angles[ROLL] * (M_PI*2 / 360);
			SinCos( angle, &sr, &cr );

			mat[0][0] = (cp*cy) * scale.x;
			mat[1][0] = (sr*sp*cy+cr*-sy) * scale.y;
			mat[2][0] = (cr*sp*cy+-sr*-sy) * scale.z;
			mat[3][0] = origin.x;
			mat[0][1] = (cp*sy) * scale.x;
			mat[1][1] = (sr*sp*sy+cr*cy) * scale.y;
			mat[2][1] = (cr*sp*sy+-sr*cy) * scale.z;
			mat[3][1] = origin.y;
			mat[0][2] = (-sp) * scale.x;
			mat[1][2] = (sr*cp) * scale.y;
			mat[2][2] = (cr*cp) * scale.z;
			mat[3][2] = origin.z;
		}
		else if( angles[PITCH] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );

			mat[0][0] = (cp*cy) * scale.x;
			mat[1][0] = (-sy) * scale.y;
			mat[2][0] = (sp*cy) * scale.z;
			mat[3][0] = origin.x;
			mat[0][1] = (cp*sy) * scale.x;
			mat[1][1] = (cy) * scale.y;
			mat[2][1] = (sp*sy) * scale.z;
			mat[3][1] = origin.y;
			mat[0][2] = (-sp) * scale.x;
			mat[1][2] = 0;
			mat[2][2] = (cp) * scale.z;
			mat[3][2] = origin.z;
		}
		else if( angles[YAW] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );

			mat[0][0] = (cy) * scale.x;
			mat[1][0] = (-sy) * scale.y;
			mat[2][0] = 0;
			mat[3][0] = origin.x;
			mat[0][1] = (sy) * scale.x;
			mat[1][1] = (cy) * scale.y;
			mat[2][1] = 0;
			mat[3][1] = origin.y;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = scale.z;
			mat[3][2] = origin.z;
		}
		else
		{
			mat[0][0] = scale.x;
			mat[1][0] = 0;
			mat[2][0] = 0;
			mat[3][0] = origin.x;
			mat[0][1] = 0;
			mat[1][1] = scale.y;
			mat[2][1] = 0;
			mat[3][1] = origin.y;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = scale.z;
			mat[3][2] = origin.z;
		}
	}

	_forceinline matrix3x4( const Vector &origin, const Vector &angles, float scale = 1.0f )
	{
		float	angle, sr, sp, sy, cr, cp, cy;

		if( angles[ROLL] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );
			angle = angles[ROLL] * (M_PI*2 / 360);
			SinCos( angle, &sr, &cr );

			mat[0][0] = (cp*cy) * scale;
			mat[1][0] = (sr*sp*cy+cr*-sy) * scale;
			mat[2][0] = (cr*sp*cy+-sr*-sy) * scale;
			mat[3][0] = origin.x;
			mat[0][1] = (cp*sy) * scale;
			mat[1][1] = (sr*sp*sy+cr*cy) * scale;
			mat[2][1] = (cr*sp*sy+-sr*cy) * scale;
			mat[3][1] = origin.y;
			mat[0][2] = (-sp) * scale;
			mat[1][2] = (sr*cp) * scale;
			mat[2][2] = (cr*cp) * scale;
			mat[3][2] = origin.z;
		}
		else if( angles[PITCH] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );

			mat[0][0] = (cp*cy) * scale;
			mat[1][0] = (-sy) * scale;
			mat[2][0] = (sp*cy) * scale;
			mat[3][0] = origin.x;
			mat[0][1] = (cp*sy) * scale;
			mat[1][1] = (cy) * scale;
			mat[2][1] = (sp*sy) * scale;
			mat[3][1] = origin.y;
			mat[0][2] = (-sp) * scale;
			mat[1][2] = 0;
			mat[2][2] = (cp) * scale;
			mat[3][2] = origin.z;
		}
		else if( angles[YAW] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );

			mat[0][0] = (cy) * scale;
			mat[1][0] = (-sy) * scale;
			mat[2][0] = 0;
			mat[3][0] = origin.x;
			mat[0][1] = (sy) * scale;
			mat[1][1] = (cy) * scale;
			mat[2][1] = 0;
			mat[3][1] = origin.y;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = scale;
			mat[3][2] = origin.z;
		}
		else
		{
			mat[0][0] = scale;
			mat[1][0] = 0;
			mat[2][0] = 0;
			mat[3][0] = origin.x;
			mat[0][1] = 0;
			mat[1][1] = scale;
			mat[2][1] = 0;
			mat[3][1] = origin.y;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = scale;
			mat[3][2] = origin.z;
		}
	}

	_forceinline matrix3x4( const Vector &origin, const Radian &angles )
	{
		float	angle, sr, sp, sy, cr, cp, cy;

		angle = angles[ROLL];	// YAW -> ROLL
		SinCos( angle, &sy, &cy );
		angle = angles[YAW];	// PITCH -> YAW
		SinCos( angle, &sp, &cp );
		angle = angles[PITCH];	// ROLL -> PITCH
		SinCos( angle, &sr, &cr );

		mat[0][0] = (cp*cy);
		mat[0][1] = (cp*sy);
		mat[0][2] = (-sp);
		mat[1][0] = (sr*sp*cy+cr*-sy);
		mat[1][1] = (sr*sp*sy+cr*cy);
		mat[1][2] = (sr*cp);
		mat[2][0] = (cr*sp*cy+-sr*-sy);
		mat[2][1] = (cr*sp*sy+-sr*cy);
		mat[2][2] = (cr*cp);
		mat[3][0] = origin.x;
		mat[3][1] = origin.y;
		mat[3][2] = origin.z;
	}

	// init from quaternion + origin
	_forceinline matrix3x4( const Vector &origin, const Vector4D &quaternion )
	{
		mat[0][0] = 1.0f - 2.0f * (quaternion.y * quaternion.y + quaternion.z * quaternion.z);
		mat[1][0] = 2.0f * (quaternion.x * quaternion.y - quaternion.z * quaternion.w);
		mat[2][0] = 2.0f * (quaternion.x * quaternion.z + quaternion.y * quaternion.w);
		mat[3][0] = origin[0];
		mat[0][1] = 2.0f * (quaternion.x * quaternion.y + quaternion.z * quaternion.w);
		mat[1][1] = 1.0f - 2.0f * (quaternion.x * quaternion.x + quaternion.z * quaternion.z);
		mat[2][1] = 2.0f * (quaternion.y * quaternion.z - quaternion.x * quaternion.w);
		mat[3][1] = origin[1];
		mat[0][2] = 2.0f * (quaternion.x * quaternion.z - quaternion.y * quaternion.w);
		mat[1][2] = 2.0f * (quaternion.y * quaternion.z + quaternion.x * quaternion.w);
		mat[2][2] = 1.0f - 2.0f * (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
		mat[3][2] = origin[2];
	}

	// init from rotational matrix
	_forceinline matrix3x4( const matrix3x3 &in )
	{
		mat[0][0] = in[0][0];
		mat[0][1] = in[0][1];
		mat[0][2] = in[0][2];
		mat[1][0] = in[1][0];
		mat[1][1] = in[1][1];
		mat[1][2] = in[1][2];
		mat[2][0] = in[2][0];
		mat[2][1] = in[2][1];
		mat[2][2] = in[2][2];
		mat[3][0] = 0;
		mat[3][1] = 0;
		mat[3][2] = 0;
	}

	void Identity();

	// array access
	float* operator[]( int i ) { return mat[i]; }
	float const* operator[]( int i ) const { return mat[i]; }
	operator float *() { return (float *)&mat[0][0]; }
	operator const float *() const { return (float *)&mat[0][0]; } 

	_forceinline int operator == ( const matrix3x4& mat2 ) const
	{
		if( mat[0][0] != mat2[0][0] || mat[0][1] != mat2[0][1] || mat[0][2] != mat2[0][2] )
			return false;
		if( mat[1][0] != mat2[1][0] || mat[1][1] != mat2[1][1] || mat[1][2] != mat2[1][2] )
			return false;
		if( mat[2][0] != mat2[2][0] || mat[2][1] != mat2[2][1] || mat[2][2] != mat2[2][2] )
			return false;
		if( mat[3][0] != mat2[3][0] || mat[3][1] != mat2[3][1] || mat[3][2] != mat2[3][2] )
			return false;
		return true;
	}

	_forceinline int operator != ( const matrix3x4& mat2 ) const { return !(*this == mat2 ); }
	matrix3x4& operator=( const matrix3x3 &vOther );
	matrix3x4& operator=( const matrix4x4 &vOther );

	// Access the basis vectors.
	Vector	GetForward() const { return mat[0]; };
	Vector	GetRight() const { return mat[1]; };
	Vector	GetUp() const { return mat[2]; };
	Vector	GetRow( int i ) const { return mat[i]; }
	Vector	GetOrigin() const { return mat[3]; };

	void	SetForward( const Vector &vForward ) { mat[0] = vForward; };
	void	SetRight( const Vector &vRight ) { mat[1] = vRight; };
	void	SetUp( const Vector &vUp ) { mat[2] = vUp; };

	void	SetOrigin( const Vector &vOrigin ) { mat[3] = vOrigin; };
	void	GetOrigin( Vector &vOrigin ) { vOrigin = mat[3]; };

	Vector	GetAngles( void )
	{
		float xyDist = sqrt( mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1] );
		Vector angles;

		// enough here to get angles?
		if( xyDist > 0.001f )
		{
			angles[0] = RAD2DEG( atan2( -mat[0][2], xyDist ) );
			angles[1] = RAD2DEG( atan2( mat[0][1], mat[0][0] ) );
			angles[2] = RAD2DEG( atan2( mat[1][2], mat[2][2] ) );
		}
		else	// forward is mostly Z, gimbal lock
		{
			angles[0] = RAD2DEG( atan2( -mat[0][2], xyDist ) );
			angles[1] = RAD2DEG( atan2( -mat[1][0], mat[1][1] ) );
			angles[2] = 0.0f;
		}

		return angles;
	}

	void GetStudioTransform( Vector &position, Radian &angles )
	{
		float xyDist = sqrt( mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1] );

		// enough here to get angles?
		if( xyDist > 0.001f )
		{
			angles.y = atan2( -mat[0][2], xyDist );
			angles.z = atan2( mat[0][1], mat[0][0] );
			angles.x = atan2( mat[1][2], mat[2][2] );
		}
		else	// forward is mostly Z, gimbal lock
		{
			angles.y = atan2( -mat[0][2], xyDist );
			angles.z = atan2( -mat[1][0], mat[1][1] );
			angles.x = 0.0f;
		}

		position = mat[3];
	}

	void	GetAngles( Vector &angles ) { angles = GetAngles(); }

	void	GetAngles( Radian &angles )
	{
		float xyDist = sqrt( mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1] );

		// enough here to get angles?
		if( xyDist > 0.001f )
		{
			angles.y = atan2( -mat[0][2], xyDist );
			angles.z = atan2( mat[0][1], mat[0][0] );
			angles.x = atan2( mat[1][2], mat[2][2] );
		}
		else	// forward is mostly Z, gimbal lock
		{
			angles.y = atan2( -mat[0][2], xyDist );
			angles.z = atan2( -mat[1][0], mat[1][1] );
			angles.x = 0.0f;
		}
	}

	Vector4D	GetQuaternion( void );
	void	GetQuaternion( Vector4D &quat ) { quat = GetQuaternion(); }

	// transform point and normal
	Vector	VectorTransform( const Vector &v ) const;
	Vector	VectorITransform( const Vector &v ) const;
	Vector	VectorRotate( const Vector &v ) const;
	Vector	VectorIRotate( const Vector &v ) const;

	// copy as OpenGl matrix
	inline void CopyToArray( float *rgfl ) const
	{
		rgfl[ 0] = mat[0][0];
		rgfl[ 1] = mat[0][1];
		rgfl[ 2] = mat[0][2];
		rgfl[ 3] = 0.0f;
		rgfl[ 4] = mat[1][0];
		rgfl[ 5] = mat[1][1];
		rgfl[ 6] = mat[1][2];
		rgfl[ 7] = 0.0f;
		rgfl[ 8] = mat[2][0];
		rgfl[ 9] = mat[2][1];
		rgfl[10] = mat[2][2];
		rgfl[11] = 0.0f;
		rgfl[12] = mat[3][0];
		rgfl[13] = mat[3][1];
		rgfl[14] = mat[3][2];
		rgfl[15] = 1.0f;
	}

	// copy as Vector4D array
	inline void CopyToArray4x3( Vector4D array[] ) const
	{
		array[0] = Vector4D( mat[0][0], mat[0][1], mat[0][2], mat[3][0] );
		array[1] = Vector4D( mat[1][0], mat[1][1], mat[1][2], mat[3][1] );
		array[2] = Vector4D( mat[2][0], mat[2][1], mat[2][2], mat[3][2] );
	}

	// Transpose.
	matrix3x4	Transpose() const
	{
		return matrix3x4(  // transpose 3x3, position is unchanged
			mat[0][0], mat[1][0], mat[2][0],
			mat[0][1], mat[1][1], mat[2][1],
			mat[0][2], mat[1][2], mat[2][2],
			mat[3][0], mat[3][1], mat[3][2] );
	}

	matrix3x4 Invert( void ) const;	// basic orthonormal invert
	matrix3x4 ConcatTransforms( const matrix3x4 mat2 );
	matrix3x4 ConcatTransforms( const matrix3x4 mat2 ) const;

	Vector mat[4];
};

class matrix4x4
{
public:
	matrix4x4();
	matrix4x4( float m00, float m01, float m02, float m03,
		 float m10, float m11, float m12, float m13,
		 float m20, float m21, float m22, float m23,
		 float m30, float m31, float m32, float m33 )
	{
		mat[0][0] = m00;
		mat[0][1] = m01;
		mat[0][2] = m02;
		mat[0][3] = m03;
		mat[1][0] = m10;
		mat[1][1] = m11;
		mat[1][2] = m12;
		mat[1][3] = m13;
		mat[2][0] = m20;
		mat[2][1] = m21;
		mat[2][2] = m22;
		mat[2][3] = m23;
		mat[3][0] = m30;
		mat[3][1] = m31;
		mat[3][2] = m32;
		mat[3][3] = m33;
	}

	// init from OpenGl matrix
	matrix4x4( const float *opengl_matrix )
	{
		mat[0][0] = opengl_matrix[0];
		mat[0][1] = opengl_matrix[1];
		mat[0][2] = opengl_matrix[2];
		mat[0][3] = opengl_matrix[3];
		mat[1][0] = opengl_matrix[4];
		mat[1][1] = opengl_matrix[5];
		mat[1][2] = opengl_matrix[6];
		mat[1][3] = opengl_matrix[7];
		mat[2][0] = opengl_matrix[8];
		mat[2][1] = opengl_matrix[9];
		mat[2][2] = opengl_matrix[10];
		mat[2][3] = opengl_matrix[11];
		mat[3][0] = opengl_matrix[12];
		mat[3][1] = opengl_matrix[13];
		mat[3][2] = opengl_matrix[14];
		mat[3][3] = opengl_matrix[15];
	}

	// init from entity
	_forceinline matrix4x4( const Vector &origin, const Vector &angles, float scale = 1.0f )
	{
		float	angle, sr, sp, sy, cr, cp, cy;

		if( angles[ROLL] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );
			angle = angles[ROLL] * (M_PI*2 / 360);
			SinCos( angle, &sr, &cr );

			mat[0][0] = (cp*cy) * scale;
			mat[1][0] = (sr*sp*cy+cr*-sy) * scale;
			mat[2][0] = (cr*sp*cy+-sr*-sy) * scale;
			mat[3][0] = origin.x;
			mat[0][1] = (cp*sy) * scale;
			mat[1][1] = (sr*sp*sy+cr*cy) * scale;
			mat[2][1] = (cr*sp*sy+-sr*cy) * scale;
			mat[3][1] = origin.y;
			mat[0][2] = (-sp) * scale;
			mat[1][2] = (sr*cp) * scale;
			mat[2][2] = (cr*cp) * scale;
			mat[3][2] = origin.z;
			mat[0][3] = 0;
			mat[1][3] = 0;
			mat[2][3] = 0;
			mat[3][3] = 1;
		}
		else if( angles[PITCH] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );

			mat[0][0] = (cp*cy) * scale;
			mat[1][0] = (-sy) * scale;
			mat[2][0] = (sp*cy) * scale;
			mat[3][0] = origin.x;
			mat[0][1] = (cp*sy) * scale;
			mat[1][1] = (cy) * scale;
			mat[2][1] = (sp*sy) * scale;
			mat[3][1] = origin.y;
			mat[0][2] = (-sp) * scale;
			mat[1][2] = 0;
			mat[2][2] = (cp) * scale;
			mat[3][2] = origin.z;
			mat[0][3] = 0;
			mat[1][3] = 0;
			mat[2][3] = 0;
			mat[3][3] = 1;
		}
		else if( angles[YAW] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );

			mat[0][0] = (cy) * scale;
			mat[1][0] = (-sy) * scale;
			mat[2][0] = 0;
			mat[3][0] = origin.x;
			mat[0][1] = (sy) * scale;
			mat[1][1] = (cy) * scale;
			mat[2][1] = 0;
			mat[3][1] = origin.y;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = scale;
			mat[3][2] = origin.z;
			mat[0][3] = 0;
			mat[1][3] = 0;
			mat[2][3] = 0;
			mat[3][3] = 1;
		}
		else
		{
			mat[0][0] = scale;
			mat[1][0] = 0;
			mat[2][0] = 0;
			mat[3][0] = origin.x;
			mat[0][1] = 0;
			mat[1][1] = scale;
			mat[2][1] = 0;
			mat[3][1] = origin.y;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = scale;
			mat[3][2] = origin.z;
			mat[0][3] = 0;
			mat[1][3] = 0;
			mat[2][3] = 0;
			mat[3][3] = 1;
		}
	}

	// init from entity
	_forceinline matrix4x4( const Vector &origin, const Vector &angles, const Vector &scale )
	{
		float	angle, sr, sp, sy, cr, cp, cy;

		if( angles[ROLL] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );
			angle = angles[ROLL] * (M_PI*2 / 360);
			SinCos( angle, &sr, &cr );

			mat[0][0] = (cp*cy) * scale.x;
			mat[1][0] = (sr*sp*cy+cr*-sy) * scale.y;
			mat[2][0] = (cr*sp*cy+-sr*-sy) * scale.z;
			mat[3][0] = origin.x;
			mat[0][1] = (cp*sy) * scale.x;
			mat[1][1] = (sr*sp*sy+cr*cy) * scale.y;
			mat[2][1] = (cr*sp*sy+-sr*cy) * scale.z;
			mat[3][1] = origin.y;
			mat[0][2] = (-sp) * scale.x;
			mat[1][2] = (sr*cp) * scale.y;
			mat[2][2] = (cr*cp) * scale.z;
			mat[3][2] = origin.z;
			mat[0][3] = 0;
			mat[1][3] = 0;
			mat[2][3] = 0;
			mat[3][3] = 1;
		}
		else if( angles[PITCH] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );
			angle = angles[PITCH] * (M_PI*2 / 360);
			SinCos( angle, &sp, &cp );

			mat[0][0] = (cp*cy) * scale.x;
			mat[1][0] = (-sy) * scale.y;
			mat[2][0] = (sp*cy) * scale.z;
			mat[3][0] = origin.x;
			mat[0][1] = (cp*sy) * scale.x;
			mat[1][1] = (cy) * scale.y;
			mat[2][1] = (sp*sy) * scale.z;
			mat[3][1] = origin.y;
			mat[0][2] = (-sp) * scale.x;
			mat[1][2] = 0;
			mat[2][2] = (cp) * scale.z;
			mat[3][2] = origin.z;
			mat[0][3] = 0;
			mat[1][3] = 0;
			mat[2][3] = 0;
			mat[3][3] = 1;
		}
		else if( angles[YAW] )
		{
			angle = angles[YAW] * (M_PI*2 / 360);
			SinCos( angle, &sy, &cy );

			mat[0][0] = (cy) * scale.x;
			mat[1][0] = (-sy) * scale.y;
			mat[2][0] = 0;
			mat[3][0] = origin.x;
			mat[0][1] = (sy) * scale.x;
			mat[1][1] = (cy) * scale.y;
			mat[2][1] = 0;
			mat[3][1] = origin.y;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = scale.z;
			mat[3][2] = origin.z;
			mat[0][3] = 0;
			mat[1][3] = 0;
			mat[2][3] = 0;
			mat[3][3] = 1;
		}
		else
		{
			mat[0][0] = scale.x;
			mat[1][0] = 0;
			mat[2][0] = 0;
			mat[3][0] = origin.x;
			mat[0][1] = 0;
			mat[1][1] = scale.y;
			mat[2][1] = 0;
			mat[3][1] = origin.y;
			mat[0][2] = 0;
			mat[1][2] = 0;
			mat[2][2] = scale.z;
			mat[3][2] = origin.z;
			mat[0][3] = 0;
			mat[1][3] = 0;
			mat[2][3] = 0;
			mat[3][3] = 1;
		}
	}

	// init from quaternion + origin
	_forceinline matrix4x4( const Vector &origin, const Vector4D &quaternion )
	{
		mat[0][0] = 1.0f - 2.0f * (quaternion.y * quaternion.y + quaternion.z * quaternion.z);
		mat[1][0] = 2.0f * (quaternion.x * quaternion.y - quaternion.z * quaternion.w);
		mat[2][0] = 2.0f * (quaternion.x * quaternion.z + quaternion.y * quaternion.w);
		mat[3][0] = origin[0];
		mat[0][1] = 2.0f * (quaternion.x * quaternion.y + quaternion.z * quaternion.w);
		mat[1][1] = 1.0f - 2.0f * (quaternion.x * quaternion.x + quaternion.z * quaternion.z);
		mat[2][1] = 2.0f * (quaternion.y * quaternion.z - quaternion.x * quaternion.w);
		mat[3][1] = origin[1];
		mat[0][2] = 2.0f * (quaternion.x * quaternion.z - quaternion.y * quaternion.w);
		mat[1][2] = 2.0f * (quaternion.y * quaternion.z + quaternion.x * quaternion.w);
		mat[2][2] = 1.0f - 2.0f * (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
		mat[3][2] = origin[2];
		mat[0][3] = 0;
		mat[1][3] = 0;
		mat[2][3] = 0;
		mat[3][3] = 1;
	}

	// init from matrix3x4
	_forceinline matrix4x4( const matrix3x4 &in )
	{
		mat[0][0] = in[0][0];
		mat[0][1] = in[0][1];
		mat[0][2] = in[0][2];
		mat[1][0] = in[1][0];
		mat[1][1] = in[1][1];
		mat[1][2] = in[1][2];
		mat[2][0] = in[2][0];
		mat[2][1] = in[2][1];
		mat[2][2] = in[2][2];
		mat[3][0] = in[3][0];
		mat[3][1] = in[3][1];
		mat[3][2] = in[3][2];
		mat[0][3] = 0;
		mat[1][3] = 0;
		mat[2][3] = 0;
		mat[3][3] = 1;
	}

	void Identity();

	// creates some non-identity states
	void CreateModelview( void );
	void CreateTexture( void );
	void Crop(const Vector &mins, const Vector &maxs);
	void LookAt(const Vector &eye, const Vector &dir, const Vector &up); // like gluLookAt
	void CreateProjection( float fov_x, float fov_y, float zNear, float zFar );
	void CreateProjection( float xMax, float xMin, float yMax, float yMin, float zNear, float zFar );
	void CreateOrtho( float xLeft, float xRight, float yBottom, float yTop, float zNear, float zFar );
	void CreateOrthoRH(float xLeft, float xRight, float yBottom, float yTop, float zNear, float zFar);
	void CreateTranslate( float x, float y, float z );
	void CreateRotate( float angle, float x, float y, float z );
	void CreateScale( float scale );
	void CreateScale( float x, float y, float z );
	
	// array access
	float* operator[]( int i ) { return mat[i]; }
	float const* operator[]( int i ) const { return mat[i]; }
	operator float *() { return (float *)&mat[0][0]; }
	operator const float *() const { return (float *)&mat[0][0]; } 

	_forceinline int operator == ( const matrix4x4& mat2 ) const
	{
		if( mat[0][0] != mat2[0][0] || mat[0][1] != mat2[0][1] || mat[0][2] != mat2[0][2] || mat[0][3] != mat2[0][3] )
			return false;
		if( mat[1][0] != mat2[1][0] || mat[1][1] != mat2[1][1] || mat[1][2] != mat2[1][2] || mat[1][3] != mat2[1][3] )
			return false;
		if( mat[2][0] != mat2[2][0] || mat[2][1] != mat2[2][1] || mat[2][2] != mat2[2][2] || mat[2][3] != mat2[2][3] )
			return false;
		if( mat[3][0] != mat2[3][0] || mat[3][1] != mat2[3][1] || mat[3][2] != mat2[3][2] || mat[3][3] != mat2[3][3] )
			return false;
		return true;
	}

	_forceinline int operator != ( const matrix4x4& mat2 ) const { return !(*this == mat2 ); }
	matrix4x4& operator=( const matrix3x3 &vOther );
	matrix4x4& operator=( const matrix3x4 &vOther );
	matrix4x4& operator=( const matrix4x4 &vOther );

	// Access the basis vectors.
	Vector	GetForward() const { return mat[0]; };
	Vector	GetRight() const { return mat[1]; };
	Vector	GetUp() const { return mat[2]; };
	Vector	GetRow( int i ) const { return mat[i]; }
	Vector	GetOrigin() const { return mat[3]; };

	void	SetForward( const Vector4D &vForward ) { mat[0] = vForward; };
	void	SetRight( const Vector4D &vRight ) { mat[1] = vRight; };
	void	SetUp( const Vector4D &vUp ) { mat[2] = vUp; };
	void	SetForward( const Vector &vForward ) { mat[0] = Vector4D( vForward.x, vForward.y, vForward.z, 1.0f ); };
	void	SetRight( const Vector &vRight ) { mat[1] = Vector4D( vRight.x, vRight.y, vRight.z, 1.0f ); };
	void	SetUp( const Vector &vUp ) { mat[2] = Vector4D( vUp.x, vUp.y, vUp.z, 1.0f ); };

	void	SetOrigin( const Vector &vOrigin ) { mat[3] = Vector4D( vOrigin.x, vOrigin.y, vOrigin.z, 1.0f ); };
	void	GetOrigin( Vector &vOrigin ) { vOrigin = mat[3]; };

	Vector	GetAngles( void )
	{
		float xyDist = sqrt( mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1] );
		Vector angles;

		// enough here to get angles?
		if( xyDist > 0.001f )
		{
			angles[0] = RAD2DEG( atan2( -mat[0][2], xyDist ) );
			angles[1] = RAD2DEG( atan2( mat[0][1], mat[0][0] ) );
			angles[2] = RAD2DEG( atan2( mat[1][2], mat[2][2] ) );
		}
		else	// forward is mostly Z, gimbal lock
		{
			angles[0] = RAD2DEG( atan2( -mat[0][2], xyDist ) );
			angles[1] = RAD2DEG( atan2( -mat[1][0], mat[1][1] ) );
			angles[2] = 0;
		}

		return angles;
	}

	void	GetAngles( Vector &angles ) { angles = GetAngles(); }

	void GetStudioTransform( Vector &position, Radian &angles )
	{
		float xyDist = sqrt( mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1] );

		// enough here to get angles?
		if( xyDist > 0.001f )
		{
			angles.y = atan2( -mat[0][2], xyDist );
			angles.z = atan2( mat[0][1], mat[0][0] );
			angles.x = atan2( mat[1][2], mat[2][2] );
		}
		else	// forward is mostly Z, gimbal lock
		{
			angles.y = atan2( -mat[0][2], xyDist );
			angles.z = atan2( -mat[1][0], mat[1][1] );
			angles.x = 0.0f;
		}

		position = mat[3];
	}

	Vector4D	GetQuaternion( void );
	void	GetQuaternion( Vector4D &quat ) { quat = GetQuaternion(); }

	// transform point and normal
	Vector	VectorTransform( const Vector &v ) const;
	Vector4D	VectorTransform( const Vector4D &v ) const;
	Vector	VectorITransform( const Vector &v ) const;
	Vector	VectorRotate( const Vector &v ) const;
	Vector	VectorIRotate( const Vector &v ) const;

	// transform plane
	void	TransformPositivePlane( const struct mplane_s &in, struct mplane_s &out );
	void	TransformPositivePlane( const struct plane_s &in, struct plane_s &out );
	void	TransformStandardPlane( const struct mplane_s &in, struct mplane_s &out );
	void	TransformStandardPlane( const struct plane_s &in, struct plane_s &out );

	// copy as OpenGl matrix
	inline void CopyToArray( float *rgfl ) const
	{
		rgfl[ 0] = mat[0][0];
		rgfl[ 1] = mat[0][1];
		rgfl[ 2] = mat[0][2];
		rgfl[ 3] = mat[0][3];
		rgfl[ 4] = mat[1][0];
		rgfl[ 5] = mat[1][1];
		rgfl[ 6] = mat[1][2];
		rgfl[ 7] = mat[1][3];
		rgfl[ 8] = mat[2][0];
		rgfl[ 9] = mat[2][1];
		rgfl[10] = mat[2][2];
		rgfl[11] = mat[2][3];
		rgfl[12] = mat[3][0];
		rgfl[13] = mat[3][1];
		rgfl[14] = mat[3][2];
		rgfl[15] = mat[3][3];
	}

	// Transpose.
	matrix4x4	Transpose() const
	{
		return matrix4x4(
			mat[0][0], mat[1][0], mat[2][0], mat[3][0],
			mat[0][1], mat[1][1], mat[2][1], mat[3][1],
			mat[0][2], mat[1][2], mat[2][2], mat[3][2],
			mat[0][3], mat[1][3], mat[2][3], mat[3][3] );
	}

	matrix4x4 Invert( void ) const;	// basic orthonormal invert
	matrix4x4 InvertFull( void ) const;	// full invert
	matrix4x4 ConcatTransforms( const matrix4x4 mat2 ) const;
	matrix4x4 Concat( const matrix4x4 mat2 ) const;

	matrix4x4 QuakeToNewton( void ) const;
	matrix4x4 NewtonToQuake( void ) const;

	void ConcatTranslate( float x, float y, float z )
	{
		matrix4x4 temp;
		temp.CreateTranslate( x, y, z );
		*this = Concat( temp );
	}

	void ConcatRotate( float angle, float x, float y, float z )
	{
		matrix4x4 temp;
		temp.CreateRotate( angle, x, y, z );
		*this = Concat( temp );
	}

	void ConcatScale( float scale )
	{
		matrix4x4 temp;
		temp.CreateScale( scale );
		*this = Concat( temp );
	}

	void ConcatScale( float x, float y, float z )
	{
		matrix4x4 temp;
		temp.CreateScale( x, y, z );
		*this = Concat( temp );
	}

	Vector4D mat[4];
};

#endif//MATRIX_H
