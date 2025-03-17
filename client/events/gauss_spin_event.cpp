/*
gauss_spin_event.cpp
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

#include "gauss_spin_event.h"
#include "game_event_utils.h"
#include "hud.h"
#include "const.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"

// TODO place it somewhere else
#define SND_CHANGE_PITCH	(1<<7) 

CGaussSpinEvent::CGaussSpinEvent(event_args_t *args) :
	CBaseGameEvent(args)
{
}

void CGaussSpinEvent::Execute()
{
	gEngfuncs.pEventAPI->EV_PlaySound( GetEntityIndex(), GetOrigin(), CHAN_WEAPON, "ambience/pulsemachine.wav", 1.0, ATTN_NORM, GetSoundFlags(), GetPitch());
}

int CGaussSpinEvent::GetPitch() const
{
	return m_arguments->iparam1;
}

int CGaussSpinEvent::GetSoundFlags() const
{
	return m_arguments->bparam1 ? SND_CHANGE_PITCH : 0;
}
