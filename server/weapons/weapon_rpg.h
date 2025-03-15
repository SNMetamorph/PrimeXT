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
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "user_messages.h"

class CRpg : public CBasePlayerWeapon
{
	DECLARE_CLASS(CRpg, CBasePlayerWeapon);
public:
	CRpg();
	void Spawn() override;
	void Precache() override;
	int AddToPlayer(CBasePlayer *pPlayer) override;
	void AddActiveRocket();
	void RemoveActiveRocket();

	DECLARE_DATADESC();
};
