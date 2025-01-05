/*
game_event_utils.h - events-related code that used among several events
Copyright (C) 2024 SNMetamorph

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
#include "vector.h"

namespace GameEventUtils
{
	void EjectBrass(const Vector &origin, const Vector &angles, const Vector &velocity, int modelIndex, int soundType);
	void SpawnMuzzleflash();
}
