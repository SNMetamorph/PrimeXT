/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"

class CTripmine : public CBasePlayerWeapon
{
	DECLARE_CLASS(CTripmine, CBasePlayerWeapon);
public:
	CTripmine();

	void Spawn();
	void Precache();
	void SetObjectCollisionBox()
	{
		//!!!BUGBUG - fix the model!
		pev->absmin = GetAbsOrigin() + Vector(-16, -16, -5);
		pev->absmax = GetAbsOrigin() + Vector(16, 16, 28);
	}
};
