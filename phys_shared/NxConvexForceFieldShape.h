#ifndef NX_PHYSICS_NXCONVEXFORCEFIELDSHAPE
#define NX_PHYSICS_NXCONVEXFORCEFIELDSHAPE
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

class NxConvexForceFieldShapeDesc;

/**
 \brief Convex shaped region used to define force field.
 

<b>Platform:</b>
\li PC SW: Yes
\li GPU  : Yes [SW]
\li PS3  : Yes
\li XB360: Yes
\li WII	 : Yes

 @see NxForceFieldShape
*/
class NxConvexForceFieldShape  : public NxForceFieldShape
{
public:
	/**
	\brief Saves the state of the shape object to a descriptor.

	\param[out] desc Descriptor to save to.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see NxConvexForceFieldShapeDesc
	*/
	virtual	void saveToDesc(NxConvexForceFieldShapeDesc& desc) const=0;
};

/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

