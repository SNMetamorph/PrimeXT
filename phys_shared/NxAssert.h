#ifndef NX_FOUNDATION_NXASSERT
#define NX_FOUNDATION_NXASSERT
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


/**
This ANSI assert is included so that NX_ASSERTs can continue to appear in user side
code, where the custom assert in Assert.h would not work.
*/

#include <assert.h>
#ifndef NX_ASSERT
  #ifdef _DEBUG
    #define NX_ASSERT(x) assert(x)
  #else
    #define NX_ASSERT(x) {}
  #endif
#endif

 /** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

