#ifndef NX_PHYSICS_NXPRISMATICJOINTDESC
#define NX_PHYSICS_NXPRISMATICJOINTDESC
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
#include "NxJointDesc.h"
/**
\brief Desc class for #NxPrismaticJoint.

<b>Platform:</b>
\li PC SW: Yes
\li GPU  : Yes [SW]
\li PS3  : Yes
\li XB360: Yes
\li WII	 : Yes

@see NxJointDesc NxPrismaticJoint NxScene.createJoint
*/
class NxPrismaticJointDesc : public NxJointDesc
	{
	public:
	/**
	\brief Constructor sets to default.
	*/
	NX_INLINE NxPrismaticJointDesc();	
	
	/**
	\brief (re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault();
	
	/**
	\brief Returns true if the descriptor is valid.
	\return true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;

	};

NX_INLINE NxPrismaticJointDesc::NxPrismaticJointDesc() : NxJointDesc(NX_JOINT_PRISMATIC)	//constructor sets to default
	{
	setToDefault();
	}

NX_INLINE void NxPrismaticJointDesc::setToDefault()
	{
	NxJointDesc::setToDefault();
	}

NX_INLINE bool NxPrismaticJointDesc::isValid() const
	{
	return NxJointDesc::isValid();
	}

/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

