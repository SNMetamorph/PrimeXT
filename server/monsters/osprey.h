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

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "customentity.h"

typedef struct 
{
	int isValid;
	EHANDLE hGrunt;
	Vector	vecOrigin;
	Vector  vecAngles;
} t_ospreygrunt;

#define SF_WAITFORTRIGGER	0x40
#define MAX_CARRY	24

class COsprey : public CBaseMonster
{
	DECLARE_CLASS( COsprey, CBaseMonster );
public:
	DECLARE_DATADESC();
	
	int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	
	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return CLASS_MACHINE; };
	int  BloodColor( void ) { return DONT_BLEED; }
	void Killed( entvars_t *pevAttacker, int iGib );

	void UpdateGoal( void );
	BOOL HasDead( void );
	void FlyThink( void );
	void DeployThink( void );
	void Flight( void );
	void HitTouch( CBaseEntity *pOther );
	void FindAllThink( void );
	void HoverThink( void );
	CBaseMonster *MakeGrunt( Vector vecSrc );
	void CrashTouch( CBaseEntity *pOther );
	void DyingThink( void );
	void CommandUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	void ShowDamage( void );

	CBaseEntity *m_pGoalEnt;
	Vector m_vel1;
	Vector m_vel2;
	Vector m_pos1;
	Vector m_pos2;
	Vector m_ang1;
	Vector m_ang2;
	float m_startTime;
	float m_dTime;

	Vector m_velocity;

	float m_flIdealtilt;
	float m_flRotortilt;

	float m_flRightHealth;
	float m_flLeftHealth;

	int m_iUnits;
	EHANDLE m_hGrunt[MAX_CARRY];
	Vector m_vecOrigin[MAX_CARRY];
	EHANDLE m_hRepel[4];

	int m_iSoundState;
	int m_iSpriteTexture;

	int m_iPitch;

	int m_iExplode;
	int m_iTailGibs;
	int m_iBodyGibs;
	int m_iEngineGibs;

	int m_iDoLeftSmokePuff;
	int m_iDoRightSmokePuff;
};