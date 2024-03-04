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
// CONTROLLER
//=========================================================

#pragma once

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"env_sprite.h"
#include	"schedule.h"
#include	"weapons.h"
#include	"squadmonster.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define CONTROLLER_AE_HEAD_OPEN	1
#define CONTROLLER_AE_BALL_SHOOT	2
#define CONTROLLER_AE_SMALL_SHOOT	3
#define CONTROLLER_AE_POWERUP_FULL	4
#define CONTROLLER_AE_POWERUP_HALF	5

#define CONTROLLER_FLINCH_DELAY	2		// at most one flinch every n secs

class CController : public CSquadMonster
{
	DECLARE_CLASS( CController, CSquadMonster );
public:
	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );

	void RunAI( void );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );	// balls
	BOOL CheckRangeAttack2 ( float flDot, float flDist );	// head
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );	// block, throw
	Schedule_t* GetSchedule ( void );
	Schedule_t* GetScheduleOfType ( int Type );
	void StartTask ( Task_t *pTask );
	void RunTask ( Task_t *pTask );
	CUSTOM_SCHEDULES;

	void Stop( void );
	void Move ( float flInterval );
	int  CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist );
	void MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval );
	void SetActivity ( Activity NewActivity );
	BOOL ShouldAdvanceRoute( float flWaypointDist );
	int LookupFloat( );

	float m_flNextFlinch;

	float m_flShootTime;
	float m_flShootEnd;

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void DeathSound( void );

	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pDeathSounds[];

	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );

	CSprite *m_pBall[2];	// hand balls
	int m_iBall[2];			// how bright it should be
	float m_iBallTime[2];	// when it should be that color
	int m_iBallCurrent[2];	// current brightness

	Vector m_vecEstVelocity;

	Vector m_velocity;
	int m_fInCombat;
};
