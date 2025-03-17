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

#define WEAPON_GAUSS			9
#define GAUSS_WEIGHT			20
#define GAUSS_MAX_CLIP			WEAPON_NOCLIP
#define GAUSS_DEFAULT_GIVE		20
#define GAUSS_CLASSNAME			weapon_gauss

#define GAUSS_PRIMARY_CHARGE_VOLUME	256 // how loud gauss is while charging
#define GAUSS_PRIMARY_FIRE_VOLUME	450 // how loud gauss is when discharged

enum gauss_e
{
	GAUSS_IDLE = 0,
	GAUSS_IDLE2,
	GAUSS_FIDGET,
	GAUSS_SPINUP,
	GAUSS_SPIN,
	GAUSS_FIRE,
	GAUSS_FIRE2,
	GAUSS_HOLSTER,
	GAUSS_DRAW
};

class CGaussWeaponContext : public CBaseWeaponContext
{
public:
	CGaussWeaponContext() = delete;
	CGaussWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);
	~CGaussWeaponContext() = default;

	int iItemSlot() override { return 4; }
	int GetItemInfo(ItemInfo *p) const override;
	bool Deploy() override;
	void Holster() override;
	void PrimaryAttack() override;
	void SecondaryAttack() override;
	void WeaponIdle() override;

	void StartFire();
	void Fire( Vector vecOrigSrc, Vector vecDirShooting, float flDamage );
	float GetFullChargeTime();

	int m_fInAttack;	
	float m_flStartCharge;
	float m_flAmmoStartCharge;
	float m_flPlayAftershock;
	int m_iSoundState; // don't save this
	float m_flNextAmmoBurn; // while charging, when to absorb another unit of player's ammo?

	// was this weapon just fired primary or secondary?
	// we need to know so we can pick the right set of effects. 
	BOOL m_fPrimaryFire;
	uint16_t m_usGaussFire;
	uint16_t m_usGaussSpin;
};
