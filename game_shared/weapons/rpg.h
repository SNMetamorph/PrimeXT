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
#include "weapon_context.h"
#include "weapon_layer.h"
#include <memory>

#ifndef CLIENT_DLL
#include "weapons.h"
#endif

#define WEAPON_RPG			8
#define RPG_WEIGHT			20
#define RPG_MAX_CLIP		1
#define RPG_DEFAULT_GIVE	1
#define RPG_CLASSNAME		weapon_rpg

enum rpg_e
{
	RPG_IDLE = 0,
	RPG_FIDGET,
	RPG_RELOAD,	// to reload
	RPG_FIRE2,	// to empty
	RPG_HOLSTER1,	// loaded
	RPG_DRAW1,	// loaded
	RPG_HOLSTER2,	// unloaded
	RPG_DRAW_UL,	// unloaded
	RPG_IDLE_UL,	// unloaded idle
	RPG_FIDGET_UL,	// unloaded fidget
};

class CRpgWeaponContext : public CBaseWeaponContext
{
public:
	CRpgWeaponContext() = delete;
	CRpgWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);
	~CRpgWeaponContext() = default;

	void Reload() override;
	int iItemSlot() override { return 4; }
	int GetItemInfo(ItemInfo *p) const override;
	bool Deploy() override;
	bool CanHolster() override;
	void Holster() override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void WeaponIdle() override;
	bool ShouldWeaponIdle() override { return true; };	// laser spot give updates from WeaponIdle
	void UpdateSpot();

#ifndef CLIENT_DLL
	CLaserSpot *m_pSpot;
#endif
	int m_cActiveRockets;// how many missiles in flight from this launcher right now?
	int m_fSpotActive;
	uint16_t m_usRpg;
};
