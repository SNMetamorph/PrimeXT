/*
iksolver.h - Ken Perlin' inverse kinematic solver
Copyright (C) 2017 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef IKSOLVER_H
#define IKSOLVER_H

class CIKSolver
{
public:
	//------------ GENERAL VECTOR MATH SUPPORT -----------
	static float dot( float const a[], float const b[] ) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
	static float length( float const v[] ) { return sqrt( dot( v, v )); }
	static void normalize( float v[] ) { float norm = length( v ); v[0] /= norm; v[1] /= norm; v[2] /= norm; }
	static float findD( float a, float b, float c ) { return (c + (a * a - b * b) / c) / 2; }
	static float findE( float a, float d ) { return sqrt(a * a - d * d); }

	static void cross( float const a[], float const b[], float c[] )
	{
		c[0] = a[1] * b[2] - a[2] * b[1];
		c[1] = a[2] * b[0] - a[0] * b[2];
		c[2] = a[0] * b[1] - a[1] * b[0];
	}

	static void rot( float const M[3][3], float const src[], float dst[] )
	{
		dst[0] = dot( M[0], src );
		dst[1] = dot( M[1], src );
		dst[2] = dot( M[2], src );
	}

	void defineM( float const P[], float const D[] )
	{
		float *X = Minv[0], *Y = Minv[1], *Z = Minv[2];
		int i;

		for( i = 0; i < 3; i++ )
			X[i] = P[i];
		normalize( X );

		// Its y axis is perpendicular to P, so Y = unit( E - X(E·X) ).
		float dDOTx = dot( D, X );

		for( i = 0; i < 3; i++ )
			Y[i] = D[i] - dDOTx * X[i];
		normalize( Y );

		// Its z axis is perpendicular to both X and Y, so Z = XЧY.
		cross( X, Y, Z );

		// Mfwd = (Minv)T, since transposing inverts a rotation matrix.
		for( i = 0; i < 3; i++ )
		{
			Mfwd[i][0] = Minv[0][i];
			Mfwd[i][1] = Minv[1][i];
			Mfwd[i][2] = Minv[2][i];
		}
	}

	bool solve( float A, float B, float const P[], float const D[], float Q[] )
	{
		float	R[3];

		defineM( P, D );
		rot( Minv, P, R );
		float r = length( R );
		float d = findD( A, B, r );
		float e = findE( A, d );
		float S[3] = { d, e, 0 };
		rot( Mfwd, S, Q );
		return d > (r - B) && d < A;
	}

	float Mfwd[3][3];
	float Minv[3][3];
};

#endif//IKSOLVER_H
