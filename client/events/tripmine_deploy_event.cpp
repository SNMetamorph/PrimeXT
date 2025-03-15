/*
tripmine_deploy_event.cpp
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

#include "tripmine_deploy_event.h"
#include "game_event_utils.h"
#include "hud.h"
#include "const.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"
#include "weapons/tripmine.h"

CTripmineDeployEvent::CTripmineDeployEvent(event_args_t *args) :
	CBaseGameEvent(args)
{
}

void CTripmineDeployEvent::Execute()
{
	if (!IsEventLocal())
		return;

	pmtrace_t tr;
	matrix3x3 cameraTransform(GetAngles());
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(GetEntityIndex() - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(GetOrigin(), GetOrigin() + cameraTransform.GetForward() * 128, PM_NORMAL, -1, &tr);
	if (tr.fraction < 1.0) {
		gEngfuncs.pEventAPI->EV_WeaponAnimation(TRIPMINE_DRAW, 0);
	}
	gEngfuncs.pEventAPI->EV_PopPMStates();
}
