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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "shake.h"
#include "gamerules.h"
#include "user_messages.h"

class CGauss : public CBasePlayerWeapon
{
	DECLARE_CLASS( CGauss, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	DECLARE_DATADESC();

	BOOL Deploy( void );
	void Holster( void );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );

	int m_fInAttack;	
	float m_flStartCharge;
	float m_flPlayAftershock;
	void StartFire( void );
	void Fire( Vector vecOrigSrc, Vector vecDirShooting, float flDamage );
	float GetFullChargeTime( void );
	int m_iBalls;
	int m_iGlow;
	int m_iBeam;
	int m_iSoundState; // don't save this

	float m_flNextAmmoBurn;// while charging, when to absorb another unit of player's ammo?

	// was this weapon just fired primary or secondary?
	// we need to know so we can pick the right set of effects. 
	BOOL m_fPrimaryFire;
};
