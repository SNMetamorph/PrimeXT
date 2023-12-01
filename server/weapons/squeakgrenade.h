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
#include "ggrenade.h"

class CSqueak : public CBasePlayerWeapon
{
	DECLARE_CLASS(CSqueak, CBasePlayerWeapon);
public:
	void Spawn(void);
	void Precache(void);
	int iItemSlot(void) { return 5; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack(void);
	BOOL Deploy(void);
	void Holster(void);
	void WeaponIdle(void);
	int m_fJustThrown;
};

enum w_squeak_e
{
	WSQUEAK_IDLE1 = 0,
	WSQUEAK_FIDGET,
	WSQUEAK_JUMP,
	WSQUEAK_RUN,
};