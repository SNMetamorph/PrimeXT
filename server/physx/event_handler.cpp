/*
event_handler.h - class for handling PhysX simulation events
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
#include "event_handler.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include <algorithm>

using namespace physx;

void EventHandler::onWorldInit()
{
	m_impl = std::make_unique<Impl>();
}

void EventHandler::onWorldShutdown()
{
	m_impl.reset();
}

EventHandler::TouchEventsQueue& EventHandler::getTouchEventsQueue()
{
	return m_impl->touchEventsQueue;
}

std::vector<EventHandler::WaterContactPair>& EventHandler::getWaterContactPairs()
{
	return m_impl->waterContactPairs;
}


// Documentation:
// SDK state should not be modified from within the callbacks. In particular objects should not
// be created or destroyed. If state modification is needed then the changes should be stored to a buffer
// and performed after the simulation step.
void EventHandler::onTrigger(PxTriggerPair* pairs, PxU32 count)
{
	for (PxU32 i = 0; i < count; i++)
	{
		const PxTriggerPair &pair = pairs[i];
		if (pair.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER)) {
			continue; // ignore pairs when shapes have been deleted
		}

		edict_t *triggerEntity = reinterpret_cast<edict_t*>(pair.triggerActor->userData);
		if (triggerEntity && FStrEq(STRING(triggerEntity->v.classname), "func_water")) 
		{
			WaterContactPair contactPair = { pair.triggerActor, pair.otherActor };
			if (pair.status == PxPairFlag::eNOTIFY_TOUCH_FOUND) 
			{
				auto searchIter = std::find(m_impl->waterContactPairs.begin(), m_impl->waterContactPairs.end(), contactPair);
				if (searchIter == std::end(m_impl->waterContactPairs)) {
					m_impl->waterContactPairs.push_back(contactPair);
				}
			}
			else if (pair.status == PxPairFlag::eNOTIFY_TOUCH_LOST) 
			{
 				auto it = std::remove_if(m_impl->waterContactPairs.begin(), m_impl->waterContactPairs.end(), [&contactPair](const auto &p) {
					return p == contactPair;
				});
				m_impl->waterContactPairs.erase(it, m_impl->waterContactPairs.end());
			}
		}
	}
}

void EventHandler::onContact(const PxContactPairHeader &pairHeader, const PxContactPair *pairs, PxU32 nbPairs)
{
	for (PxU32 i = 0; i < nbPairs; i++)
	{
		const PxContactPair &pair = pairs[i];
		if (!FBitSet(pair.events, PxPairFlag::eNOTIFY_TOUCH_PERSISTS)) {
			return;
		}

		PxActor *a1 = pairHeader.actors[0];
		PxActor *a2 = pairHeader.actors[1];
		edict_t *e1 = (edict_t*)a1->userData;
		edict_t *e2 = (edict_t*)a2->userData;

		if (!e1 || !e2)
			return;

		if (e1 && e1->v.solid != SOLID_NOT)
		{
			// FIXME: build trace info
			m_impl->touchEventsQueue.push({ a1, a2 });
		}

		if (e2 && e2->v.solid != SOLID_NOT)
		{
			// FIXME: build trace info
			m_impl->touchEventsQueue.push({ a2, a1 });
		}
	}
}
