#ifndef NX_PHYSICS_NXJOINTLIMITPAIRDESC
#define NX_PHYSICS_NXJOINTLIMITPAIRDESC
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

#include "NxJointLimitDesc.h"

/**
\brief Describes a pair of joint limits

<h3>Example</h3>

\include NxSpringDesc_NxJointLimitDesc_Example.cpp

<b>Platform:</b>
\li PC SW: Yes
\li GPU  : Yes [SW]
\li PS3  : Yes
\li XB360: Yes
\li WII	 : Yes

@see NxRevoluteJoint NxSphericalJoint NxJointLimitDesc
*/
class NxJointLimitPairDesc
	{
	public:

	/**
	\brief The low limit (smaller value)

	<b>Range:</b> See #NxJointLimitDesc<br>
	<b>Default:</b> See #NxJointLimitDesc

	@see NxJointLimitDesc
	*/
	NxJointLimitDesc low;
	
	/**
	\brief the high limit (larger value)

	<b>Range:</b> See #NxJointLimitDesc<br>
	<b>Default:</b> See #NxJointLimitDesc

	@see NxJointLimitDesc
	*/
	NxJointLimitDesc high;

	/**
	\brief Constructor, sets members to default values.
	*/
	NX_INLINE NxJointLimitPairDesc();

	/** 
	\brief Sets members to default values.
	*/
	NX_INLINE void setToDefault();

	/**
	\brief Returns true if the descriptor is valid.

	\return true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;
	};

NX_INLINE NxJointLimitPairDesc::NxJointLimitPairDesc()
	{
	setToDefault();
	}

NX_INLINE void NxJointLimitPairDesc::setToDefault()
	{
	//nothing
	}

NX_INLINE bool NxJointLimitPairDesc::isValid() const
	{
	return (low.isValid() && high.isValid() && low.value <= high.value);
	}

/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

