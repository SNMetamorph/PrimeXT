/*
contact_modify_callback.cpp - part of PhysX physics engine implementation
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

#include "contact_modify_callback.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"

using namespace physx;

void ContactModifyCallback::onContactModify(PxContactModifyPair* const pairs, PxU32 count)
{
	for (PxU32 i = 0; i < count; i++)
	{
		PxContactModifyPair &pair = pairs[i];
		const PxActor *a1 = pair.actor[0];
		const PxActor *a2 = pair.actor[1];
		edict_t *e1 = (edict_t*)a1->userData;
		edict_t *e2 = (edict_t*)a2->userData;

		if (!e1 || !e2)
			return;

		if (FBitSet(e1->v.flags, FL_CONVEYOR) || FBitSet(e2->v.flags, FL_CONVEYOR))
		{
			edict_t *conveyorEntity = FBitSet(e1->v.flags, FL_CONVEYOR) ? e1 : e2;
			Vector conveyorSpeed = conveyorEntity->v.movedir * conveyorEntity->v.speed;
			for (PxU32 j = 0; j < pair.contacts.size(); j++) {
				pair.contacts.setTargetVelocity(j, conveyorSpeed);
			}
		}
	}
}
