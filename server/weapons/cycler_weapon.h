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
/*

===== h_cycler.cpp ========================================================

  The Halflife Cycler Monsters

*/

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "weapons.h"
#include "player.h"
#include "weapon_context.h"
#include "weapon_layer.h"
#include <memory>

#define WEAPON_CYCLER	5

class CCyclerWeaponContext : public CBaseWeaponContext
{
public:
	CCyclerWeaponContext() = delete;
	CCyclerWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);
	~CCyclerWeaponContext() = default;

	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	bool UsePredicting() override { return false; }

	string_t	m_iPlayerModel;
	string_t	m_iWorldModel;
	string_t	m_iViewModel;

	struct {
		bool primary = false;
		bool secondary = false;
	} m_bActiveAnims;
};

class CWeaponCycler : public CBasePlayerWeapon
{
	DECLARE_CLASS( CWeaponCycler, CBasePlayerWeapon );
public:
	CWeaponCycler();
	void Spawn();
	void KeyValue(KeyValueData *pkvd);

	DECLARE_DATADESC();
};
