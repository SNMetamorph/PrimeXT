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

===== mortar.cpp ========================================================

  the "LaBuznik" mortar device              

*/

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "weapons.h"
#include "decals.h"
#include "soundent.h"
#include "ggrenade.h"

class CMortar : public CGrenade
{
	DECLARE_CLASS( CMortar, CGrenade );
public:
	void Spawn( void );
	void Precache( void );
	void MortarExplode( void );

	DECLARE_DATADESC();

	int m_spriteTexture;
};