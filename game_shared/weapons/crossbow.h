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

#define WEAPON_CROSSBOW			6
#define CROSSBOW_WEIGHT			10
#define CROSSBOW_MAX_CLIP		5
#define CROSSBOW_DEFAULT_GIVE	5
#define CROSSBOW_CLASSNAME		weapon_crossbow

enum crossbow_e
{
	CROSSBOW_IDLE1 = 0,	// full
	CROSSBOW_IDLE2,	// empty
	CROSSBOW_FIDGET1,	// full
	CROSSBOW_FIDGET2,	// empty
	CROSSBOW_FIRE1,	// full
	CROSSBOW_FIRE2,	// reload
	CROSSBOW_FIRE3,	// empty
	CROSSBOW_RELOAD,	// from empty
	CROSSBOW_DRAW1,	// full
	CROSSBOW_DRAW2,	// empty
	CROSSBOW_HOLSTER1,	// full
	CROSSBOW_HOLSTER2,	// empty
};

class CCrossbowWeaponContext : public CBaseWeaponContext
{
public:
	CCrossbowWeaponContext() = delete;
	CCrossbowWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);
	~CCrossbowWeaponContext() = default;

	int iItemSlot() { return 3; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	void Reload() override;
	void WeaponIdle() override;
	void FireBolt();
	void FireSniperBolt();

	bool m_fInZoom; // don't save this
	uint16_t m_usCrossbow;
	uint16_t m_usCrossbow2;
};
