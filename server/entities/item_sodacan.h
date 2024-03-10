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

//=========================================================
// Soda can
//=========================================================
class CItemSoda : public CBaseEntity
{
	DECLARE_CLASS(CItemSoda, CBaseEntity);
public:
	void	Spawn(void);
	void	Precache(void);
	void	CanThink(void);
	void	CanTouch(CBaseEntity *pOther);

	DECLARE_DATADESC();
};
