/*
game_event_manager.h - class that responsible for registering game events handlers
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

class CGameEventManager
{
public:
	CGameEventManager();
	~CGameEventManager() = default;

private:
	CGameEventManager(const CGameEventManager&) = delete;
	CGameEventManager(CGameEventManager&&) = delete;
	CGameEventManager& operator=(const CGameEventManager&) = delete;
	CGameEventManager& operator=(CGameEventManager&&) = delete;

	void RegisterGlockEvents();
	void RegisterCrossbowEvents();
	void RegisterPythonEvents();
	void RegisterMP5Events();
	void RegisterShotgunEvents();
	void RegisterCrowbarEvents();
	void RegisterTripmineEvents();
	void RegisterSnarkEvents();
	void RegisterHornetgunEvents();
	void RegisterRPGEvents();
	void RegisterEgonEvents();
	void RegisterGaussEvents();
};
