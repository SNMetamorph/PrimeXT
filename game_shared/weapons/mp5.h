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
#include <utility>

#define WEAPON_MP5			4
#define MP5_WEIGHT			15
#define MP5_MAX_CLIP		50
#define MP5_DEFAULT_AMMO	25
#define MP5_DEFAULT_GIVE	25
#define MP5_CLASSNAME		weapon_9mmAR

enum mp5_e
{
	MP5_LONGIDLE = 0,
	MP5_IDLE1,
	MP5_LAUNCH,
	MP5_RELOAD,
	MP5_DEPLOY,
	MP5_FIRE1,
	MP5_FIRE2,
	MP5_FIRE3,
};

class CMP5WeaponContext : public CBaseWeaponContext
{
public:
	CMP5WeaponContext() = delete;
	~CMP5WeaponContext() = default;
	CMP5WeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 3; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	int SecondaryAmmoIndex() override;
	bool Deploy() override;
	void Reload() override;
	void WeaponIdle() override;

	uint16_t m_usEvent1;
	uint16_t m_usEvent2;
};
