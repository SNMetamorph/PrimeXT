/*
base_game_event.cpp - 
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

#include "base_game_event.h"
#include "hud.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"

CBaseGameEvent::CBaseGameEvent(event_args_s *args) :
	m_arguments(args)
{
}

bool CBaseGameEvent::IsEventLocal() const
{
	return gEngfuncs.pEventAPI->EV_IsLocal(m_arguments->entindex - 1) != 0;
}

bool CBaseGameEvent::IsEntityPlayer() const
{
	return (m_arguments->entindex >= 1) && (m_arguments->entindex <= gEngfuncs.GetMaxClients());
}
