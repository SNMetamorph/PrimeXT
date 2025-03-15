/*
mp5_fire_event.h
Copyright (C) 2025 SNMetamorph

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
#include "base_game_event.h"
#include "matrix.h"

class CMP5FireEvent : public CBaseGameEvent
{
public:
	CMP5FireEvent(event_args_t *args);
	~CMP5FireEvent() = default;

	void Execute(bool secondary);

private:
	void HandleShot();
	void HandleGrenadeLaunch();
	Vector GetShootDirection(const matrix3x3 &camera) const;
};
