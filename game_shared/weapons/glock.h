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

#define WEAPON_GLOCK		2
#define GLOCK_WEIGHT		10
#define GLOCK_MAX_CLIP		17
#define GLOCK_DEFAULT_GIVE	17
#define GLOCK_CLASSNAME		weapon_9mmhandgun

enum glock_e
{
	GLOCK_IDLE1 = 0,
	GLOCK_IDLE2,
	GLOCK_IDLE3,
	GLOCK_SHOOT,
	GLOCK_SHOOT_EMPTY,
	GLOCK_RELOAD,
	GLOCK_RELOAD_NOT_EMPTY,
	GLOCK_DRAW,
	GLOCK_HOLSTER,
	GLOCK_ADD_SILENCER
};

class CGlockWeaponContext : public CBaseWeaponContext
{
public:
	CGlockWeaponContext() = delete;
	CGlockWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);
	~CGlockWeaponContext() = default;
	
	int iItemSlot() override { return 2; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Reload() override;
	void WeaponIdle() override;
	void GlockFire( float flSpread, float flCycleTime, bool fUseAutoAim );

	uint16_t m_usFireGlock1;
	uint16_t m_usFireGlock2;
};
