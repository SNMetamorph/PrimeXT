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
#include	"soundent.h"
#include	"decals.h"
#include	"explode.h"
#include	"func_break.h"

//=========================================================
// Gargantua Monster
//=========================================================
const float GARG_ATTACKDIST = 80.0;

// Garg animation events
#define GARG_AE_SLASH_LEFT			1
//#define GARG_AE_BEAM_ATTACK_RIGHT		2	// No longer used
#define GARG_AE_LEFT_FOOT			3
#define GARG_AE_RIGHT_FOOT			4
#define GARG_AE_STOMP			5
#define GARG_AE_BREATHE			6

// Gargantua is immune to any damage but this
#define GARG_DAMAGE			(DMG_ENERGYBEAM|DMG_CRUSH|DMG_MORTAR|DMG_BLAST)
#define GARG_EYE_SPRITE_NAME		"sprites/gargeye1.spr"
#define GARG_BEAM_SPRITE_NAME		"sprites/xbeam3.spr"
#define GARG_BEAM_SPRITE2		"sprites/xbeam3.spr"
#define GARG_FLAME_LENGTH		330
#define GARG_GIB_MODEL		"models/metalplategibs.mdl"

#define ATTN_GARG			(ATTN_NORM)

class CGargantua : public CBaseMonster
{
	DECLARE_CLASS( CGargantua, CBaseMonster );
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void HandleAnimEvent( MonsterEvent_t *pEvent );

	BOOL CheckMeleeAttack1( float flDot, float flDist );		// Swipe
	BOOL CheckMeleeAttack2( float flDot, float flDist );		// Flames
	BOOL CheckRangeAttack1( float flDot, float flDist );		// Stomp attack
	void SetObjectCollisionBox( void )
	{
		pev->absmin = GetAbsOrigin() + Vector( -80, -80, 0 );
		pev->absmax = GetAbsOrigin() + Vector( 80, 80, 214 );
	}

	Schedule_t *GetScheduleOfType( int Type );
	void StartTask( Task_t *pTask );
	void RunTask( Task_t *pTask );

	void PrescheduleThink( void );

	void Killed( entvars_t *pevAttacker, int iGib );
	void DeathEffect( void );

	void EyeOff( void );
	void EyeOn( int level );
	void EyeUpdate( void );
	void Leap( void );
	void StompAttack( void );
	void FlameCreate( void );
	void FlameUpdate( void );
	void FlameControls( float angleX, float angleY );
	void FlameDestroy( void );
	inline BOOL FlameIsOn( void ) { return m_pFlame[0] != NULL; }

	void FlameDamage( Vector vecStart, Vector vecEnd, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType );

	DECLARE_DATADESC();
	CUSTOM_SCHEDULES;

private:
	static const char *pAttackHitSounds[];
	static const char *pBeamAttackSounds[];
	static const char *pAttackMissSounds[];
	static const char *pRicSounds[];
	static const char *pFootSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackSounds[];
	static const char *pStompSounds[];
	static const char *pBreatheSounds[];

	CBaseEntity* GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType);

	CSprite		*m_pEyeGlow;		// Glow around the eyes
	CBeam		*m_pFlame[4];		// Flame beams

	int		m_eyeBrightness;		// Brightness target
	float		m_seeTime;		// Time to attack (when I see the enemy, I set this)
	float		m_flameTime;		// Time of next flame attack
	float		m_painSoundTime;		// Time of next pain sound
	float		m_streakTime;		// streak timer (don't send too many)
	float		m_flameX;			// Flame thrower aim
	float		m_flameY;			
};

void SpawnExplosion( Vector center, float randomRange, float time, int magnitude );

const char *CGargantua::pAttackHitSounds[] = 
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CGargantua::pBeamAttackSounds[] = 
{
	"garg/gar_flameoff1.wav",
	"garg/gar_flameon1.wav",
	"garg/gar_flamerun1.wav",
};


const char *CGargantua::pAttackMissSounds[] = 
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CGargantua::pRicSounds[] = 
{
#if 0
	"weapons/ric1.wav",
	"weapons/ric2.wav",
	"weapons/ric3.wav",
	"weapons/ric4.wav",
	"weapons/ric5.wav",
#else
	"debris/metal4.wav",
	"debris/metal6.wav",
	"weapons/ric4.wav",
	"weapons/ric5.wav",
#endif
};

const char *CGargantua::pFootSounds[] = 
{
	"garg/gar_step1.wav",
	"garg/gar_step2.wav",
};


const char *CGargantua::pIdleSounds[] = 
{
	"garg/gar_idle1.wav",
	"garg/gar_idle2.wav",
	"garg/gar_idle3.wav",
	"garg/gar_idle4.wav",
	"garg/gar_idle5.wav",
};


const char *CGargantua::pAttackSounds[] = 
{
	"garg/gar_attack1.wav",
	"garg/gar_attack2.wav",
	"garg/gar_attack3.wav",
};

const char *CGargantua::pAlertSounds[] = 
{
	"garg/gar_alert1.wav",
	"garg/gar_alert2.wav",
	"garg/gar_alert3.wav",
};

const char *CGargantua::pPainSounds[] = 
{
	"garg/gar_pain1.wav",
	"garg/gar_pain2.wav",
	"garg/gar_pain3.wav",
};

const char *CGargantua::pStompSounds[] = 
{
	"garg/gar_stomp1.wav",
};

const char *CGargantua::pBreatheSounds[] = 
{
	"garg/gar_breathe1.wav",
	"garg/gar_breathe2.wav",
	"garg/gar_breathe3.wav",
};

// HACKHACK Cut and pasted from explode.cpp
void SpawnExplosion( Vector center, float randomRange, float time, int magnitude )
{
	KeyValueData	kvd;
	char			buf[128];

	center.x += RANDOM_FLOAT( -randomRange, randomRange );
	center.y += RANDOM_FLOAT( -randomRange, randomRange );

	CBaseEntity *pExplosion = CBaseEntity::Create( "env_explosion", center, g_vecZero, NULL );
	sprintf( buf, "%3d", magnitude );
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = buf;
	pExplosion->KeyValue( &kvd );
	pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;

	pExplosion->Spawn();
	pExplosion->SetThink( &CBaseEntity::SUB_CallUseToggle );
	pExplosion->pev->nextthink = gpGlobals->time + time;
}
