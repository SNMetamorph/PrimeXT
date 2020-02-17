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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "effects.h"

extern DLL_GLOBAL int		g_iSkillLevel;

#define SF_WAITFORTRIGGER	(0x04 | 0x40) // UNDONE: Fix!
#define SF_NOWRECKAGE		0x08

class CApache : public CBaseMonster
{
	DECLARE_CLASS( CApache, CBaseMonster );

	void Spawn( void );
	void Precache( void );
	int  Classify( void ) { return m_iClass ? m_iClass : CLASS_HUMAN_MILITARY; };
	int  BloodColor( void ) { return DONT_BLEED; }
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );

	void SetObjectCollisionBox( void )
	{
		pev->absmin = GetAbsOrigin() + Vector( -300, -300, -172 );
		pev->absmax = GetAbsOrigin() + Vector(  300,  300,  8 );
	}

	DECLARE_DATADESC();

	void HuntThink( void );
	void FlyTouch( CBaseEntity *pOther );
	void CrashTouch( CBaseEntity *pOther );
	void DyingThink( void );
	void StartupUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void NullThink( void );

	void ShowDamage( void );
	void Flight( void );
	void FireRocket( void );
	BOOL FireGun( void );
	
	int  TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);

	int m_iRockets;
	float m_flForce;
	float m_flNextRocket;

	Vector m_vecTarget;
	Vector m_posTarget;

	Vector m_vecDesired;
	Vector m_posDesired;

	Vector m_vecGoal;

	Vector m_angGun;
	float m_flLastSeen;
	float m_flPrevSeen;

	int m_iSoundState; // don't save this

	int m_iSpriteTexture;
	int m_iExplode;
	int m_iBodyGibs;

	float m_flGoalSpeed;

	int m_iDoSmokePuff;
	CBeam *m_pBeam;
};
LINK_ENTITY_TO_CLASS( monster_apache, CApache );

BEGIN_DATADESC( CApache )
	DEFINE_FIELD( m_iRockets, FIELD_INTEGER ),
	DEFINE_FIELD( m_flForce, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextRocket, FIELD_TIME ),
	DEFINE_FIELD( m_vecTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_posTarget, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecDesired, FIELD_VECTOR ),
	DEFINE_FIELD( m_posDesired, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecGoal, FIELD_VECTOR ),
	DEFINE_FIELD( m_angGun, FIELD_VECTOR ),
	DEFINE_FIELD( m_flLastSeen, FIELD_TIME ),
	DEFINE_FIELD( m_flPrevSeen, FIELD_TIME ),
	DEFINE_FIELD( m_pBeam, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_flGoalSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_iDoSmokePuff, FIELD_INTEGER ),
	DEFINE_FUNCTION( HuntThink ),
	DEFINE_FUNCTION( FlyTouch ),
	DEFINE_FUNCTION( CrashTouch ),
	DEFINE_FUNCTION( DyingThink ),
	DEFINE_FUNCTION( StartupUse ),
	DEFINE_FUNCTION( NullThink ),
END_DATADESC()


void CApache :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/apache.mdl");
	UTIL_SetSize( pev, Vector( -32, -32, -64 ), Vector( 32, 32, 0 ) );
	RelinkEntity( TRUE );

	pev->flags |= FL_MONSTER;
	pev->takedamage		= DAMAGE_AIM;
	pev->health			= gSkillData.apacheHealth;

	m_flFieldOfView = -0.707; // 270 degrees

	pev->sequence = 0;
	ResetSequenceInfo( );
	pev->frame = RANDOM_LONG(0, 0xFF);

	InitBoneControllers();

	if (pev->spawnflags & SF_WAITFORTRIGGER)
	{
		SetUse( &CApache::StartupUse );
	}
	else
	{
		SetThink( &CApache::HuntThink );
		SetTouch( &CApache::FlyTouch );
		pev->nextthink = gpGlobals->time + 1.0;
	}

	m_iRockets = 10;
}


