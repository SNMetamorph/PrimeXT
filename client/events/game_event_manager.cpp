/*
game_event_manager.cpp - class that responsible for registering game events handlers
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

#include "game_event_manager.h"
#include "hud.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"

CGameEventManager::CGameEventManager()
{
	RegisterGlockEvents();
}

void CGameEventManager::RegisterGlockEvents()
{
	gEngfuncs.pfnHookEvent("events/glock1.sc", [](event_args_s *args) {

	});
}