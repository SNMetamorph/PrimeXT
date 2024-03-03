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
#include "maprules.h"

//
// CGamePlayerHurt / game_player_hurt	-- Damages the player who fires it
// Flag: Fire once
#define SF_PKILL_FIREONCE	0x0001

class CGamePlayerHurt : public CRulePointEntity
{
	DECLARE_CLASS( CGamePlayerHurt, CRulePointEntity );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	inline BOOL RemoveOnFire( void ) { return (pev->spawnflags & SF_PKILL_FIREONCE) ? TRUE : FALSE; }
};
