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
#pragma once
#include <PxConfig.h>
#include <PxPhysicsAPI.h>
#include <PxSimulationEventCallback.h>
#include <queue>
#include <vector>
#include <memory>
#include <utility>

class EventHandler : public physx::PxSimulationEventCallback
{
public:
	using TouchEventsQueue = std::queue<std::pair<physx::PxActor*, physx::PxActor*>>;
	struct WaterContactPair
	{
		physx::PxActor *waterTriggerActor;
		physx::PxActor *objectActor;

		bool operator==(const WaterContactPair &operand) const {
			return waterTriggerActor == operand.waterTriggerActor && objectActor == operand.objectActor;
		}
	};
	
	void onWorldInit();
	void onWorldShutdown();
	TouchEventsQueue &getTouchEventsQueue();
	std::vector<WaterContactPair> &getWaterContactPairs();

	virtual void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) {};
	virtual void onWake(physx::PxActor** actors, physx::PxU32 count) {};
	virtual void onSleep(physx::PxActor** actors, physx::PxU32 count) {};
	virtual void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count);
	virtual void onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) {};
	virtual void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs);

private:
	struct Impl
	{ 
		TouchEventsQueue touchEventsQueue;
		std::vector<WaterContactPair> waterContactPairs;
	};
	std::unique_ptr<Impl> m_impl;
};