void CApache::Precache( void )
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/apache.mdl");

	PRECACHE_SOUND("apache/ap_rotor1.wav");
	PRECACHE_SOUND("apache/ap_rotor2.wav");
	PRECACHE_SOUND("apache/ap_rotor3.wav");
	PRECACHE_SOUND("apache/ap_whine1.wav");

	PRECACHE_SOUND("weapons/mortarhit.wav");

	m_iSpriteTexture = PRECACHE_MODEL( "sprites/white.spr" );

	PRECACHE_SOUND("turret/tu_fire1.wav");

	PRECACHE_MODEL("sprites/lgtning.spr");

	m_iExplode	= PRECACHE_MODEL( "sprites/fexplo.spr" );
	m_iBodyGibs = PRECACHE_MODEL( "models/metalplategibs_green.mdl" );

	UTIL_PrecacheOther( "hvr_rocket" );
}



void CApache::NullThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.5;
}


void CApache::StartupUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CApache::HuntThink );
	SetTouch( &CApache::FlyTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	SetUse( NULL );
}

void CApache :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.3;

	STOP_SOUND( ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav" );

	UTIL_SetSize( pev, Vector( -32, -32, -64), Vector( 32, 32, 0) );
	SetThink( &CApache::DyingThink );
	SetTouch( &CApache::CrashTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;

	if (pev->spawnflags & SF_NOWRECKAGE)
	{
		m_flNextRocket = gpGlobals->time + 4.0;
	}
	else
	{
		m_flNextRocket = gpGlobals->time + 15.0;
	}
}

void CApache :: DyingThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	SetLocalAvelocity( GetLocalAvelocity() * 1.02 );

	// still falling?
	if (m_flNextRocket > gpGlobals->time )
	{
		Vector vecOrigin = GetAbsOrigin();
		Vector vecVelocity = GetAbsVelocity();

		// random explosions
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_EXPLOSION);		// This just makes a dynamic light now
			WRITE_COORD( vecOrigin.x + RANDOM_FLOAT( -150, 150 ));
			WRITE_COORD( vecOrigin.y + RANDOM_FLOAT( -150, 150 ));
			WRITE_COORD( vecOrigin.z + RANDOM_FLOAT( -150, -50 ));
			WRITE_SHORT( g_sModelIndexFireball );
			WRITE_BYTE( RANDOM_LONG( 0, 29 ) + 30  ); // scale * 10
			WRITE_BYTE( 12  ); // framerate
			WRITE_BYTE( TE_EXPLFLAG_NONE );
		MESSAGE_END();

		// lots of smoke
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecOrigin.x + RANDOM_FLOAT( -150, 150 ));
			WRITE_COORD( vecOrigin.y + RANDOM_FLOAT( -150, 150 ));
			WRITE_COORD( vecOrigin.z + RANDOM_FLOAT( -150, -50 ));
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 100 ); // scale * 10
			WRITE_BYTE( 10  ); // framerate
		MESSAGE_END();

		Vector vecSpot = vecOrigin + (pev->mins + pev->maxs) * 0.5;
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_BREAKMODEL);

			// position
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z );

			// size
			WRITE_COORD( 400 );
			WRITE_COORD( 400 );
			WRITE_COORD( 132 );

			// velocity
			WRITE_COORD( vecVelocity.x ); 
			WRITE_COORD( vecVelocity.y );
			WRITE_COORD( vecVelocity.z );

			// randomization
			WRITE_BYTE( 50 ); 

			// Model
			WRITE_SHORT( m_iBodyGibs );	//model id#

			// # of shards
			WRITE_BYTE( 4 );	// let client decide

			// duration
			WRITE_BYTE( 30 );// 3.0 seconds

			// flags

			WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();

		// don't stop it we touch a entity
		pev->flags &= ~FL_ONGROUND;
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}
	else
	{
		Vector vecOrigin = GetAbsOrigin();
		Vector vecSpot = vecOrigin + (pev->mins + pev->maxs) * 0.5;

		/*
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_EXPLOSION);		// This just makes a dynamic light now
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z + 300 );
			WRITE_SHORT( g_sModelIndexFireball );
			WRITE_BYTE( 250 ); // scale * 10
			WRITE_BYTE( 8  ); // framerate
		MESSAGE_END();
		*/

		// fireball
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z + 256 );
			WRITE_SHORT( m_iExplode );
			WRITE_BYTE( 120 ); // scale * 10
			WRITE_BYTE( 255 ); // brightness
		MESSAGE_END();
		
		// big smoke
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z + 512 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 250 ); // scale * 10
			WRITE_BYTE( 5  ); // framerate
		MESSAGE_END();

		// blast circle
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_BEAMCYLINDER );
			WRITE_COORD( vecOrigin.x);
			WRITE_COORD( vecOrigin.y);
			WRITE_COORD( vecOrigin.z);
			WRITE_COORD( vecOrigin.x);
			WRITE_COORD( vecOrigin.y);
			WRITE_COORD( vecOrigin.z + 2000 ); // reach damage radius over .2 seconds
			WRITE_SHORT( m_iSpriteTexture );
			WRITE_BYTE( 0 ); // startframe
			WRITE_BYTE( 0 ); // framerate
			WRITE_BYTE( 4 ); // life
			WRITE_BYTE( 32 );  // width
			WRITE_BYTE( 0 );   // noise
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 192 );   // r, g, b
			WRITE_BYTE( 128 ); // brightness
			WRITE_BYTE( 0 );		// speed
		MESSAGE_END();

		EMIT_SOUND(ENT(pev), CHAN_STATIC, "weapons/mortarhit.wav", 1.0, 0.3);

		RadiusDamage( vecOrigin, pev, pev, 300, CLASS_NONE, DMG_BLAST );

		if (/*!(pev->spawnflags & SF_NOWRECKAGE) && */(pev->flags & FL_ONGROUND))
		{
			CBaseEntity *pWreckage = Create( "cycler_wreckage", vecOrigin, GetAbsAngles() );
			// SET_MODEL( ENT(pWreckage->pev), STRING(pev->model) );
			UTIL_SetSize( pWreckage->pev, Vector( -200, -200, -128 ), Vector( 200, 200, -32 ) );
			pWreckage->pev->frame = pev->frame;
			pWreckage->pev->sequence = pev->sequence;
			pWreckage->pev->framerate = 0;
			pWreckage->pev->dmgtime = gpGlobals->time + 5;
		}

		// gibs
		vecSpot = vecOrigin + (pev->mins + pev->maxs) * 0.5;
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
			WRITE_BYTE( TE_BREAKMODEL);

			// position
			WRITE_COORD( vecSpot.x );
			WRITE_COORD( vecSpot.y );
			WRITE_COORD( vecSpot.z + 64);

			// size
			WRITE_COORD( 400 );
			WRITE_COORD( 400 );
			WRITE_COORD( 128 );

			// velocity
			WRITE_COORD( 0 ); 
			WRITE_COORD( 0 );
			WRITE_COORD( 200 );

			// randomization
			WRITE_BYTE( 30 ); 

			// Model
			WRITE_SHORT( m_iBodyGibs );	//model id#

			// # of shards
			WRITE_BYTE( 200 );

			// duration
			WRITE_BYTE( 200 );// 10.0 seconds

			// flags

			WRITE_BYTE( BREAK_METAL );
		MESSAGE_END();

		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}


