/*
randomrange.h - simple class of implementation random values
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef RANDOMRANGE_H
#define RANDOMRANGE_H

#include <vector.h>
#include <stringlib.h>

class RandomRange
{
public:
	RandomRange() { m_flMin = m_flMax = 0; m_bDefined = false; }
	RandomRange( float fValue ) { m_flMin = m_flMax = fValue; m_bDefined = true; }
	RandomRange( float fMin, float fMax) { m_flMin = fMin; m_flMax = fMax; m_bDefined = true; }
	RandomRange( char *szToken )
	{
		char *cOneDot = NULL;
		m_bDefined = true;
	
		for( char *c = szToken; *c; c++ )
		{
			if( *c == '.' )
			{
				if( cOneDot != NULL )
				{
					// found two dots in a row - it's a range
					*cOneDot = 0; // null terminate the first number
					m_flMin = Q_atof( szToken ); // parse the first number
					*cOneDot = '.'; // change it back, just in case
					c++;
					m_flMax = Q_atof( c ); // parse the second number
					return;
				}
				else cOneDot = c;
			}
			else cOneDot = NULL;
		}

		// no range, just record the number
		m_flMax = m_flMin = Q_atof( szToken );
          }

	// a simple implementation of RANDOM_FLOAT for now	
	float Random() { return RANDOM_FLOAT(m_flMin, m_flMax); }

	float GetInstance() { return Random(); }
	float GetOffset( float fBasis ) { return Random() - fBasis; }

	bool IsDefined() { return m_bDefined; } 

	// array access...
	operator float *() { return &m_flMin; }
	operator const float *() const { return &m_flMin; } 

	float m_flMin, m_flMax; // NOTE: should be first so array acess is working correctly
	bool m_bDefined;
};

#endif//RANDOMRANGE_H
