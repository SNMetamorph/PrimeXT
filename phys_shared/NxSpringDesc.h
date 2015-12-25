#ifndef NX_PHYSICS_NXSPRINGDESC
#define NX_PHYSICS_NXSPRINGDESC
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
\brief Describes a joint spring.

The spring is implicitly integrated, so even high spring and damper coefficients should be robust.

<h3>Example</h3>

\include NxSpringDesc_NxJointLimitDesc_Example.cpp

@see NxDistanceJoint NxRevoluteJoint NxSphericalJoint NxWheelShape NxMaterial NxMaterialDesc
*/
class NxSpringDesc
	{
	public:
	
	/**
	\brief spring coefficient

	<b>Range:</b> [0,inf)<br>
	<b>Default:</b> 0

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes
	*/
	NxReal spring;
	
	/**
	\brief damper coefficient

	<b>Range:</b> [0,inf)<br>
	<b>Default:</b> 0

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes
	*/
	NxReal damper;
	
	/**
	\brief target value (angle/position) of spring where the spring force is zero.

	<b>Range:</b> Angular: (-PI,PI]<br>
	<b>Range:</b> Positional: (-inf,inf)<br>
	<b>Default:</b> 0

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes
	*/
	NxReal targetValue;

	/** 
	\brief Initializes the NxSpringDesc with default parameters.
	*/
	NX_INLINE NxSpringDesc();

	/**
	\brief Initializes a NxSpringDesc with the given parameters.

	\param[in] spring Spring Coefficient. <b>Range:</b> (-inf,inf)
	\param[in] damper Damper Coefficient. <b>Range:</b>  [0,inf)
	\param[in] targetValue Target value (angle/position) of spring where the spring force is zero. 
	<b>Range:</b> Angular: (-PI,PI] Positional: (-inf,inf)

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes
	*/
	NX_INLINE NxSpringDesc(NxReal spring, NxReal damper = 0, NxReal targetValue = 0);

	/**
	\brief (re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault();

	/**
	\brief Returns true if the descriptor is valid.

	\return True if the current settings are valid.
	*/
	NX_INLINE bool isValid() const;
	};

NX_INLINE NxSpringDesc::NxSpringDesc()
	{
	setToDefault();
	}

NX_INLINE NxSpringDesc::NxSpringDesc(NxReal s, NxReal d, NxReal t)
	{
	spring = s;
	damper = d;
	targetValue = t;
	}

NX_INLINE void NxSpringDesc::setToDefault()
	{
	spring = 0;
	damper = 0;
	targetValue = 0;
	}

NX_INLINE bool NxSpringDesc::isValid() const
	{
	return (spring >= 0 && damper >= 0);
	}

/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

