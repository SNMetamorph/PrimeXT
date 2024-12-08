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
#include "gamerules.h"
#include "user_messages.h"
#include "weapon_logic_funcs.h"

class CCrossbowWeaponLogic : public CBaseWeaponLogic
{
public:
	CCrossbowWeaponLogic() = delete;
	CCrossbowWeaponLogic(IWeaponLogicFuncs *funcs);

	int iItemSlot( ) { return 3; }
	int GetItemInfo(ItemInfo *p);

	void FireBolt( void );
	void FireSniperBolt( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL Deploy( void );
	void Holster( void );
	void Reload( void );
	void WeaponIdle( void );

	int m_fInZoom; // don't save this
	int m_fZoomInUse;
};

class CCrossbow : public CBasePlayerWeapon
{
	DECLARE_CLASS( CCrossbow, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );

	DECLARE_DATADESC();
};
