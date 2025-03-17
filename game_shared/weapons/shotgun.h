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

#pragma once
#include "weapon_context.h"
#include "weapon_layer.h"
#include <memory>
#include <utility>

#define WEAPON_SHOTGUN			7
#define SHOTGUN_WEIGHT			15
#define SHOTGUN_MAX_CLIP		8
#define SHOTGUN_DEFAULT_GIVE	12
#define SHOTGUN_CLASSNAME		weapon_shotgun

enum shotgun_e
{
	SHOTGUN_IDLE = 0,
	SHOTGUN_FIRE,
	SHOTGUN_FIRE2,
	SHOTGUN_RELOAD,
	SHOTGUN_PUMP,
	SHOTGUN_START_RELOAD,
	SHOTGUN_DRAW,
	SHOTGUN_HOLSTER,
	SHOTGUN_IDLE4,
	SHOTGUN_IDLE_DEEP
};

class CShotgunWeaponContext : public CBaseWeaponContext
{
public:
	CShotgunWeaponContext() = delete;
	~CShotgunWeaponContext() = default;
	CShotgunWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 3; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Reload() override;
	void WeaponIdle() override;

	uint16_t m_usSingleFire;
	uint16_t m_usDoubleFire;
};
