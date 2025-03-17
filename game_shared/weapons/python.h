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
#include <utility>

#define WEAPON_PYTHON			3
#define PYTHON_WEIGHT			15
#define PYTHON_MAX_CLIP			6
#define PYTHON_DEFAULT_GIVE		6
#define PYTHON_CLASSNAME		weapon_357

enum python_e
{
	PYTHON_IDLE1 = 0,
	PYTHON_FIDGET,
	PYTHON_FIRE1,
	PYTHON_RELOAD,
	PYTHON_HOLSTER,
	PYTHON_DRAW,
	PYTHON_IDLE2,
	PYTHON_IDLE3
};

class CPythonWeaponContext : public CBaseWeaponContext
{
public:
	CPythonWeaponContext() = delete;
	~CPythonWeaponContext() = default;
	CPythonWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot() override { return 2; }
	int GetItemInfo(ItemInfo *p) const override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	bool Deploy() override;
	void Holster() override;
	void Reload() override;
	void WeaponIdle() override;

	bool m_fInZoom;	// don't save this. 
	uint16_t m_usFirePython;
};
