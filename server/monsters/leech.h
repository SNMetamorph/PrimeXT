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
// leech - basic little swimming monster
//=========================================================
//
// UNDONE:
// DONE:Steering force model for attack
// DONE:Attack animation control / damage
// DONE:Establish range of up/down motion and steer around vertical obstacles
// DONE:Re-evaluate height periodically
// DONE:Fall (MOVETYPE_TOSS) and play different anim if out of water
// Test in complex room (c2a3?)
// DONE:Sounds? - Kelly will fix
// Blood cloud? Hurt effect?
// Group behavior?
// DONE:Save/restore
// Flop animation - just bind to ACT_TWITCH
// Fix fatal push into wall case
//
// Try this on a bird
// Try this on a model with hulls/tracehull?
//

#pragma once

#include	"float.h"
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"

// Animation events
#define LEECH_AE_ATTACK		1
#define LEECH_AE_FLOP		2

// Movement constants

#define	LEECH_ACCELERATE		10
#define	LEECH_CHECK_DIST		45
#define	LEECH_SWIM_SPEED		50
#define	LEECH_SWIM_ACCEL		80
#define	LEECH_SWIM_DECEL		10
#define	LEECH_TURN_RATE		90
#define	LEECH_SIZEX		10
#define	LEECH_FRAMETIME		0.1

#define DEBUG_BEAMS		0

#if DEBUG_BEAMS
#include "effects.h"
#endif

class CLeech : public CBaseMonster
{
	DECLARE_CLASS( CLeech, CBaseMonster );
public:
	void Spawn( void );
	void Precache( void );
	void SwimThink( void );
	void DeadThink( void );

	void Touch( CBaseEntity *pOther );
	void SetObjectCollisionBox( void );

	void AttackSound( void );
	void AlertSound( void );
	void UpdateMotion( void );
	float ObstacleDistance( CBaseEntity *pTarget );
	void MakeVectors( void );
	void RecalculateWaterlevel( void );
	void SwitchLeechState( void );
	
	// Base entity functions
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int BloodColor( void ) { return DONT_BLEED; }
	void Killed( entvars_t *pevAttacker, int iGib );
	void Activate( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	int Classify( void ) { return m_iClass ? m_iClass : CLASS_INSECT; }
	int IRelationship( CBaseEntity *pTarget );

	DECLARE_DATADESC();

	static const char *pAttackSounds[];
	static const char *pAlertSounds[];

private:
	// UNDONE: Remove unused boid vars, do group behavior
	float	m_flTurning;// is this boid turning?
	BOOL	m_fPathBlocked;// TRUE if there is an obstacle ahead
	float	m_flAccelerate;
	float	m_obstacle;
	float	m_top;
	float	m_bottom;
	float	m_height;
	float	m_waterTime;
	float	m_sideTime;		// Timer to randomly check clearance on sides
	float	m_zTime;
	float	m_stateTime;
	float	m_attackSoundTime;

#if DEBUG_BEAMS
	CBeam	*m_pb;
	CBeam	*m_pt;
#endif
};
