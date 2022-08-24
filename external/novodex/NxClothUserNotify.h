#ifndef NX_PHYSICS_NXCLOTHUSERNOTIFY
#define NX_PHYSICS_NXCLOTHUSERNOTIFY
/*----------------------------------------------------------------------------*\
|
|					Public Interface to NVIDIA PhysX Technology
|
|							     www.nvidia.com
|
\*----------------------------------------------------------------------------*/
/** \addtogroup physics
@{
*/


#include "Nxp.h"

class NxCloth;

/**
\brief An interface class that the user can implement in order to receive simulation events.

<b>Threading:</b> It is not necessary to make this class thread safe as it will only be called in the context of the
user thread.

See the NxUserNotify documentation for an example of how to use notifications.

@see NxScene.setClothUserNotify() NxScene.getClothUserNotify() NxUserNotify
*/
class NxClothUserNotify
{
	virtual ~NxClothUserNotify(){};
};

/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