void CApache::FlyTouch( CBaseEntity *pOther )
{
	// bounce if we hit something solid
	if ( pOther->pev->solid == SOLID_BSP) 
	{
		TraceResult tr = UTIL_GetGlobalTrace( );

		// UNDONE, do a real bounce
		SetAbsVelocity( GetAbsVelocity() + tr.vecPlaneNormal * (GetAbsVelocity().Length() + 200 ));
	}
}


void CApache::CrashTouch( CBaseEntity *pOther )
{
	// only crash if we hit something solid
	if ( pOther->pev->solid == SOLID_BSP) 
	{
		SetTouch( NULL );
		m_flNextRocket = gpGlobals->time;
		pev->nextthink = gpGlobals->time;
	}
}



void CApache :: GibMonster( void )
{
	// EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/bodysplat.wav", 0.75, ATTN_NORM, 0, 200);		
}


void CApache :: HuntThink( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	ShowDamage( );

	if ( m_pGoalEnt == NULL && !FStringNull(pev->target) )// this monster has a target
	{
		m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
		if (m_pGoalEnt)
		{
			m_posDesired = m_pGoalEnt->GetAbsOrigin();
			UTIL_MakeVectors( m_pGoalEnt->GetAbsAngles() );
			m_vecGoal = gpGlobals->v_forward;
		}
	}

	// if (m_hEnemy == NULL)
	{
		Look( 4092 );
		m_hEnemy = BestVisibleEnemy( );
	}

	// generic speed up
	if (m_flGoalSpeed < 800)
		m_flGoalSpeed += 5;

	if (m_hEnemy != NULL)
	{
		// ALERT( at_console, "%s\n", STRING( m_hEnemy->pev->classname ) );
		if (FVisible( m_hEnemy ))
		{
			if (m_flLastSeen < gpGlobals->time - 5)
				m_flPrevSeen = gpGlobals->time;
			m_flLastSeen = gpGlobals->time;
			m_posTarget = m_hEnemy->Center( );
		}
		else
		{
			m_hEnemy = NULL;
		}
	}

	m_vecTarget = (m_posTarget - GetAbsOrigin()).Normalize();

	float flLength = (GetAbsOrigin() - m_posDesired).Length();

	if (m_pGoalEnt)
	{
		// ALERT( at_console, "%.0f\n", flLength );

		if (flLength < 128)
		{
			m_pGoalEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_pGoalEnt->pev->target ) );
			if (m_pGoalEnt)
			{
				m_posDesired = m_pGoalEnt->GetAbsOrigin();
				UTIL_MakeVectors( m_pGoalEnt->GetAbsAngles() );
				m_vecGoal = gpGlobals->v_forward;
				flLength = (GetAbsOrigin() - m_posDesired).Length();
			}
		}
	}
	else
	{
		m_posDesired = GetAbsOrigin();
	}

	if (flLength > 250) // 500
	{
		// float flLength2 = (m_posTarget - GetAbsOrigin()).Length() * (1.5 - DotProduct((m_posTarget - GetAbsOrigin()).Normalize(), GetAbsVelocity().Normalize() ));
		// if (flLength2 < flLength)
		if (m_flLastSeen + 90 > gpGlobals->time && DotProduct( (m_posTarget - GetAbsOrigin()).Normalize(), (m_posDesired - GetAbsOrigin()).Normalize( )) > 0.25)
		{
			m_vecDesired = (m_posTarget - GetAbsOrigin()).Normalize( );
		}
		else
		{
			m_vecDesired = (m_posDesired - GetAbsOrigin()).Normalize( );
		}
	}
	else
	{
		m_vecDesired = m_vecGoal;
	}

	Flight( );

	// ALERT( at_console, "%.0f %.0f %.0f\n", gpGlobals->time, m_flLastSeen, m_flPrevSeen );
	if ((m_flLastSeen + 1 > gpGlobals->time) && (m_flPrevSeen + 2 < gpGlobals->time))
	{
		if (FireGun( ))
		{
			// slow down if we're fireing
			if (m_flGoalSpeed > 400)
				m_flGoalSpeed = 400;
		}

		// don't fire rockets and gun on easy mode
		if (g_iSkillLevel == SKILL_EASY)
			m_flNextRocket = gpGlobals->time + 10.0;
	}

	UTIL_MakeVectors( GetAbsAngles() );
	Vector vecEst = (gpGlobals->v_forward * 800 + GetAbsVelocity()).Normalize( );
	// ALERT( at_console, "%d %d %d %4.2f\n", GetAbsAngles().x < 0, DotProduct( GetAbsVelocity(), gpGlobals->v_forward ) > -100, m_flNextRocket < gpGlobals->time, DotProduct( m_vecTarget, vecEst ) );

	if ((m_iRockets % 2) == 1)
	{
		FireRocket( );
		m_flNextRocket = gpGlobals->time + 0.5;
		if (m_iRockets <= 0)
		{
			m_flNextRocket = gpGlobals->time + 10;
			m_iRockets = 10;
		}
	}
	else if (DotProduct( GetAbsVelocity(), gpGlobals->v_forward ) > -100 && m_flNextRocket < gpGlobals->time)
	{
		if (m_flLastSeen + 60 > gpGlobals->time)
		{
			if (m_hEnemy != NULL)
			{
				// make sure it's a good shot
				if (DotProduct( m_vecTarget, vecEst) > .965)
				{
					TraceResult tr;
					
					UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecEst * 4096, ignore_monsters, edict(), &tr );
					if ((tr.vecEndPos - m_posTarget).Length() < 512)
						FireRocket( );
				}
			}
			else
			{
				TraceResult tr;
				
				UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecEst * 4096, dont_ignore_monsters, edict(), &tr );
				// just fire when close
				if ((tr.vecEndPos - m_posTarget).Length() < 512)
					FireRocket( );
			}
		}
	}
}


