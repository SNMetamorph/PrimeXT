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
#include "env_sprite.h"

#define N_SCALE		15
#define N_SPHERES	20

class CNihilanth : public CBaseMonster
{
	DECLARE_CLASS( CNihilanth, CBaseMonster );
public:
	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );
	int Classify( void ) { return CLASS_ALIEN_MILITARY; };
	int BloodColor( void ) { return BLOOD_COLOR_YELLOW; }
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );

	void SetObjectCollisionBox( void );

	void HandleAnimEvent( MonsterEvent_t *pEvent );

	void StartupThink( void );
	void HuntThink( void );
	void CrashTouch( CBaseEntity *pOther );
	void DyingThink( void );
	void StartupUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void NullThink( void );
	void CommandUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void FloatSequence( void );
	void NextActivity( void );

	void Flight( void );

	BOOL AbsorbSphere( void );
	BOOL EmitSphere( void );
	void TargetSphere( USE_TYPE useType, float value );
	CBaseEntity *RandomTargetname( const char *szName );
	void ShootBalls( void );
	void MakeFriend( Vector vecPos );
	
	int  TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);

	void PainSound( void );
	void DeathSound( void );

	static const char *pAttackSounds[];	// vocalization: play sometimes when he launches an attack
	static const char *pBallSounds[];	// the sound of the lightening ball launch
	static const char *pShootSounds[];	// grunting vocalization: play sometimes when he launches an attack
	static const char *pRechargeSounds[];	// vocalization: play when he recharges
	static const char *pLaughSounds[];	// vocalization: play sometimes when hit and still has lots of health
	static const char *pPainSounds[];	// vocalization: play sometimes when hit and has much less health and no more chargers
	static const char *pDeathSounds[];	// vocalization: play as he dies
	
	// x_teleattack1.wav	the looping sound of the teleport attack ball.

	float m_flForce;

	float m_flNextPainSound;

	Vector m_velocity;
	Vector m_avelocity;

	Vector m_vecTarget;
	Vector m_posTarget;

	Vector m_vecDesired;
	Vector m_posDesired;

	float  m_flMinZ;
	float  m_flMaxZ;

	Vector m_vecGoal;

	float m_flLastSeen;
	float m_flPrevSeen;

	int m_irritation;

	int m_iLevel;
	int m_iTeleport;

	EHANDLE m_hRecharger;

	EHANDLE m_hSphere[N_SPHERES];
	int	m_iActiveSpheres;

	float m_flAdj;

	CSprite *m_pBall;

	char m_szRechargerTarget[64];
	char m_szDrawUse[64];
	char m_szTeleportUse[64];
	char m_szTeleportTouch[64];
	char m_szDeadUse[64];
	char m_szDeadTouch[64];

	float m_flShootEnd;
	float m_flShootTime;

	EHANDLE m_hFriend[3];
};