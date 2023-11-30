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
	void Spawn(void);
	void Precache(void);
	void Reload(void);
	int iItemSlot(void) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer(CBasePlayer *pPlayer);

	DECLARE_DATADESC();

	BOOL Deploy(void);
	BOOL CanHolster(void);
	void Holster(void);

	void PrimaryAttack(void);
	void SecondaryAttack(void);
	void WeaponIdle(void);

	void UpdateSpot(void);
	BOOL ShouldWeaponIdle(void) { return TRUE; };	// laser spot give updates from WeaponIdle

	CLaserSpot *m_pSpot;
	int m_cActiveRockets;// how many missiles in flight from this launcher right now?
	int m_fSpotActive;
};