void CApache :: Flight( void )
{
	// tilt model 5 degrees
	Vector vecAdj = Vector( 5.0, 0, 0 );

	// estimate where I'll be facing in one seconds
	UTIL_MakeVectors( GetAbsAngles() + GetLocalAvelocity() * 2 + vecAdj);
	// Vector vecEst1 = GetAbsOrigin() + GetAbsVelocity() + gpGlobals->v_up * m_flForce - Vector( 0, 0, 384 );
	// float flSide = DotProduct( m_posDesired - vecEst1, gpGlobals->v_right );
	
	float flSide = DotProduct( m_vecDesired, gpGlobals->v_right );

	Vector avelocity = GetLocalAvelocity();

	if (flSide < 0)
	{
		if (avelocity.y < 60)
		{
			avelocity.y += 8; // 9 * (3.0/2.0);
		}
	}
	else
	{
		if (avelocity.y > -60)
		{
			avelocity.y -= 8; // 9 * (3.0/2.0);
		}
	}
	avelocity.y *= 0.98;

	// estimate where I'll be in two seconds
	UTIL_MakeVectors( GetAbsAngles() + avelocity * 1 + vecAdj);
	Vector vecEst = GetAbsOrigin() + GetAbsVelocity() * 2.0 + gpGlobals->v_up * m_flForce * 20 - Vector( 0, 0, 384 * 2 );

	// add immediate force
	UTIL_MakeVectors( GetAbsAngles() + vecAdj);
	Vector velocity = GetAbsVelocity();
	velocity.x += gpGlobals->v_up.x * m_flForce;
	velocity.y += gpGlobals->v_up.y * m_flForce;
	velocity.z += gpGlobals->v_up.z * m_flForce;
	// add gravity
	velocity.z -= 38.4; // 32ft/sec

	SetAbsVelocity( velocity );

	float flSpeed = GetAbsVelocity().Length();
	float flDir = DotProduct( Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, 0 ), Vector( GetAbsVelocity().x, GetAbsVelocity().y, 0 ) );
	if (flDir < 0)
		flSpeed = -flSpeed;

	float flDist = DotProduct( m_posDesired - vecEst, gpGlobals->v_forward );

	// float flSlip = DotProduct( GetAbsVelocity(), gpGlobals->v_right );
	float flSlip = -DotProduct( m_posDesired - vecEst, gpGlobals->v_right );

	// fly sideways
	if (flSlip > 0)
	{
		if (GetAbsAngles().z > -30 && avelocity.z > -15)
			avelocity.z -= 4;
		else
			avelocity.z += 2;
	}
	else
	{

		if (GetAbsAngles().z < 30 && avelocity.z < 15)
			avelocity.z += 4;
		else
			avelocity.z -= 2;
	}

	// sideways drag
	velocity = GetAbsVelocity();
	velocity.x = velocity.x * (1.0 - fabs( gpGlobals->v_right.x ) * 0.05);
	velocity.y = velocity.y * (1.0 - fabs( gpGlobals->v_right.y ) * 0.05);
	velocity.z = velocity.z * (1.0 - fabs( gpGlobals->v_right.z ) * 0.05);

	// general drag
	velocity = velocity * 0.995;

	SetAbsVelocity( velocity );
	
	// apply power to stay correct height
	if (m_flForce < 80 && vecEst.z < m_posDesired.z) 
	{
		m_flForce += 12;
	}
	else if (m_flForce > 30)
	{
		if (vecEst.z > m_posDesired.z) 
			m_flForce -= 8;
	}

	// pitch forward or back to get to target
	if (flDist > 0 && flSpeed < m_flGoalSpeed && GetAbsAngles().x + avelocity.x < 40)
	{
		// ALERT( at_console, "F " );
		// lean forward
		avelocity.x += 12.0;
	}
	else if (flDist < 0 && flSpeed > -50 && GetAbsAngles().x + avelocity.x > -20)
	{
		// ALERT( at_console, "B " );
		// lean backward
		avelocity.x -= 12.0;
	}
	else if (GetAbsAngles().x + avelocity.x < 0)
	{
		// ALERT( at_console, "f " );
		avelocity.x += 4.0;
	}
	else if (GetAbsAngles().x + avelocity.x > 0)
	{
		// ALERT( at_console, "b " );
		avelocity.x -= 4.0;
	}

	// ALERT( at_console, "%.0f %.0f : %.0f %.0f : %.0f %.0f : %.0f\n", GetAbsOrigin().x, GetAbsVelocity().x, flDist, flSpeed, GetAbsAngles().x, avelocity.x, m_flForce ); 
	// ALERT( at_console, "%.0f %.0f : %.0f %0.f : %.0f\n", GetAbsOrigin().z, GetAbsVelocity().z, vecEst.z, m_posDesired.z, m_flForce ); 

	SetLocalAvelocity( avelocity );

	// make rotor, engine sounds
	if (m_iSoundState == 0)
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav", 1.0, 0.3, 0, 110 );
		// EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_whine1.wav", 0.5, 0.2, 0, 110 );

		m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions
	}
	else
	{
		CBaseEntity *pPlayer = NULL;

		pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
		// UNDONE: this needs to send different sounds to every player for multiplayer.	
		if (pPlayer)
		{

			float pitch = DotProduct( GetAbsVelocity() - pPlayer->GetAbsVelocity(), (pPlayer->GetAbsOrigin() - GetAbsOrigin()).Normalize() );

			pitch = (int)(100 + pitch / 50.0);

			if (pitch > 250) 
				pitch = 250;
			if (pitch < 50)
				pitch = 50;
			if (pitch == 100)
				pitch = 101;

			float flVol = (m_flForce / 100.0) + .1;
			if (flVol > 1.0) 
				flVol = 1.0;

			EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_rotor2.wav", 1.0, 0.3, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);
		}
		// EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, "apache/ap_whine1.wav", flVol, 0.2, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);
	
		// ALERT( at_console, "%.0f %.2f\n", pitch, flVol );
	}
}


