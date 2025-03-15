/*
egon_fire_event.cpp
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

#include "egon_fire_event.h"
#include "game_event_utils.h"
#include "hud.h"
#include "const.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"

BEAM *CEgonFireEvent::pBeam = nullptr;
BEAM *CEgonFireEvent::pBeam2 = nullptr;

CEgonFireEvent::CEgonFireEvent(event_args_t *args) :
	CBaseGameEvent(args)
{
}

void CEgonFireEvent::Execute()
{
	const EGON_FIREMODE fireMode = GetFireMode();
	const int pitch = (fireMode == FIRE_WIDE) ? 125 : 100;
	const float volume = (fireMode == FIRE_WIDE) ? 0.98f : 0.9f;

	if (IsStartup()) {
		gEngfuncs.pEventAPI->EV_PlaySound(GetEntityIndex(), GetOrigin(), CHAN_WEAPON, EGON_SOUND_STARTUP, volume, ATTN_NORM, 0, pitch);
	}
	else {
		gEngfuncs.pEventAPI->EV_PlaySound(GetEntityIndex(), GetOrigin(), CHAN_STATIC, EGON_SOUND_RUN, volume, ATTN_NORM, 0, pitch);
	}

	if (IsEventLocal())
	{
		const matrix3x3 cameraTransform(GetAngles());
		const size_t index = gEngfuncs.pfnRandomLong(0, CEgonWeaponContext::g_fireAnims1.size() - 1);
		gEngfuncs.pEventAPI->EV_WeaponAnimation(CEgonWeaponContext::g_fireAnims1[index], 1);

		if (IsStartup() && !pBeam && !pBeam2)
		{
			pmtrace_t tr;
			cl_entity_t *pl = gEngfuncs.GetEntityByIndex(GetEntityIndex());
			if (pl)
			{
				vec3_t vecSrc = GetOrigin();
				vec3_t vecEnd = vecSrc + cameraTransform.GetForward() * 2048.f;

				gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
				gEngfuncs.pEventAPI->EV_PushPMStates();
				gEngfuncs.pEventAPI->EV_SetSolidPlayers(GetEntityIndex() - 1);
				gEngfuncs.pEventAPI->EV_SetTraceHull(2);
				gEngfuncs.pEventAPI->EV_PlayerTrace(vecSrc, vecEnd, PM_NORMAL, -1, &tr);
				gEngfuncs.pEventAPI->EV_PopPMStates();

				int beamModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex(EGON_BEAM_SPRITE);
				const float r = 0.5f;
				const float g = 0.5f;
				const float b = 125.0f;

				pBeam = gEngfuncs.pEfxAPI->R_BeamEntPoint(GetEntityIndex() | 0x1000, tr.endpos, beamModelIndex, 99999, 3.5, 0.2, 0.7, 55, 0, 0, r, g, b);

				if (pBeam)
					pBeam->flags |= (FBEAM_SINENOISE);

				pBeam2 = gEngfuncs.pEfxAPI->R_BeamEntPoint(GetEntityIndex() | 0x1000, tr.endpos, beamModelIndex, 99999, 5.0, 0.08, 0.7, 25, 0, 0, r, g, b);
			}
		}
	}
}

void CEgonFireEvent::UpdateBeams()
{
	pmtrace_t tr;
	vec3_t view_ofs;
	vec3_t vecEnd, angles;
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
		
	gEngfuncs.GetViewAngles(angles);
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(view_ofs);
	matrix3x3 cameraTransform(angles);
	vecEnd = player->origin + view_ofs + cameraTransform.GetForward() * 2048.f;
	
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(player->index - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(player->origin, vecEnd, PM_STUDIO_BOX, -1, &tr);
	gEngfuncs.pEventAPI->EV_PopPMStates();

	if (pBeam)
	{
		pBeam->target = tr.endpos;
		pBeam->die = gEngfuncs.GetClientTime() + 0.1f; // We keep it alive just a little bit forward in the future, just in case.
	}

	if (pBeam2)
	{
		pBeam2->target = tr.endpos;
		pBeam2->die = gEngfuncs.GetClientTime() + 0.1f; // We keep it alive just a little bit forward in the future, just in case.
	}
}

EGON_FIRESTATE CEgonFireEvent::GetFireState() const
{
	return static_cast<EGON_FIRESTATE>(m_arguments->iparam1);
}

EGON_FIREMODE CEgonFireEvent::GetFireMode() const
{
	return static_cast<EGON_FIREMODE>(m_arguments->iparam2);
}

bool CEgonFireEvent::IsStartup() const
{
	return m_arguments->bparam1 != 0;
}
