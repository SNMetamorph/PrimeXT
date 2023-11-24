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
// sprite monster like in doom, wolf3d. Just for fun
// 		UNDER CONSTRUCTION
//=========================================================

#pragma once

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"

typedef enum
{
	STATE_IDLE = 0,
	STATE_RUN,
	STATE_ATTACK,
	STATE_PAIN,
	STATE_KILLED,
	STATE_GIBBED,
	MAX_ANIMS
} AISTATE;

typedef enum
{
	ATTACK_NONE = 0,
	ATTACK_STRAIGHT,
	ATTACK_SLIDING,
	ATTACK_MELEE,
	ATTACK_MISSILE
} ATTACKSTATE;

// distance ranges
typedef enum
{
	RANGE_MELEE = 0,
	RANGE_NEAR,
	RANGE_MID,
	RANGE_FAR
} RANGETYPE;

#define SF_FLYING_MONSTER	BIT( 0 )

class CSpriteMonster : public CBaseDelay
{
	DECLARE_CLASS( CSpriteMonster, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );

	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	RANGETYPE TargetRange( CBaseEntity *pTarg );
	BOOL TargetVisible( CBaseEntity *pTarg );
	void AttackFinished( float flFinishTime );
	void AI_Run( float flDist );
	void InitThink( void );
	void MonsterThink( void );
	BOOL FindTarget( void );
	BOOL InFront( CBaseEntity *pTarg );
	BOOL MonsterCheckAnyAttack( void );
	BOOL MonsterCheckAttack( void );
	void FoundTarget( void );
	void HuntTarget( void );
	void MonsterIdle( void );
	void MonsterSight( void );
	void MonsterRun( void );

	virtual STATE GetState( void ) { return (pev->effects & EF_NODRAW) ? STATE_OFF : STATE_ON; };

	DECLARE_DATADESC();

	AISTATE		m_iAIState;
	ATTACKSTATE	m_iAttackState;

	Vector		anims[MAX_ANIMS];

	float		m_flMoveDistance;	// member laste move distance. Used for strafe
	BOOL		m_fLeftY;

	float		m_flSightTime;
	EHANDLE		m_hSightEntity;
	int		m_iRefireCount;

	float		m_flEnemyYaw;
	RANGETYPE		m_iEnemyRange;
	BOOL		m_fEnemyInFront;
	BOOL		m_fEnemyVisible;
	float		m_flSearchTime;

	float		m_flAttackFinished;
	edict_t		*oldenemy;
};