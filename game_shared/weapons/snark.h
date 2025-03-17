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

#define WEAPON_SNARK		15
#define SNARK_WEIGHT		5
#define SNARK_MAX_CLIP		WEAPON_NOCLIP
#define SNARK_DEFAULT_GIVE	5
#define SNARK_CLASSNAME		weapon_snark

enum w_squeak_e
{
	WSQUEAK_IDLE1 = 0,
	WSQUEAK_FIDGET,
	WSQUEAK_JUMP,
	WSQUEAK_RUN
};

enum squeak_e {
	SQUEAK_IDLE1 = 0,
	SQUEAK_FIDGETFIT,
	SQUEAK_FIDGETNIP,
	SQUEAK_DOWN,
	SQUEAK_UP,
	SQUEAK_THROW
};

class CSqueakWeaponContext : public CBaseWeaponContext
{
public:
	CSqueakWeaponContext() = delete;
	~CSqueakWeaponContext() = default;
	CSqueakWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 5; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	void WeaponIdle() override;

private:
	bool m_fJustThrown;
	uint16_t m_usSnarkFire;
};
