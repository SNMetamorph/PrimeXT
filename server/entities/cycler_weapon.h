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

//Weapon Cycler
class CWeaponCycler : public CBasePlayerWeapon
{
	DECLARE_CLASS( CWeaponCycler, CBasePlayerWeapon );
public:
	void Spawn( void );
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL Deploy( void );
	void Holster( void );
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();
private:
	string_t	m_iPlayerModel;
	string_t	m_iWorldModel;
	string_t	m_iViewModel;

	struct {
		bool primary = false;
		bool secondary = false;
	} m_bActiveAnims;
};