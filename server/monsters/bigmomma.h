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
// monster template
//=========================================================

#pragma once

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"decals.h"
#include	"weapons.h"
#include	"game.h"

#define SF_INFOBM_RUN		0x0001
#define SF_INFOBM_WAIT		0x0002

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define BIG_AE_STEP1			1		// Footstep left
#define BIG_AE_STEP2			2		// Footstep right
#define BIG_AE_STEP3			3		// Footstep back left
#define BIG_AE_STEP4			4		// Footstep back right
#define BIG_AE_SACK				5		// Sack slosh
#define BIG_AE_DEATHSOUND			6		// Death sound

#define BIG_AE_MELEE_ATTACKBR			8		// Leg attack
#define BIG_AE_MELEE_ATTACKBL			9		// Leg attack
#define BIG_AE_MELEE_ATTACK1			10		// Leg attack
#define BIG_AE_MORTAR_ATTACK1			11		// Launch a mortar
#define BIG_AE_LAY_CRAB			12		// Lay a headcrab
#define BIG_AE_JUMP_FORWARD			13		// Jump up and forward
#define BIG_AE_SCREAM			14		// alert sound
#define BIG_AE_PAIN_SOUND			15		// pain sound
#define BIG_AE_ATTACK_SOUND			16		// attack sound
#define BIG_AE_BIRTH_SOUND			17		// birth sound
#define BIG_AE_EARLY_TARGET			50		// Fire target early

// User defined conditions
#define bits_COND_NODE_SEQUENCE		( bits_COND_SPECIAL1 )		// pev->netname contains the name of a sequence to play

// Attack distance constants
#define	BIG_ATTACKDIST			170
#define BIG_MORTARDIST			800
#define BIG_MAXCHILDREN			20			// Max # of live headcrab children


#define bits_MEMORY_CHILDPAIR		(bits_MEMORY_CUSTOM1)
#define bits_MEMORY_ADVANCE_NODE	(bits_MEMORY_CUSTOM2)
#define bits_MEMORY_COMPLETED_NODE	(bits_MEMORY_CUSTOM3)
#define bits_MEMORY_FIRED_NODE	(bits_MEMORY_CUSTOM4)

int gSpitSprite, gSpitDebrisSprite;
Vector VecCheckSplatToss( entvars_t *pev, const Vector &vecSpot1, Vector vecSpot2, float maxHeight );
void MortarSpray( const Vector &position, const Vector &direction, int spriteModel, int count );


// UNDONE:	
//
#define BIG_CHILDCLASS		"monster_babycrab"

class CBigMomma : public CBaseMonster
{
	DECLARE_CLASS( CBigMomma, CBaseMonster );
public:
	void Spawn( void );
	void Precache( void );
	void Activate( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	void		RunTask( Task_t *pTask );
	void		StartTask( Task_t *pTask );
	Schedule_t	*GetSchedule( void );
	Schedule_t	*GetScheduleOfType( int Type );
	void		TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );

	void NodeStart( int iszNextNode );
	void NodeReach( void );
	BOOL ShouldGoToNode( void );

	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void LayHeadcrab( void );

	int GetNodeSequence(void);
	int GetNodePresequence(void);

	float GetNodeDelay(void);
	float GetNodeRange(void);
	float GetNodeYaw(void);
	
	// Restart the crab count on each new level
	void OverrideReset(void);

	void DeathNotice( entvars_t *pevChild );

	BOOL CanLayCrab(void);

	void LaunchMortar( void );

	void SetObjectCollisionBox(void);

	BOOL CheckMeleeAttack1( float flDot, float flDist );	// Slash
	BOOL CheckMeleeAttack2( float flDot, float flDist );	// Lay a crab
	BOOL CheckRangeAttack1( float flDot, float flDist );	// Mortar launch

	static const char *pChildDieSounds[];
	static const char *pSackSounds[];
	static const char *pDeathSounds[];
	static const char *pAttackSounds[];
	static const char *pAttackHitSounds[];
	static const char *pBirthSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pFootSounds[];

	CUSTOM_SCHEDULES;
	DECLARE_DATADESC();
private:
	float	m_nodeTime;
	float	m_crabTime;
	float	m_mortarTime;
	float	m_painSoundTime;
	int	m_crabCount;
};
