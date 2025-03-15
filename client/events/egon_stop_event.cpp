/*
egon_stop_event.cpp
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

#include "egon_stop_event.h"
#include "egon_fire_event.h"
#include "game_event_utils.h"
#include "hud.h"
#include "const.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"

CEgonStopEvent::CEgonStopEvent(event_args_t *args) :
	CBaseGameEvent(args)
{
}

void CEgonStopEvent::Execute()
{
	gEngfuncs.pEventAPI->EV_StopSound(GetEntityIndex(), CHAN_STATIC, EGON_SOUND_RUN);

	if (ShouldMakeNoise()) {
		gEngfuncs.pEventAPI->EV_PlaySound(GetEntityIndex(), GetOrigin(), CHAN_WEAPON, EGON_SOUND_OFF, 0.98, ATTN_NORM, 0, 100);
	}

	if (IsEventLocal())
	{
		if (CEgonFireEvent::pBeam)
		{
			CEgonFireEvent::pBeam->die = 0.0f;
			CEgonFireEvent::pBeam = nullptr;
		}
		if (CEgonFireEvent::pBeam2)
		{
			CEgonFireEvent::pBeam2->die = 0.0f;
			CEgonFireEvent::pBeam2 = nullptr;
		}
	}
}

bool CEgonStopEvent::ShouldMakeNoise() const
{
	return m_arguments->iparam1 != 0;
}
