/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

//=========================================================
// Gargantua
//=========================================================

#pragma once

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"nodes.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"customentity.h"
#include	"weapons.h"
#include	"effects.h"
#include	"decals.h"
#include	"env_explosion.h"
#include	"func_break.h"

#define GARG_STOMP_SPRITE_NAME	"sprites/gargeye1.spr"
#define GARG_STOMP_BUZZ_SOUND		"weapons/mine_charge.wav"
#define	STOMP_INTERVAL		0.025
#define STOMP_SPRITE_COUNT		10
#define STOMP_FRAMETIME			0.015	// gpGlobals->frametime

class CStomp : public CBaseEntity
{
	DECLARE_CLASS( CStomp, CBaseEntity );
public:
	void Spawn( void );
	void Think( void );
	static CStomp *StompCreate( const Vector &origin, const Vector &end, float speed );

private:
// UNDONE: re-use this sprite list instead of creating new ones all the time
//	CSprite		*m_pSprites[ STOMP_SPRITE_COUNT ];
};