#ifndef NX_PHYSICS_NXCAPSULEFORCEFIELDSHAPE
#define NX_PHYSICS_NXCAPSULEFORCEFIELDSHAPE
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

class NxCapsuleForceFieldShapeDesc;

/**
 \brief A capsule shaped region used to define a force field
 

<b>Platform:</b>
\li PC SW: Yes
\li GPU  : Yes [SW]
\li PS3  : Yes
\li XB360: Yes
\li WII	 : Yes

 @see NxForceFieldShape
*/
class NxCapsuleForceFieldShape : public NxForceFieldShape
	{
	public:
	/**
	\brief Call this to initialize or alter the capsule. 

	\param[in] radius The new radius of the capsule. <b>Range:</b> (0,inf)
	\param[in] height The new height of the capsule. <b>Range:</b> (0,inf)

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see setRadius() setHeight()
	*/
	virtual void setDimensions(NxReal radius, NxReal height) = 0;

	/**
	\brief Alters the radius of the capsule.

	\param[in] radius The new radius of the capsule. <b>Range:</b> (0,inf)

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see setDimensions() NxCapsuleForceFieldShapeDesc.radius getRadius()
	*/
	virtual void setRadius(NxReal radius) = 0;

	/**
	\brief Retrieves the radius of the capsule.

	\return The radius of the capsule.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes
	
	@see setRadius() setDimensions() NxCapsuleForceFieldShapeDesc.radius
	*/
	virtual NxReal getRadius() const = 0;

	/**
	\brief Alters the height of the capsule. 

	\param[in] height The new height of the capsule. <b>Range:</b> (0,inf)

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes
	
	@see getHeight() NxCapsuleForceFieldShapeDesc.height getRadius() setDimensions()
	*/
	virtual void setHeight(NxReal height) = 0;	

	/**
	\brief Retrieves the height of the capsule.

	\return The height of the capsule measured from end to end.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes
	
	@see setHeight() setRadius() NxCapsuleForceFieldShapeDesc.height
	*/
	virtual NxReal getHeight() const = 0;

	/**
	\brief Saves the state of the shape object to a descriptor.

	\param[out] desc Descriptor to save to.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see NxCapsuleForceFieldShapeDesc
	*/
	virtual	void saveToDesc(NxCapsuleForceFieldShapeDesc& desc) const=0;
};



/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

