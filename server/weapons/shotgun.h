/*
*
*	Copyright(c) 1996 - 2002, Valve LLC.All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").Id Technology(c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and /or resulting
* object code is restricted to non - commercial enhancements to products from
* Valve LLC.All other use, distribution, or modification is prohibited
* without written permission from Valve LLC.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "user_messages.h"

class CShotgun : public CBasePlayerWeapon
{
	DECLARE_CLASS(CShotgun, CBasePlayerWeapon);
public:
	void Spawn(void);
	void Precache(void);
	int iItemSlot() { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer(CBasePlayer *pPlayer);

	DECLARE_DATADESC();

	void PrimaryAttack(void);
	void SecondaryAttack(void);
	BOOL Deploy();
	void Reload(void);
	void WeaponIdle(void);
	int m_fInReload;
	float m_flNextReload;
	int m_iShell;
};