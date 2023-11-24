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
// Houndeye - spooky sonic dog. 
//=========================================================

#pragma once

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"animation.h"
#include	"nodes.h"
#include	"squadmonster.h"
#include	"soundent.h"
#include	"game.h"

// houndeye does 20 points of damage spread over a sphere 384 units in diameter, and each additional 
// squad member increases the BASE damage by 110%, per the spec.
#define HOUNDEYE_MAX_SQUAD_SIZE		4
#define HOUNDEYE_MAX_ATTACK_RADIUS		384
#define HOUNDEYE_SQUAD_BONUS			(float)1.1

#define HOUNDEYE_EYE_FRAMES 4 // how many different switchable maps for the eye

#define HOUNDEYE_SOUND_STARTLE_VOLUME	128 // how loud a sound has to be to badly scare a sleeping houndeye

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_HOUND_CLOSE_EYE = LAST_COMMON_TASK + 1,
	TASK_HOUND_OPEN_EYE,
	TASK_HOUND_THREAT_DISPLAY,
	TASK_HOUND_FALL_ASLEEP,
	TASK_HOUND_WAKE_UP,
	TASK_HOUND_HOP_BACK
};

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_HOUND_AGITATED = LAST_COMMON_SCHEDULE + 1,
	SCHED_HOUND_HOP_RETREAT,
	SCHED_HOUND_FAIL,
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		HOUND_AE_WARN		1
#define		HOUND_AE_STARTATTACK	2
#define		HOUND_AE_THUMP		3
#define		HOUND_AE_ANGERSOUND1	4
#define		HOUND_AE_ANGERSOUND2	5
#define		HOUND_AE_HOPBACK		6
#define		HOUND_AE_CLOSE_EYE		7

class CHoundeye : public CSquadMonster
{
	DECLARE_CLASS( CHoundeye, CSquadMonster );
public:
	void Spawn( void );
	void Precache( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void SetYawSpeed ( void );
	void WarmUpSound ( void );
	void AlertSound( void );
	void DeathSound( void );
	void WarnSound( void );
	void PainSound( void );
	void IdleSound( void );
	void StartTask( Task_t *pTask );
	void RunTask ( Task_t *pTask );
	void SonicAttack( void );
	void PrescheduleThink( void );
	void SetActivity ( Activity NewActivity );
	void WriteBeamColor ( void );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL FValidateHintType ( short sHint );
	BOOL FCanActiveIdle ( void );
	Schedule_t *GetScheduleOfType ( int Type );
	Schedule_t *GetSchedule( void );

	CUSTOM_SCHEDULES;
	DECLARE_DATADESC();

	int m_iSpriteTexture;
	BOOL m_fAsleep;// some houndeyes sleep in idle mode if this is set, the houndeye is lying down
	BOOL m_fDontBlink;// don't try to open/close eye if this bit is set!
	Vector m_vecPackCenter; // the center of the pack. The leader maintains this by averaging the origins of all pack members.
};