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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#endif

#define WEAPON_TRIPMINE			13
#define TRIPMINE_WEIGHT			-10
#define TRIPMINE_MAX_CLIP		WEAPON_NOCLIP
#define TRIPMINE_DEFAULT_GIVE	1
#define TRIPMINE_CLASSNAME		weapon_tripmine

enum tripmine_e
{
	TRIPMINE_IDLE1 = 0,
	TRIPMINE_IDLE2,
	TRIPMINE_ARM1,
	TRIPMINE_ARM2,
	TRIPMINE_FIDGET,
	TRIPMINE_HOLSTER,
	TRIPMINE_DRAW,
	TRIPMINE_WORLD,
	TRIPMINE_GROUND,
};

class CTripmineWeaponContext : public CBaseWeaponContext
{
public:
	CTripmineWeaponContext() = delete;
	~CTripmineWeaponContext() = default;
	CTripmineWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 5; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	void WeaponIdle() override;

private:
	uint16_t m_usTripFire;
};