void CApache :: FireRocket( void )
{
	static float side = 1.0;
	static int count;

	if (m_iRockets <= 0)
		return;

	UTIL_MakeVectors( GetAbsAngles() );
	Vector vecSrc = GetAbsOrigin() + 1.5 * (gpGlobals->v_forward * 21 + gpGlobals->v_right * 70 * side + gpGlobals->v_up * -79);

	switch( m_iRockets % 5)
	{
	case 0:	vecSrc = vecSrc + gpGlobals->v_right * 10; break;
	case 1: vecSrc = vecSrc - gpGlobals->v_right * 10; break;
	case 2: vecSrc = vecSrc + gpGlobals->v_up * 10; break;
	case 3: vecSrc = vecSrc - gpGlobals->v_up * 10; break;
	case 4: break;
	}

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( vecSrc.x );
		WRITE_COORD( vecSrc.y );
		WRITE_COORD( vecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 20 ); // scale * 10
		WRITE_BYTE( 12 ); // framerate
	MESSAGE_END();

	CBaseEntity *pRocket = CBaseEntity::Create( "hvr_rocket", vecSrc, GetAbsAngles(), edict() );
	if (pRocket)
		pRocket->SetAbsVelocity( GetAbsVelocity() + gpGlobals->v_forward * 100 );

	m_iRockets--;

	side = - side;
}



