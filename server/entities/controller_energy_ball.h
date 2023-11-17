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
// CONTROLLER ZAPBALL
//=========================================================

#pragma once

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"effects.h"
#include	"schedule.h"
#include	"weapons.h"
#include	"squadmonster.h"

class CControllerZapBall : public CBaseMonster
{
	DECLARE_CLASS( CControllerZapBall, CBaseMonster );

	void Spawn( void );
	void Precache( void );
	void AnimateThink( void );
	void ExplodeTouch( CBaseEntity *pOther );

	DECLARE_DATADESC();

	EHANDLE m_hOwner;
};