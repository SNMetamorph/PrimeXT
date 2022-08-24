#ifndef NX_PHYSICS_NXDISTANCEJOINT
#define NX_PHYSICS_NXDISTANCEJOINT
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
#include "NxJoint.h"

class NxDistanceJointDesc;

/**
 \brief A distance joint maintains a certain distance between two points on two actors.

 \image html distanceJoint.png

 <h3>Creation</h3>

 Example:

 \include NxDistanceJoint_Create.cpp
 
 <h3>Visualizations:</h3>
\li #NX_VISUALIZE_JOINT_LOCAL_AXES
\li #NX_VISUALIZE_JOINT_WORLD_AXES
\li #NX_VISUALIZE_JOINT_LIMITS

 @see NxDistanceJointDesc NxJoint NxScene.createJoint()
*/
class NxDistanceJoint : public NxJoint
	{
	public:
	/**
	\brief Use this for changing a significant number of joint parameters at once.

	Use the set() methods for changing only a single property at once.

	Please note that you can not change the actor pointers using this function, if you do so the joint will be marked as broken and will stop working.
	
	Calling the loadFromDesc() method on a broken joint will result in an error message.

	<b>Sleeping:</b> This call wakes the actor if it is sleeping.

	\param[in] desc The descriptor used to set this objects state.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see saveToDesc() NxDistanceJointDesc
	*/
	virtual void loadFromDesc(const NxDistanceJointDesc& desc) = 0;

	/**
	\brief Writes all of the object's attributes to the desc struct  

	\param[out] desc The descriptor used to retrieve this objects state.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see loadFromDesc() NxDistanceJointDesc
	*/
	virtual void saveToDesc(NxDistanceJointDesc& desc) = 0;
	};

/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

