#ifndef NX_PHYSICS_NXCAPSULEFORCEFIELDSHAPEDESC
#define NX_PHYSICS_NXCAPSULEFORCEFIELDSHAPEDESC
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


/**
 \brief A descriptor for NxCapsuleForceFieldShape
 

<b>Platform:</b>
\li PC SW: Yes
\li GPU  : Yes [SW]
\li PS3  : Yes
\li XB360: Yes
\li WII	 : Yes

 @see NxForceFieldShapeDesc NxCapsuleForceFieldShape NxForceField
*/
class NxCapsuleForceFieldShapeDesc : public NxForceFieldShapeDesc
	{
	public:
	/**
	\brief Radius of the capsule's hemispherical ends and its trunk.

	<b>Range:</b> (0,inf)<br>
	<b>Default:</b> 1.0

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see NxCapsuleForceFieldShape.setRadius() NxCapsuleForceFieldShape.setDimensions()
	*/
	NxReal radius;

	/**
	\brief The distance between the two hemispherical ends of the capsule.

	The height is along the capsule's Y axis. 

	<b>Range:</b> (0,inf)<br>
	<b>Default:</b> 1.0

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see NxCapsuleForceFieldShape.setHeight() NxCapsuleForceFieldShape.setDimensions()
	*/
	NxReal height;	//!< height of the capsule


	/**
	\brief constructor sets to default.
	*/
	NX_INLINE NxCapsuleForceFieldShapeDesc();

	/**
	\brief (re)sets the structure to the default.	
	*/
	virtual NX_INLINE void setToDefault();

	/**
	\brief Returns true if the descriptor is valid.

	\return True if the current settings are valid
	*/
	virtual NX_INLINE bool isValid() const;
	};

NX_INLINE NxCapsuleForceFieldShapeDesc::NxCapsuleForceFieldShapeDesc() : NxForceFieldShapeDesc(NX_SHAPE_CAPSULE)
	{
	setToDefault();
	}

NX_INLINE void NxCapsuleForceFieldShapeDesc::setToDefault()
	{
	NxForceFieldShapeDesc::setToDefault();
	radius = 1.0f;
	height = 1.0f;
	}

NX_INLINE bool NxCapsuleForceFieldShapeDesc::isValid() const
	{
	if(!NxMath::isFinite(radius))	return false;
	if(radius<=0.0f)				return false;
	if(!NxMath::isFinite(height))	return false;
	if(height<=0.0f)				return false;
	return NxForceFieldShapeDesc::isValid();
	}

/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

