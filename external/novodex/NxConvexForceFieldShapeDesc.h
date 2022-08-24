#ifndef NX_PHYSICS_NXCONVEXFORCEFIELDSHAPEDESC
#define NX_PHYSICS_NXCONVEXFORCEFIELDSHAPEDESC
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
 \brief A descriptor for NxConvexForceFieldShape
 

<b>Platform:</b>
\li PC SW: Yes
\li GPU  : Yes [SW]
\li PS3  : Yes
\li XB360: Yes
\li WII	 : Yes

 @see NxConvexForceFieldShape NxForceFieldShapeDesc
*/
class NxConvexForceFieldShapeDesc : public NxForceFieldShapeDesc
	{
	public:

	/**
	\brief References the triangle mesh that we want to instance.

	<b>Default:</b> NULL

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes [SW]
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see NxConvexMesh NxConvexMeshDesc NxPhysicsSDK.createConvexMesh()
	*/
	NxConvexMesh*	meshData;

	/**
	\brief constructor sets to default.
	*/
	NX_INLINE NxConvexForceFieldShapeDesc();

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

NX_INLINE NxConvexForceFieldShapeDesc::NxConvexForceFieldShapeDesc() : NxForceFieldShapeDesc(NX_SHAPE_CONVEX)
	{
	setToDefault();
	}

NX_INLINE void NxConvexForceFieldShapeDesc::setToDefault()
	{
	NxForceFieldShapeDesc::setToDefault();
	meshData = NULL;
	}

NX_INLINE bool NxConvexForceFieldShapeDesc::isValid() const
	{
	if(!meshData)	return false;
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

