/*
game_event_utils.cpp - events-related code that used among several events
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

#include "game_event_utils.h"
#include "hud.h"
#include "utils.h"
#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "pm_defs.h"

void GameEventUtils::EjectBrass(const Vector &origin, const Vector &angles, const Vector &velocity, int modelIndex, int soundType)
{
	gEngfuncs.pEfxAPI->R_TempModel( 
		const_cast<float*>(&origin[0]), 
		const_cast<float*>(&velocity[0]), 
		const_cast<float*>(&angles[0]), 
		2.5, modelIndex, soundType);
}

void GameEventUtils::FireBullet(int entIndex, const matrix3x3 &camera, const Vector &origin, const Vector &direction, int tracerFreq)
{
	pmtrace_t tr;
	Vector endPos = origin + direction * 8196.f;

	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(entIndex - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2); // 2 is a point hull
	gEngfuncs.pEventAPI->EV_PlayerTrace(const_cast<float*>(&origin.x), endPos, PM_NORMAL, -1, &tr);
	CreateTracer(camera, origin, tr.endpos, tracerFreq);
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

void GameEventUtils::CreateTracer(const matrix3x3 &camera, const Vector &origin, const Vector &end, int frequency)
{
	static int32_t count = 0;
	if (count % frequency == 0) 
	{
		const Vector offset = Vector(0.f, 0.f, -4.f);
		Vector startPos = origin + offset + camera.GetRight() * -6.f + camera.GetForward() * 10.f;
		gEngfuncs.pEfxAPI->R_TracerEffect(startPos, const_cast<float*>(&end.x));
	}
	count++;
}

void GameEventUtils::SpawnMuzzleflash()
{
	cl_entity_t *ent = gEngfuncs.GetViewModel();
	if (ent) {
		ent->curstate.effects |= EF_MUZZLEFLASH;
	}
}
