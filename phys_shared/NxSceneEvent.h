#ifndef NXSCENEEVENT_H
#define NXSCENEEVENT_H

#include "Nxp.h"

#ifdef RRB_SUPPORTED_PLATFORM
class NxActor;
class NxShape;
class NxJoint;
class NxEffector;
class NxSpringAndDamperEffector;
class NxCompartment;
class NxForceField;
class NxForceFieldLinearKernel;
class NxForceFieldShapeGroup;
class NxMaterial;
class NxSweepCache;
class NxSceneQuery;
class NxFluid;
class NxCloth;
class NxSoftBody;

class NxSceneEvent
{
public:
	enum Type
	{
		CreateActor,
		CreateShape,
		CreateJoint,
		CreateEffector,
		CreateSpringAndDamperEffector,
		CreateCompartment,
		CreateForceField,
		CreateForceFieldLinearKernel,
		CreateForceFieldShapeGroup,
		CreateForceFieldVariety,
		CreateForceFieldMaterial,
		CreateMaterial,
		CreateSweepCache,
		CreateSceneQuery,
		CreateFluid,
		CreateCloth,
		CreateSoftBody,

		ReleaseActor,
		ReleaseShape,
		ReleaseJoint,
		ReleaseEffector,
		ReleaseSpringAndDamperEffector,
		ReleaseCompartment,
		ReleaseForceField,
		ReleaseForceFieldLinearKernel,
		ReleaseForceFieldShapeGroup,
		ReleaseForceFieldVariety,
		ReleaseForceFieldMaterial,
		ReleaseMaterial,
		ReleaseSweepCache,
		ReleaseSceneQuery,
		ReleaseFluid,
		ReleaseCloth,
		ReleaseSoftBody
	};

	NxSceneEvent(Type t,NxActor * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxShape * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxJoint * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxEffector * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxSpringAndDamperEffector * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxCompartment * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxForceField * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxForceFieldLinearKernel * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxForceFieldShapeGroup * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxForceFieldVariety a) : type(t),object((void *)a) {}			// can be used for any type that typedefs to NxU16
	NxSceneEvent(Type t,NxMaterial * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxSweepCache * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxSceneQuery * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxFluid * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxCloth * a) : type(t),object(a) {}
	NxSceneEvent(Type t,NxSoftBody * a) : type(t),object(a) {}

	Type    type;
	void *  object;
};

#endif
#endif