BOOL CApache :: FireGun( )
{
	UTIL_MakeVectors( GetAbsAngles() );
		
	Vector posGun, angGun;
	GetAttachment( 1, posGun, angGun );

	Vector vecTarget = (m_posTarget - posGun).Normalize( );

	Vector vecOut;

	vecOut.x = DotProduct( gpGlobals->v_forward, vecTarget );
	vecOut.y = -DotProduct( gpGlobals->v_right, vecTarget );
	vecOut.z = DotProduct( gpGlobals->v_up, vecTarget );

	Vector angles = UTIL_VecToAngles (vecOut);

	if (angles.y > 180)
		angles.y = angles.y - 360;
	if (angles.y < -180)
		angles.y = angles.y + 360;
	if (angles.x > 180)
		angles.x = angles.x - 360;
	if (angles.x < -180)
		angles.x = angles.x + 360;

	if (angles.x > m_angGun.x)
		m_angGun.x = Q_min( angles.x, m_angGun.x + 12 );
	if (angles.x < m_angGun.x)
		m_angGun.x = Q_max( angles.x, m_angGun.x - 12 );
	if (angles.y > m_angGun.y)
		m_angGun.y = Q_min( angles.y, m_angGun.y + 12 );
	if (angles.y < m_angGun.y)
		m_angGun.y = Q_max( angles.y, m_angGun.y - 12 );

	m_angGun.y = SetBoneController( 0, m_angGun.y );
	m_angGun.x = SetBoneController( 1, m_angGun.x );

	Vector posBarrel, angBarrel;
	GetAttachment( 0, posBarrel, angBarrel );
	Vector vecGun = (posBarrel - posGun).Normalize( );

	if (DotProduct( vecGun, vecTarget ) > 0.98)
	{
#if 1
		FireBullets( 1, posGun, vecGun, VECTOR_CONE_4DEGREES, 8192, BULLET_MONSTER_12MM, 1 );
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "turret/tu_fire1.wav", 1, 0.3);
#else
		static float flNext;
		TraceResult tr;
		UTIL_TraceLine( posGun, posGun + vecGun * 8192, dont_ignore_monsters, ENT( pev ), &tr );

		if (!m_pBeam)
		{
			m_pBeam = CBeam::BeamCreate( "sprites/lgtning.spr", 80 );
			m_pBeam->PointEntInit( GetAbsOrigin(), entindex( ) );
			m_pBeam->SetEndAttachment( 1 );
			m_pBeam->SetColor( 255, 180, 96 );
			m_pBeam->SetBrightness( 192 );
		}

		if (flNext < gpGlobals->time)
		{
			flNext = gpGlobals->time + 0.5;
			m_pBeam->SetStartPos( tr.vecEndPos );
		}
#endif
		return TRUE;
	}
	else
	{
		if (m_pBeam)
		{
			UTIL_Remove( m_pBeam );
			m_pBeam = NULL;
		}
	}
	return FALSE;
}



