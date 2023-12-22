/*
collision_filter_data.cpp - part of PhysX physics engine implementation
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

#include "collision_filter_data.h"

using namespace physx;

enum ColllisionFlags : PxU32
{
	ConveyorActor = (1 << 0),
};

CollisionFilterData::CollisionFilterData() :
	m_conveyorFlag(false)
{
}

CollisionFilterData::CollisionFilterData(const physx::PxFilterData &data)
{
	m_conveyorFlag = data.word0 & ColllisionFlags::ConveyorActor;
}

bool CollisionFilterData::HasConveyorFlag() const
{
	return m_conveyorFlag;
}

void CollisionFilterData::SetConveyorFlag(bool state)
{
	m_conveyorFlag = state;
}

PxFilterData CollisionFilterData::ToNativeType() const
{
	PxFilterData filterData;
	filterData.word0 |= m_conveyorFlag ? ColllisionFlags::ConveyorActor : 0;
	return filterData;
}
