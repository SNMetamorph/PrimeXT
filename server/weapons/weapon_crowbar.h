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

class CCrowbar : public CBasePlayerWeapon
{
	DECLARE_CLASS( CCrowbar, CBasePlayerWeapon );

public:
	DECLARE_DATADESC();

	CCrowbar();

	void Spawn();
	void Precache();
	void SwingAgain();
	void Smack();
};