void CApache :: ShowDamage( void )
{
	if (m_iDoSmokePuff > 0 || RANDOM_LONG(0,99) > pev->health)
	{
		Vector vecOrigin = GetAbsOrigin();
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z - 32 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG(0,9) + 20 ); // scale * 10
			WRITE_BYTE( 12 ); // framerate
		MESSAGE_END();
	}
	if (m_iDoSmokePuff > 0)
		m_iDoSmokePuff--;
}


int CApache :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if (pevInflictor->owner == edict())
		return 0;

	if (bitsDamageType & DMG_BLAST)
	{
		flDamage *= 2;
	}

	/*
	if ( (bitsDamageType & DMG_BULLET) && flDamage > 50)
	{
		// clip bullet damage at 50
		flDamage = 50;
	}
	*/

	// ALERT( at_console, "%.0f\n", flDamage );
	return CBaseEntity::TakeDamage(  pevInflictor, pevAttacker, flDamage, bitsDamageType );
}



void CApache::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	// ALERT( at_console, "%d %.0f\n", ptr->iHitgroup, flDamage );

	// ignore blades
	if (ptr->iHitgroup == 6 && (bitsDamageType & (DMG_ENERGYBEAM|DMG_BULLET|DMG_CLUB)))
		return;

	// hit hard, hits cockpit, hits engines
	if (flDamage > 50 || ptr->iHitgroup == 1 || ptr->iHitgroup == 2)
	{
		// ALERT( at_console, "%.0f\n", flDamage );
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
		m_iDoSmokePuff = 3 + (flDamage / 5.0);
	}
	else
	{
		// do half damage in the body
		// AddMultiDamage( pevAttacker, this, flDamage / 2.0, bitsDamageType );
		UTIL_Ricochet( ptr->vecEndPos, 2.0 );
	}
}





