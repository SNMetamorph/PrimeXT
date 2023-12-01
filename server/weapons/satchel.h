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

class CSatchel : public CBasePlayerWeapon
{
	DECLARE_CLASS(CSatchel, CBasePlayerWeapon);
public:
	DECLARE_DATADESC();

	void Spawn(void);
	void Precache(void);
	int iItemSlot(void) { return 5; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer(CBasePlayer *pPlayer);
	void PrimaryAttack(void);
	void SecondaryAttack(void);
	int AddDuplicate(CBasePlayerItem *pOriginal);
	BOOL CanDeploy(void);
	BOOL Deploy(void);
	BOOL IsUseable(void);

	void Holster(void);
	void WeaponIdle(void);
	void Throw(void);
	int m_chargeReady;
};