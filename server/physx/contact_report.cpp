/*
contact_report.h - part of PhysX physics engine implementation
Copyright (C) 2023 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include "contact_report.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define CONVEYOR_SCALE_FACTOR	((1.0f / gpGlobals->frametime) * 0.5f)

using namespace physx;

CPhysicPhysX::ContactReport &CPhysicPhysX::ContactReport::getInstance()
{
	static ContactReport instance;
	return instance;
}

void CPhysicPhysX::ContactReport::onContact(const PxContactPairHeader &pairHeader, const PxContactPair *pairs, PxU32 nbPairs)
{
	for (PxU32 i = 0; i < nbPairs; i++)
	{
		const PxContactPair &pair = pairs[i];
		if (!FBitSet(pair.events, PxPairFlag::eNOTIFY_TOUCH_PERSISTS)) {
			return;
		}

		edict_t* e1 = (edict_t*)pairHeader.actors[0]->userData;
		edict_t* e2 = (edict_t*)pairHeader.actors[1]->userData;

		if (!e1 || !e2)
			return;

		if (e1->v.flags & FL_CONVEYOR)
		{
			PxRigidBody *actor = pairHeader.actors[1]->is<PxRigidBody>();
			Vector basevelocity = e1->v.movedir * e1->v.speed * CONVEYOR_SCALE_FACTOR;
			actor->setForceAndTorque(basevelocity, PxVec3(0.f), PxForceMode::eIMPULSE);
		}

		if (e2->v.flags & FL_CONVEYOR)
		{
			PxRigidBody* actor = pairHeader.actors[0]->is<PxRigidBody>();
			Vector basevelocity = e2->v.movedir * e2->v.speed * CONVEYOR_SCALE_FACTOR;
			actor->setForceAndTorque(basevelocity, PxVec3(0.f), PxForceMode::eIMPULSE);
		}

		if (e1 && e1->v.solid != SOLID_NOT)
		{
			// FIXME: build trace info
			DispatchTouch(e1, e2);
		}

		if (e2 && e2->v.solid != SOLID_NOT)
		{
			// FIXME: build trace info
			DispatchTouch(e1, e2);
		}
	}
}
