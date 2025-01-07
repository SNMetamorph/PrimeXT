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

#define WEAPON_GLOCK		2
#define GLOCK_WEIGHT		10
#define GLOCK_MAX_CLIP		17
#define GLOCK_DEFAULT_GIVE	17
#define GLOCK_CLASSNAME		weapon_glock

class CGlockWeaponLogic : public CBaseWeaponContext
{
public:
	CGlockWeaponLogic() = delete;
	CGlockWeaponLogic(IWeaponLayer *layer);

	int iItemSlot() override { return 2; }
	int GetItemInfo(ItemInfo *p) override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Reload() override;
	void WeaponIdle() override;
	void GlockFire( float flSpread, float flCycleTime, bool fUseAutoAim );

	uint16_t m_usFireGlock1;
	uint16_t m_usFireGlock2;
};
