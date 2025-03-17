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

#define WEAPON_HORNETGUN		11
#define HORNETGUN_WEIGHT		10
#define HORNETGUN_MAX_CLIP		WEAPON_NOCLIP
#define HIVEHAND_DEFAULT_GIVE	8
#define HORNETGUN_CLASSNAME		weapon_hornetgun

enum hgun_e
{
	HGUN_IDLE1 = 0,
	HGUN_FIDGETSWAY,
	HGUN_FIDGETSHAKE,
	HGUN_DOWN,
	HGUN_UP,
	HGUN_SHOOT
};

class CHornetgunWeaponContext : public CBaseWeaponContext
{
public:
	CHornetgunWeaponContext() = delete;
	~CHornetgunWeaponContext() = default;
	CHornetgunWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 4; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	bool IsUseable() override { return TRUE; };
	void Holster() override;
	void Reload() override;
	void WeaponIdle() override;

private:
	float GetRechargeTime() const;

	float m_flRechargeTime;
	int m_iFirePhase; // don't save me.
	uint16_t m_usHornetFire;
};
