/*
hornetgun_fire_event.cpp
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

#include "hornetgun_fire_event.h"
#include "game_event_utils.h"
#include "hud.h"
#include "const.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"
#include "weapons/hornetgun.h"

CHornetgunFireEvent::CHornetgunFireEvent(event_args_t *args) :
	CBaseGameEvent(args)
{
}

void CHornetgunFireEvent::Execute()
{
	if (IsEventLocal())
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( HGUN_SHOOT, 1 );
		// V_PunchAxis( 0, gEngfuncs.pfnRandomLong ( 0, 2 ) );
	}

	const char *szSoundName = nullptr;
	switch ( gEngfuncs.pfnRandomLong ( 0 , 2 ) )
	{
		case 0:	szSoundName = "agrunt/ag_fire1.wav"; break;
		case 1: szSoundName = "agrunt/ag_fire2.wav"; break;
		case 2:	szSoundName = "agrunt/ag_fire3.wav"; break;
	}
	gEngfuncs.pEventAPI->EV_PlaySound( GetEntityIndex(), GetOrigin(), CHAN_WEAPON, szSoundName, 1, ATTN_NORM, 0, 100 );
}
