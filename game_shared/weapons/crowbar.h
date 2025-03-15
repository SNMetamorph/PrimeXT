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

#define WEAPON_CROWBAR		1
#define CROWBAR_WEIGHT		0
#define CROWBAR_CLASSNAME	weapon_crowbar

class CCrowbarWeaponContext : public CBaseWeaponContext
{
public:
	CCrowbarWeaponContext() = delete;
	~CCrowbarWeaponContext() = default;
	CCrowbarWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 1; }
	int GetItemInfo(ItemInfo *p) override;
	void PrimaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	bool Swing(bool fFirst);

	int m_iSwing;
	uint16_t m_usCrowbar;
#ifndef CLIENT_DLL
	TraceResult m_trHit;
#endif
};
