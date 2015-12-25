#ifndef NX_FOUNDATION_NX16F32
#define NX_FOUNDATION_NX16F32
/*----------------------------------------------------------------------------*\
|
|					Public Interface to NVIDIA PhysX Technology
|
|							     www.nvidia.com
|
\*----------------------------------------------------------------------------*/
/** \addtogroup foundation
  @{
*/
//Exclude file from docs
/** \cond */

//the file name of this header is legacy due to pain of renaming file in repository.

#include "Nx.h"

class Nx16Real
	{
	public:
    union 
		{
        struct S
			{
            NxReal        _11, _12, _13, _14;
            NxReal        _21, _22, _23, _24;
            NxReal        _31, _32, _33, _34;
            NxReal        _41, _42, _43, _44;
			} s;
		NxReal m[4][4];
		};
	};

/** \endcond */
 /** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

