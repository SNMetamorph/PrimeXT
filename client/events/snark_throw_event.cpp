/*
snark_throw_event.cpp
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

#include "snark_throw_event.h"
#include "game_event_utils.h"
#include "hud.h"
#include "const.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"
#include "weapons/snark.h"

CSnarkThrowEvent::CSnarkThrowEvent(event_args_t *args) :
	CBaseGameEvent(args)
{
}

void CSnarkThrowEvent::Execute()
{
	if (!IsEventLocal())
		return;

	pmtrace_t tr;
	matrix3x3 cameraTransform(GetAngles());
	const Vector origin = GetStartOrigin();

	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(GetEntityIndex() - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(origin + cameraTransform.GetForward() * 20, origin + cameraTransform.GetForward() * 64, PM_NORMAL, -1, &tr);

	if (!tr.allsolid && !tr.startsolid && tr.fraction > 0.25f) {
		gEngfuncs.pEventAPI->EV_WeaponAnimation(SQUEAK_THROW, 0);
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

Vector CSnarkThrowEvent::GetStartOrigin() const
{
	const Vector hullMin = { -16.f, -16.f, -36.f };
	const Vector duckHullMin = { -16.f, -16.f, -18.f };

	if (m_arguments->ducking)
		return GetOrigin() - (hullMin - duckHullMin);
	else
		return GetOrigin();
}
