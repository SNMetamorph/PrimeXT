/*
holdable_item_controller.h - PD controller for held objects
Copyright (C) 2026 SNMetamorph

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
#include <map>
#include "ehandle.h"
#include "mathlib.h"

class HoldableItemController
{
public:
	struct Target
	{
		Vector origin;
		Vector4D quat;
	};

	void SetTarget( CBaseEntity *pEntity, const Vector &origin, const Vector4D &quat );
	void ClearTarget( CBaseEntity *pEntity );
	void ClearAllTargets();

	// called before each physics substep — applies PD forces/torques
	void ApplyForces();

private:
	struct Entry
	{
		EHANDLE handle;
		Target target;
	};

	std::map<int, Entry> m_entries;	// keyed by entity index
};
