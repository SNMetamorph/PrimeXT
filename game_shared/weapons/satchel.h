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

#define WEAPON_SATCHEL			14
#define SATCHEL_WEIGHT			-10
#define SATCHEL_MAX_CLIP		WEAPON_NOCLIP
#define SATCHEL_DEFAULT_GIVE	1
#define SATCHEL_CLASSNAME		weapon_satchel

class CSatchelWeaponContext : public CBaseWeaponContext
{
public:
	CSatchelWeaponContext() = delete;
	CSatchelWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);
	~CSatchelWeaponContext() = default;

	int iItemSlot() override { return 5; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool CanDeploy() override;
	bool Deploy() override;
	bool IsUseable() override; 
	void Holster() override;
	void WeaponIdle() override;
	void Throw();

	int m_chargeReady;
};

template<>
struct CBaseWeaponContext::AssignedWeaponID<CSatchelWeaponContext> {
	static constexpr int32_t value = WEAPON_SATCHEL;
};
