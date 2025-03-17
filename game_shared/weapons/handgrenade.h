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

#define WEAPON_HANDGRENADE			12
#define HANDGRENADE_WEIGHT			5
#define HANDGRENADE_MAX_CLIP		WEAPON_NOCLIP
#define HANDGRENADE_DEFAULT_GIVE	1
#define HANDGRENADE_CLASSNAME		weapon_handgrenade

class CHandGrenadeWeaponContext : public CBaseWeaponContext
{
public:
	CHandGrenadeWeaponContext() = delete;
	CHandGrenadeWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);
	~CHandGrenadeWeaponContext() = default;

	int iItemSlot() override { return 5; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	bool Deploy() override;
	bool CanHolster() override;
	void Holster() override;
	void WeaponIdle() override;

	float m_flStartThrow;
	float m_flReleaseThrow;
};
