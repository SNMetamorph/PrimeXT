//===== Copyright © 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: a class for performing cube-mapped spherical sample lookups.
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef CUBEMAP_H
#define CUBEMAP_H

#include "mathlib.h"

template<class T, int RES> struct CCubeMap
{
	T m_Samples[6][RES][RES];
	
public:
	_forceinline void GetCoords( Vector const &vecNormalizedDirection, int &nX, int &nY, int &nFace )
	{
		int nLargest = 0;
		int nAxis0 = 1;
		int nAxis1 = 2;

		// find largest magnitude component
		if( fabs( vecNormalizedDirection[1] ) > fabs( vecNormalizedDirection[0] ))
		{
			nLargest = 1;
			nAxis0 = 0;
			nAxis1 = 2;
		}

		if( fabs( vecNormalizedDirection[2] ) > fabs( vecNormalizedDirection[nLargest] ))
		{
			nLargest = 2;
			nAxis0 = 0;
			nAxis1 = 1;
		}

		float flZ = vecNormalizedDirection[nLargest];

		if( flZ < 0 )
		{
			flZ = - flZ;
			nLargest += 3;
		}

		nFace = nLargest;
		flZ = 1.0 / flZ;

		nX = RemapValClamped( vecNormalizedDirection[nAxis0] * flZ, -1, 1, 0, RES - 1 );
		nY = RemapValClamped( vecNormalizedDirection[nAxis1] * flZ, -1, 1, 0, RES - 1 );
	}

	_forceinline T & GetSample( Vector const &vecNormalizedDirection )
	{
		int nX, nY, nFace;
		GetCoords( vecNormalizedDirection, nX, nY, nFace );
		return m_Samples[nFace][nX][nY];
	}
};

#endif// CUBEMAP_H