/*
base_game_event.h - base class for game events
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
#include "event_args.h"
#include "vector.h"
#include <stdint.h>

class CBaseGameEvent
{
public:
	CBaseGameEvent(event_args_s *args);
	~CBaseGameEvent() = default;

protected:
	Vector GetOrigin() const { return Vector(m_arguments->origin); };
	Vector GetAngles() const { return Vector(m_arguments->angles); };
	Vector GetVelocity() const { return Vector(m_arguments->velocity); };
	int32_t GetEntityIndex() const { return m_arguments->entindex; };
	bool IsEventLocal() const;
	bool IsEntityPlayer() const;

	event_args_t *m_arguments;
};
