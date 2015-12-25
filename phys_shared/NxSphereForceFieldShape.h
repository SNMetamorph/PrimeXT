#ifndef NX_PHYSICS_NXSPHEREFORCEFIELDSHAPE
#define NX_PHYSICS_NXSPHEREFORCEFIELDSHAPE
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
#include "NxForceFieldShape.h"

class NxSphereForceFieldShapeDesc;

/**
 \brief A spherical force field shape
 

<b>Platform:</b>
\li PC SW: Yes
\li GPU  : Yes [SW]
\li PS3  : Yes
\li XB360: Yes
\li WII	 : Yes

 @see NxForceFieldShape
*/
class NxSphereForceFieldShape : public NxForceFieldShape
{
	public:
	/**
	\brief Sets the sphere radius.

	Call this to initialize or alter the sphere. 

	\param[in] radius The new radius for the sphere. <b>Range:</b> (0,inf)

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes
	*/
	virtual void setRadius(NxReal radius) = 0;

	/**
	\brief Retrieves the radius of the sphere.

	\return The radius of the sphere.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes
	*/

	virtual NxReal getRadius()					const = 0;

	/**
	\brief Saves the state of the shape object to a descriptor.

	\param[out] desc Descriptor to save to.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see NxSphereForceFieldShapeDesc
	*/
	virtual	void saveToDesc(NxSphereForceFieldShapeDesc& desc) const=0;
};

/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