class CApacheHVR : public CGrenade
{
	DECLARE_CLASS( CApacheHVR, CGrenade );
	void Spawn( void );
	void Precache( void );
	void IgniteThink( void );
	void AccelerateThink( void );

	DECLARE_DATADESC();

	int m_iTrail;
	Vector m_vecForward;
};
LINK_ENTITY_TO_CLASS( hvr_rocket, CApacheHVR );

BEGIN_DATADESC( CApacheHVR )
	DEFINE_FIELD( m_vecForward, FIELD_VECTOR ),
	DEFINE_FUNCTION( IgniteThink ),
	DEFINE_FUNCTION( AccelerateThink ),
END_DATADESC()

void CApacheHVR :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/HVR.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	RelinkEntity( TRUE );

	SetThink( &CApacheHVR::IgniteThink );
	SetTouch( &CApacheHVR::ExplodeTouch );

	UTIL_MakeVectors( GetAbsAngles() );
	m_vecForward = gpGlobals->v_forward;
	pev->gravity = 0.5;

	pev->nextthink = gpGlobals->time + 0.1;

	pev->dmg = 150;
}


void CApacheHVR :: Precache( void )
{
	PRECACHE_MODEL("models/HVR.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND ("weapons/rocket1.wav");
}


void CApacheHVR :: IgniteThink( void  )
{
	// pev->movetype = MOVETYPE_TOSS;

	// pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5 );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 15 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	// set to accelerate
	SetThink( &CApacheHVR::AccelerateThink );
	pev->nextthink = gpGlobals->time + 0.1;
}


void CApacheHVR :: AccelerateThink( void  )
{
	// check world boundaries
	if ( !IsInWorld( FALSE ))
	{
		UTIL_Remove( this );
		return;
	}

	// accelerate
	float flSpeed = GetAbsVelocity().Length();
	if (flSpeed < 1800)
	{
		SetAbsVelocity( GetAbsVelocity() + m_vecForward * 200 );
	}

	// re-aim
	SetAbsAngles( UTIL_VecToAngles( GetAbsVelocity() ));

	SetNextThink( 0.1 );
}
