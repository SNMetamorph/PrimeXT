#ifndef NX_PHYSICS_NX_CLOTHMESH
#define NX_PHYSICS_NX_CLOTHMESH
/** \addtogroup cloth
  @{
*/
/*----------------------------------------------------------------------------*\
|
|					Public Interface to NVIDIA PhysX Technology
|
|							     www.nvidia.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxClothMeshDesc.h"

class NxStream;

class NxClothMesh
{
protected:
	NX_INLINE NxClothMesh() {}
	virtual ~NxClothMesh() {}

public:
	/**
	\brief Saves the cloth descriptor. 
	A cloth mesh is created via the cooker. The cooker potentially changes the
	order of the arrays references by the pointers points and triangles.	
	Since saveToDesc returns the data of the cooked mesh, this data might
	differ from the originally provided data. Note that this is in contrast to the meshData
	member of NxClothDesc, which is guaranteed to provide data in the same order as
	that used to create the mesh.
	
	\param desc The descriptor used to retrieve the state of the object.
	\return True on success.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see NxClothMeshDesc
	*/
	virtual	bool saveToDesc(NxClothMeshDesc& desc) const = 0;

	/**
	\brief Gets the number of cloth instances referencing this cloth mesh.

	<b>Platform:</b>
	\li PC SW: Yes
	\li GPU  : Yes
	\li PS3  : Yes
	\li XB360: Yes
	\li WII	 : Yes

	@see NxCloth
	*/
	virtual	NxU32 getReferenceCount() const  = 0;
};
/** @} */
#endif
//NVIDIACOPYRIGHTBEGIN
///////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010 NVIDIA Corporation
// All rights reserved. www.nvidia.com
///////////////////////////////////////////////////////////////////////////
//NVIDIACOPYRIGHTEND

