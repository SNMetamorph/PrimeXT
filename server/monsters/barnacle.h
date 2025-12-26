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
// barnacle - stationary ceiling mounted 'fishing' monster
//=========================================================

#pragma once

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"

#define	BARNACLE_BODY_HEIGHT	44 // how 'tall' the barnacle's model is.
#define BARNACLE_PULL_SPEED		8
#define BARNACLE_KILL_VICTIM_DELAY	5 // how many seconds after pulling prey in to gib them. 
#define BARNACLE_CHECK_SPACING	8

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	BARNACLE_AE_PUKEGIB	2

class CBarnacle : public CBaseMonster
{
	DECLARE_CLASS( CBarnacle, CBaseMonster );
public:
	void Spawn( void );
	void Precache( void );
	CBaseEntity *TongueTouchEnt ( float *pflLength );
	int Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void BarnacleThink ( void );
	void WaitTillDead ( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void SetObjectCollisionBox();

	float m_flAltitude;
	float m_flCachedLength;
	float m_flKillVictimTime;
	int m_cGibs;		// barnacle loads up on gibs each time it kills something.
	BOOL m_fTongueExtended;
	BOOL m_fLiftingPrey;
	float m_flTongueAdj;

	DECLARE_DATADESC();
};
