/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "effects.h"
#include "env_explosion.h"
#include "monsters.h"
#include "func_tankcontrols.h"

#define SF_TANK_ACTIVE		0x0001

#define SF_TANK_LINEOFSIGHT		0x0010
#define SF_TANK_CANCONTROL		0x0020	// g-cont. can't activate tank if set for now

// g-cont. reserve 1 bit for me
#define SF_TANK_MATCHTARGET		0x0080
#define SF_TANK_SOUNDON		0x8000

#define MAX_FIRING_SPREADS		ARRAYSIZE( gTankSpread )

static Vector gTankSpread[] =
{
	Vector( 0, 0, 0 ),		// perfect
	Vector( 0.025, 0.025, 0.025 ),// small cone
	Vector( 0.05, 0.05, 0.05 ),	// medium cone
	Vector( 0.1, 0.1, 0.1 ),	// large cone
	Vector( 0.25, 0.25, 0.25 ),	// extra-large cone
};

enum TANKBULLET
{
	TANK_BULLET_NONE = 0,
	TANK_BULLET_9MM = 1,
	TANK_BULLET_MP5 = 2,
	TANK_BULLET_12MM = 3,
	TANK_BULLET_OTHER = 4,	// just for difference between pacific and armed tanks
};

class CBaseTank : public CBaseDelay
{
	DECLARE_CLASS( CBaseTank, CBaseDelay );
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Think( void );
	void	TrackTarget( void );

	void TryFire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	virtual void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	virtual Vector UpdateTargetPosition( CBaseEntity *pTarget )
	{
		// kill the player aiming jitter for scripted tanks
		if( m_bulletType == TANK_BULLET_NONE && pTarget->IsPlayer( ))
			return pTarget->Center() * 0.25f + pTarget->EyePosition() * 0.75f;
		return pTarget->BodyTarget( GetAbsOrigin() );
	}

	void StartRotSound( void );
	void StopRotSound( void );

	// Bmodels don't go across transitions
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_HOLD_ANGLES; }

	inline BOOL IsActive( void ) { return (pev->spawnflags & SF_TANK_ACTIVE) ? TRUE : FALSE; }
	inline void TankActivate( void ) { pev->spawnflags |= SF_TANK_ACTIVE; SetNextThink( 0.1f ); m_iState = STATE_ON; m_fireLast = 0; }
	inline void TankDeactivate( void ) { pev->spawnflags &= ~SF_TANK_ACTIVE; m_fireLast = 0; m_iState = STATE_OFF; StopRotSound(); }
	inline BOOL CanFire( void ) { return (gpGlobals->time - m_lastSightTime) < m_persist; }
	BOOL InRange( float range );

	// Acquire a target. pPlayer is a player in the PVS
	CBaseEntity *FindTarget( void );

	void TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, TraceResult &tr );

	Vector BarrelPosition( void )
	{
		matrix4x4	entity = EntityToWorldTransform();
		return entity.VectorTransform( m_barrelPos );
	}

	void AdjustAnglesForBarrel( Vector &angles, float distance );
	int IRelationship( CBaseEntity* pTarget );

	DECLARE_DATADESC();

	BOOL StartControl( CBasePlayer* pController, CFuncTankControls* pControls );
	void StopControl( CFuncTankControls* pControls );

	CFuncTankControls	*m_pControls;	// tankcontrols is used as a go-between.

	virtual BOOL IsTank( void ) { return TRUE; }

protected:
	float		m_flNextAttack;
	
	float		m_yawCenter;	// "Center" yaw
	float		m_yawRate;	// Max turn rate to track targets
	float		m_yawRange;	// Range of turning motion (one-sided: 30 is +/- 30 degress from center)
					// Zero is full rotation
	float		m_yawTolerance;	// Tolerance angle

	float		m_pitchCenter;	// "Center" pitch
	float		m_pitchRate;	// Max turn rate on pitch
	float		m_pitchRange;	// Range of pitch motion as above
	float		m_pitchTolerance;	// Tolerance angle

	float		m_fireLast;	// Last time I fired
	float		m_fireRate;	// How many rounds/second
	float		m_lastSightTime;	// Last time I saw target
	float		m_persist;	// Persistence of firing (how long do I shoot when I can't see)
	float		m_minRange;	// Minimum range to aim/track
	float		m_maxRange;	// Max range to aim/track

	Vector		m_barrelPos;	// Length of the freakin barrel
	float		m_spriteScale;	// Scale of any sprites we shoot
	int		m_iszSpriteSmoke;
	int		m_iszSpriteFlash;
	TANKBULLET	m_bulletType;	// Bullet type
	int		m_iBulletDamage;	// 0 means use Bullet type's default damage
	
	Vector		m_sightOrigin;	// Last sight of target
	int		m_spread;		// firing spread
	int		m_iszMaster;	// Master entity (game_team_master or multisource)
	int		m_iszFireMaster;
	string_t		m_iszFireTarget;

	int		m_iTankClass;	// Behave As
};
