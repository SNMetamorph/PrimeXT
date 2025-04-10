/*
weapon_context.h - part of weapons implementation common for client & server
Copyright (C) 2024 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#include "vector.h"
#include "item_info.h"
#include "cdll_dll.h"
#include "weapon_layer.h"
#include <memory>
#include <stdint.h>

class CBaseWeaponContext
{
public:
	CBaseWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);
	virtual ~CBaseWeaponContext();

	// called by CBasePlayerWeapons ItemPostFrame()
	virtual void PrimaryAttack() {}		// do "+ATTACK"
	virtual void SecondaryAttack() {}	// do "+ATTACK2"
	virtual void Reload() {}			// do "+RELOAD"
	virtual void WeaponIdle() {}		// called when no buttons pressed

	void ItemPostFrame();

	virtual bool ShouldWeaponIdle() { return false; };
	virtual bool CanDeploy();
	virtual bool Deploy() { return true; };		// returns is deploy was successful	 
	virtual bool CanHolster() { return true; };		// can this weapon be put away right nxow?
	virtual void Holster();
	virtual bool IsUseable();
	virtual bool UsePredicting() { return true; }; // always true because weapon prediction enabled regardless of anything
	
	virtual int GetItemInfo(ItemInfo *p) const { return 0; };	// returns 0 if struct not filled out
	virtual int	PrimaryAmmoIndex(); 
	virtual int	SecondaryAmmoIndex(); 

	virtual int iItemSlot();
	virtual int	iItemPosition();
	virtual const char *pszAmmo1();
	virtual int iMaxAmmo1();
	virtual const char *pszAmmo2();
	virtual int	iMaxAmmo2();
	virtual const char *pszName();
	virtual int	iMaxClip();
	virtual int	iWeight();
	virtual	int iFlags();

	bool DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int body = 0 );
	int DefaultReload( int iClipSize, int iAnim, float fDelay, int body = 0 );
	void SendWeaponAnim( int iAnim, int body = 0 );  // skiplocal is 1 if client is predicting weapon animations
	float GetNextPrimaryAttackDelay(float delay);
	bool CanAttack(float attack_time);
	bool PlayEmptySound();
	void ResetEmptySound();

	static ItemInfo ItemInfoArray[ MAX_WEAPONS ];
	static AmmoInfo AmmoInfoArray[ MAX_AMMO_SLOTS ];

	int	m_iId;							// WEAPON_???
	int m_iPlayEmptySound;
	int m_fFireOnEmpty;					// True when the gun is empty and the player is still holding down the attack key(s)
	float m_flPumpTime;
	int	m_fInSpecialReload;				// Are we in the middle of a reload for the shotguns
	float m_flNextPrimaryAttack;		// soonest time ItemPostFrame will call PrimaryAttack
	float m_flNextSecondaryAttack;		// soonest time ItemPostFrame will call SecondaryAttack
	float m_flPrevPrimaryAttack;		// required for calculating floating point error correction for primary fire timing
	float m_flLastFireTime;				// 
	float m_flTimeWeaponIdle;			// soonest time ItemPostFrame will call WeaponIdle
	int	m_iPrimaryAmmoType;				// "primary" ammo index into players m_rgAmmo[]
	int	m_iSecondaryAmmoType;			// "secondary" ammo index into players m_rgAmmo[]
	int	m_iClip;						// number of shots left in the primary weapon clip, -1 it not used
	int	m_iClientClip;					// the last version of m_iClip sent to hud dll
	int	m_iClientWeaponState;			// the last version of the weapon state sent to hud dll (is current weapon, is on target)
	int	m_fInReload;					// Are we in the middle of a reload;
	int	m_iDefaultAmmo;					// how much ammo you get when you pick up this weapon as placed by a level designer.
	std::unique_ptr<IWeaponLayer> m_pLayer;
};
