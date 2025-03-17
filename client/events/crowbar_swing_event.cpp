/*
crowbar_swing_event.cpp
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

#include "crowbar_swing_event.h"
#include "game_event_utils.h"
#include "hud.h"
#include "const.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"
#include "weapons/crowbar.h"

int CCrowbarSwingEvent::m_iSwing = 0;

CCrowbarSwingEvent::CCrowbarSwingEvent(event_args_t *args) :
	CBaseGameEvent(args)
{
}

void CCrowbarSwingEvent::Execute()
{
	gEngfuncs.pEventAPI->EV_PlaySound(GetEntityIndex(), GetOrigin(), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM); 
	if (IsEventLocal())
	{
		switch ((m_iSwing++) % 3)
		{
			case 0:
				gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK1MISS, 1);
				break;
			case 1:
				gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK2MISS, 1); 
				break;
			case 2:
				gEngfuncs.pEventAPI->EV_WeaponAnimation(CROWBAR_ATTACK3MISS, 1); 
				break;
		}
	}
}
