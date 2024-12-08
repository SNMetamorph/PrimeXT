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
#include "weapon_logic_funcs.h"

class CGlockWeaponLogic : public CBaseWeaponLogic
{
public:
	CGlockWeaponLogic() = delete;
	CGlockWeaponLogic(IWeaponLogicFuncs *funcs);

	int iItemSlot( void ) { return 2; }
	int GetItemInfo(ItemInfo *p);
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void GlockFire( float flSpread, float flCycleTime, BOOL fUseAutoAim );
	BOOL Deploy( void );
	void Reload( void );
	void WeaponIdle( void );
};

class CGlock : public CBasePlayerWeapon
{
	DECLARE_CLASS( CGlock, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
};
