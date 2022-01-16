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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "effects.h"
#include "weapons.h"
#include "explode.h"
#include "monsters.h"
#include "player.h"


#define SF_TANK_ACTIVE		0x0001

#define SF_TANK_LINEOFSIGHT		0x0010
#define SF_TANK_CANCONTROL		0x0020	// g-cont. can't activate tank if set for now

// g-cont. reserve 1 bit for me
#define SF_TANK_MATCHTARGET		0x0080
#define SF_TANK_SOUNDON		0x8000

#define MAX_CONTROLLED_TANKS		48

enum TANKBULLET
{
	TANK_BULLET_NONE = 0,
	TANK_BULLET_9MM = 1,
	TANK_BULLET_MP5 = 2,
	TANK_BULLET_12MM = 3,
	TANK_BULLET_OTHER = 4,	// just for difference between pacific and armed tanks
};

// Custom damage
// env_laser (duration is 0.5 rate of fire)
// rockets
// explosion?

class CFuncTankControls;
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

class CFuncTankControls : public CBaseDelay
{
	DECLARE_CLASS( CFuncTankControls, CBaseDelay );
public:
	virtual int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void HandleTank( CBaseEntity *pActivator, CBaseEntity *m_pTank, BOOL activate );
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );

	DECLARE_DATADESC();

	BOOL OnControls( CBaseEntity *pTest );

	CBasePlayer	*m_pController;
	Vector		m_vecControllerUsePos; // where was the player standing when he used me?
	int		m_iTankName[MAX_CONTROLLED_TANKS]; // list if indexes into global string array
	int		m_cTanks; // the total number of targets in this manager's fire list.
	BOOL		m_fVerifyTanks;
};

BEGIN_DATADESC( CBaseTank )
	DEFINE_KEYFIELD( m_yawRate, FIELD_FLOAT, "yawrate" ),
	DEFINE_KEYFIELD( m_yawRange, FIELD_FLOAT, "yawrange" ),
	DEFINE_KEYFIELD( m_yawTolerance, FIELD_FLOAT, "yawtolerance" ),
	DEFINE_KEYFIELD( m_pitchRate, FIELD_FLOAT, "pitchrate" ),
	DEFINE_KEYFIELD( m_pitchRange, FIELD_FLOAT, "pitchrange" ),
	DEFINE_KEYFIELD( m_pitchTolerance, FIELD_FLOAT, "pitchtolerance" ),
	DEFINE_KEYFIELD( m_fireRate, FIELD_FLOAT, "firerate" ),
	DEFINE_KEYFIELD( m_persist, FIELD_FLOAT, "persistence" ),
	DEFINE_KEYFIELD( m_minRange, FIELD_FLOAT, "minRange" ),
	DEFINE_KEYFIELD( m_maxRange, FIELD_FLOAT, "maxRange" ),
	DEFINE_KEYFIELD( m_spriteScale, FIELD_FLOAT, "spritescale" ),
	DEFINE_KEYFIELD( m_iszSpriteSmoke, FIELD_STRING, "spritesmoke" ),
	DEFINE_KEYFIELD( m_iszSpriteFlash, FIELD_STRING, "spriteflash" ),
	DEFINE_KEYFIELD( m_bulletType, FIELD_INTEGER, "bullet" ),
	DEFINE_KEYFIELD( m_spread, FIELD_INTEGER, "firespread" ),
	DEFINE_KEYFIELD( m_iBulletDamage, FIELD_INTEGER, "bullet_damage" ),
	DEFINE_KEYFIELD( m_iszFireMaster, FIELD_STRING, "firemaster" ),
	DEFINE_KEYFIELD( m_iszFireTarget, FIELD_STRING, "firetarget" ),
	DEFINE_KEYFIELD( m_iTankClass, FIELD_INTEGER, "m_iClass" ),
	DEFINE_FIELD( m_yawCenter, FIELD_FLOAT ),
	DEFINE_FIELD( m_pitchCenter, FIELD_FLOAT ),
	DEFINE_FIELD( m_fireLast, FIELD_TIME ),
	DEFINE_FIELD( m_lastSightTime, FIELD_TIME ),
	DEFINE_FIELD( m_pControls, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_flNextAttack, FIELD_TIME ),
	DEFINE_FIELD( m_sightOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( m_barrelPos, FIELD_VECTOR ),
END_DATADESC()

#define MAX_FIRING_SPREADS		ARRAYSIZE( gTankSpread )

static Vector gTankSpread[] =
{
	Vector( 0, 0, 0 ),		// perfect
	Vector( 0.025, 0.025, 0.025 ),// small cone
	Vector( 0.05, 0.05, 0.05 ),	// medium cone
	Vector( 0.1, 0.1, 0.1 ),	// large cone
	Vector( 0.25, 0.25, 0.25 ),	// extra-large cone
};

void CBaseTank :: Spawn( void )
{
	Precache();

	pev->movetype = MOVETYPE_PUSH;  // so it doesn't get pushed by anything
	pev->solid = SOLID_BSP;
	SET_MODEL( edict(), GetModel() );

	SetLocalAngles( m_vecTempAngles );
	Vector angles = GetLocalAngles();

	m_yawCenter = angles.y;
	m_pitchCenter = angles.x;

	if ( IsActive() )
		SetNextThink( 1.0 );

	m_sightOrigin = BarrelPosition(); // Point at the end of the barrel

	if ( m_fireRate <= 0 )
		m_fireRate = 1;
	if ( m_spread > MAX_FIRING_SPREADS )
		m_spread = 0;

	UTIL_SetOrigin( this, GetLocalOrigin());

	if( !m_pitchTolerance && !m_yawTolerance )
		SetBits( pev->spawnflags, SF_TANK_LINEOFSIGHT );

	// create physic body
	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
}

void CBaseTank :: Precache( void )
{
	if ( m_iszSpriteSmoke )
		PRECACHE_MODEL( (char *)STRING(m_iszSpriteSmoke) );

	if ( m_iszSpriteFlash )
		PRECACHE_MODEL( (char *)STRING(m_iszSpriteFlash) );

	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
}

void CBaseTank :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "yawrate" ))
	{
		m_yawRate = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "yawrange" ))
	{
		m_yawRange = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "yawtolerance" ))
	{
		m_yawTolerance = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "pitchrange" ))
	{
		m_pitchRange = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "pitchrate" ))
	{
		m_pitchRate = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "pitchtolerance" ))
	{
		m_pitchTolerance = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "firerate" ))
	{
		m_fireRate = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "barrel" ))
	{
		m_barrelPos.x = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "barrely" ))
	{
		// HACKHACK: matrix uses the left-side hand
		// but matrices uses the right-side hand
		m_barrelPos.y = -Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "barrelz" ))
	{
		m_barrelPos.z = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "spritescale" ))
	{
		m_spriteScale = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "spritesmoke" ))
	{
		m_iszSpriteSmoke = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "spriteflash" ))
	{
		m_iszSpriteFlash = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "rotatesound" ))
	{
		pev->noise = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "persistence" ))
	{
		m_persist = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "bullet" ))
	{
		m_bulletType = (TANKBULLET)Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "bullet_damage" )) 
	{
		m_iBulletDamage = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "firespread" ))
	{
		m_spread = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "minRange" ))
	{
		m_minRange = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "maxRange" ))
	{
		m_maxRange = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "firemaster" ))
	{
		m_iszFireMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "firetarget" ))
	{
		m_iszFireTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iClass" ))
	{
		m_iTankClass = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

BOOL CBaseTank :: StartControl( CBasePlayer *pController, CFuncTankControls *pControls )
{
	if ( m_pControls != NULL )
		return FALSE;

	// Team only or disabled?
	if ( m_iszMaster )
	{
		if ( !UTIL_IsMasterTriggered( m_iszMaster, pController ) )
			return FALSE;
	}

	ALERT( at_aiconsole, "using TANK!\n");

	m_iState = STATE_ON;
	m_pControls = pControls;
	
	SetNextThink( 0.3 );
	
	return TRUE;
}

void CBaseTank :: StopControl( CFuncTankControls *pControls )
{
	if( !m_pControls || m_pControls != pControls )
	{
		return;
	}

	ALERT( at_aiconsole, "leave TANK!\n");

          m_iState = STATE_OFF;
	StopRotSound();

	DontThink();
	SetLocalAvelocity( g_vecZero );
	m_pControls = NULL;

	if( IsActive( ))
	{
		SetNextThink( 1.0 );
	}
}

void CBaseTank :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( pev->spawnflags & SF_TANK_CANCONTROL )
	{
		// player controlled turret
		if( pActivator->Classify() != CLASS_PLAYER )
			return;

		// g-cont. and ???
	}
	else
	{
		if( !ShouldToggle( useType, IsActive() ))
			return;

		if( IsActive() )
			TankDeactivate();
		else TankActivate();
	}
}

CBaseEntity *CBaseTank :: FindTarget( void )
{
	// normal mode: find the client in PVS
	if( !m_iszFireTarget && !m_iTankClass )
		return CBaseEntity::Instance( FIND_CLIENT_IN_PVS( edict() ));

	// custom mode: find specified entity by targetname or classname
	edict_t *pent = UTIL_EntitiesInPVS( edict() );
	const char *chFireTarget = STRING( m_iszFireTarget );

	Vector barrelEnd = BarrelPosition();
	CBaseEntity *pReturn = NULL;
	int iBestRelationship = R_DL;
	float flDist = 8192, flNearest = 8192; // so first visible entity will become the closest.

	// check all the entities
	while ( !FNullEnt( pent ) )
	{
		CBaseEntity *pTarg = CBaseEntity::Instance( pent );

		if( pTarg )
		{
			if( m_iTankClass )
			{
				if( pTarg->IsAlive() )
				{
					if( IRelationship( pTarg ) > iBestRelationship )
					{
						// this entity is disliked MORE than the entity that we 
						// currently think is the best visible enemy. No need to do 
						// a distance check, just get mad at this one for now.
						iBestRelationship = IRelationship( pTarg );
						flNearest = flDist = ( pTarg->EyePosition() - barrelEnd ).Length();
						pReturn = pTarg;
					}
					else if( IRelationship( pTarg ) == iBestRelationship )
					{
						// this entity is disliked just as much as the entity that
						// we currently think is the best visible enemy, so we only
						// get mad at it if it is closer.
						flDist = ( pTarg->EyePosition() - barrelEnd ).Length();
				
						if( flDist <= flNearest )
						{
							flNearest = flDist;
							// these are guaranteed to be the same!
							pReturn = pTarg;
						}
					}
				}
			}
			else if( FStrEq( pTarg->GetTargetname(), chFireTarget ) || FStrEq( pTarg->GetClassname(), chFireTarget ))
			{
				// calculate angle needed to aim at target
				flDist = ( pTarg->EyePosition() - barrelEnd ).Length();

				if( flDist <= flNearest )
				{
					flNearest = flDist;
					// these are guaranteed to be the same!
					pReturn = pTarg;
				}
			}
		}

		pent = pent->v.chain;
	}

	// make sure what the closest entity is in-range
	if( pReturn != NULL && InRange( flDist ))
	{
//		ALERT( at_console, "Found %s\n", pReturn->GetClassname( ));
		return pReturn;
	}

	return NULL;
}

int CBaseTank :: IRelationship( CBaseEntity* pTarget )
{
	int iOtherClass = pTarget->Classify();

	if( iOtherClass == CLASS_NONE )
		return R_NO;

	if( !m_iTankClass )
	{
		if( iOtherClass == CLASS_PLAYER )
			return R_HT;
		return R_NO;
	}
	else if( m_iTankClass == CLASS_PLAYER_ALLY )
	{
		switch( iOtherClass )
		{
		case CLASS_MACHINE:
		case CLASS_HUMAN_MILITARY:
		case CLASS_ALIEN_MILITARY:
		case CLASS_ALIEN_MONSTER:
		case CLASS_ALIEN_PREDATOR:
		case CLASS_ALIEN_PREY:
			return R_HT;
		default:
			return R_NO;
		}
	}
	else if( m_iTankClass == CLASS_HUMAN_MILITARY )
	{
		switch( iOtherClass )
		{
		case CLASS_PLAYER:
		case CLASS_PLAYER_ALLY:
		case CLASS_ALIEN_MILITARY:
		case CLASS_ALIEN_MONSTER:
		case CLASS_ALIEN_PREDATOR:
		case CLASS_ALIEN_PREY:
			return R_HT;
		case CLASS_HUMAN_PASSIVE:
			return R_DL;
		default:
			return R_NO;
		}
	}
	else if( m_iTankClass == CLASS_ALIEN_MILITARY )
	{
		switch( iOtherClass )
		{
		case CLASS_PLAYER:
		case CLASS_PLAYER_ALLY:
		case CLASS_HUMAN_MILITARY:
			return R_HT;
		case CLASS_HUMAN_PASSIVE:
			return R_DL;
		default:
			return R_NO;
		}
	}

	return R_NO;
}

BOOL CBaseTank :: InRange( float range )
{
	if( range < m_minRange )
		return FALSE;

	if( m_maxRange > 0 && range > m_maxRange )
		return FALSE;

	return TRUE;
}

void CBaseTank :: Think( void )
{
	SetLocalAvelocity( g_vecZero );
	TrackTarget();

	Vector avelocity = GetLocalAvelocity();

	if ( fabs( avelocity.x ) > 1 || fabs( avelocity.y ) > 1 )
		StartRotSound();
	else StopRotSound();
}

void CBaseTank :: TrackTarget( void )
{
	TraceResult tr;
	BOOL updateTime = FALSE, lineOfSight;
	Vector angles, direction, targetPosition, barrelEnd;
	CBasePlayer *pController = NULL;
	CBaseEntity *pTarget;

	// Get a position to aim for
	if( m_pControls && m_pControls->m_pController )
	{
		// Tanks attempt to mirror the player's angles
		pController = m_pControls->m_pController;
		pController->pev->viewmodel = 0;
		SetNextThink( 0.05 );

		if( FBitSet( pev->spawnflags, SF_TANK_MATCHTARGET ))
		{
			// "Match target" mode:
			// first, get the player's angles
			angles = pController->pev->v_angle;
			// Work out what point the player is looking at
			UTIL_MakeVectorsPrivate( angles, direction, NULL, NULL );

			targetPosition = pController->EyePosition() + direction * 1000;

			edict_t *ownerTemp = pev->owner;	// store the owner, so we can put it back after the check
			pev->owner = pController->edict();	// when doing the matchtarget check, don't hit the player or the tank.

			UTIL_TraceLine( pController->EyePosition(), targetPosition, missile, ignore_glass, edict(), &tr );

			pev->owner = ownerTemp; // put the owner back

			// Work out what angle we need to face to look at that point
			direction = tr.vecEndPos - GetAbsOrigin();
			angles = UTIL_VecToAngles( direction );
			targetPosition = tr.vecEndPos;

			// Calculate the additional rotation to point the end of the barrel at the target
			// (instead of the gun's center)
			AdjustAnglesForBarrel( angles, direction.Length() );
		}
		else
		{
			// "Match angles" mode
			// just get the player's angles
			angles = pController->pev->v_angle;
		}
	}
	else
	{
		if( IsActive( ))
		{
			SetNextThink( 0.01 );
		}
		else
		{
			SetLocalAvelocity( g_vecZero );
			m_iState = STATE_OFF;
			DontThink();
			return;
		}

		pTarget = FindTarget();

		if( !pTarget )
		{
			if( IsActive() )
				SetNextThink( 0.1 ); // at rest
			m_iState = STATE_ON;
			return;
		}

		// Calculate angle needed to aim at target
		barrelEnd = BarrelPosition();
		targetPosition = pTarget->EyePosition();
		float range = ( targetPosition - barrelEnd ).Length();
		
		if( !InRange( range ))
		{
			m_iState = STATE_ON;
			return;
                    }

		UTIL_TraceLine( barrelEnd, targetPosition, dont_ignore_monsters, edict(), &tr );
		lineOfSight = FALSE;

		// No line of sight, don't track
		// g-cont. but allow tracking for non-armed tanks because this may be it's projectors or security cameras
		if( tr.flFraction == 1.0 || tr.pHit == pTarget->edict() || m_bulletType == TANK_BULLET_NONE )
		{
			lineOfSight = TRUE;

			if( InRange( range ) && pTarget && pTarget->IsAlive() )
			{
				updateTime = TRUE;
				m_sightOrigin = UpdateTargetPosition( pTarget );
			}
		}

		// Track sight origin

		direction = m_sightOrigin - GetAbsOrigin();
		angles = UTIL_VecToAngles( direction );

		// Calculate the additional rotation to point the end of the barrel at the target (not the gun's center) 
		AdjustAnglesForBarrel( angles, direction.Length() );
	}

	float flYawCenter = m_yawCenter;
	float flPitchCenter = m_pitchCenter;

	// HACKHACK: get the range center as sum of two angles
	// it's not fully properly but it works
	if( m_hParent != NULL && FClassnameIs( m_hParent->pev, "func_tracktrain" ))
	{
		flYawCenter += m_hParent->GetAbsAngles().y;
		flPitchCenter += m_hParent->GetAbsAngles().x;
	}

	// Force the angles to be relative to the center position
	angles.y = flYawCenter + UTIL_AngleDistance( angles.y, flYawCenter );
	angles.x = flPitchCenter + UTIL_AngleDistance( angles.x, flPitchCenter );

	// limit against range in y
	if( m_yawRange != 0 && ( angles.y > flYawCenter + m_yawRange ))
	{
		angles.y = flYawCenter + m_yawRange;
		updateTime = FALSE;	// Don't update if you saw the player, but out of range
	}
	else if( m_yawRange != 0 && ( angles.y < flYawCenter - m_yawRange ))
	{
		angles.y = (flYawCenter - m_yawRange);
		updateTime = FALSE; // Don't update if you saw the player, but out of range
	}

	if( updateTime )
		m_lastSightTime = gpGlobals->time;

	Vector avelocity;

	// move toward target at rate or less
	float distY = UTIL_AngleDistance( angles.y, GetAbsAngles().y );

	avelocity.y = distY * 10;
	avelocity.y = bound( -m_yawRate, avelocity.y, m_yawRate );

	// Limit against range in x
	if( angles.x > flPitchCenter + m_pitchRange )
		angles.x = flPitchCenter + m_pitchRange;
	else if( angles.x < flPitchCenter - m_pitchRange )
		angles.x = flPitchCenter - m_pitchRange;

	// Move toward target at rate or less
	float distX = UTIL_AngleDistance( angles.x, GetAbsAngles().x );

	avelocity.x = distX  * 10;
	avelocity.x = bound( -m_pitchRate, avelocity.x, m_pitchRate );

	avelocity.z = 0.0f;	// not used

	SetLocalAvelocity( avelocity );
	SetMoveDoneTime( 0.05 );

	// firing with player-controlled tanks:
	if( pController )
	{
		if( gpGlobals->time < m_flNextAttack )
			return;	// too early

		if( pController->pev->button & IN_ATTACK )
		{
			Vector forward;
			UTIL_MakeVectorsPrivate( GetAbsAngles(), forward, NULL, NULL );

			// to make sure the gun doesn't fire too many bullets
			m_fireLast = gpGlobals->time - (1.0f / m_fireRate ) - 0.01f;

			TryFire( BarrelPosition(), forward, pController->pev );
		
			if( pController->IsPlayer() )
				((CBasePlayer *)pController)->m_iWeaponVolume = LOUD_GUN_VOLUME;

			m_flNextAttack = gpGlobals->time + (1.0f / m_fireRate);
		}
		else
		{
			m_iState = STATE_ON;
		}
	}
	else if( CanFire() && ( FBitSet( pev->spawnflags, SF_TANK_LINEOFSIGHT ) || ( fabs( distX ) < m_pitchTolerance && fabs( distY ) < m_yawTolerance )))
	{
		BOOL fire = FALSE;
		Vector forward;
		UTIL_MakeVectorsPrivate( GetAbsAngles(), forward, NULL, NULL );

		if( FBitSet( pev->spawnflags, SF_TANK_LINEOFSIGHT ))
		{
			float length = direction.Length();
			UTIL_TraceLine( barrelEnd, barrelEnd + forward * length, dont_ignore_monsters, edict(), &tr );
			if ( tr.pHit == pTarget->edict() || ( FClassnameIs( pTarget, "monster_target" ) && tr.fInOpen ))
				fire = TRUE;
		}
		else
		{
			fire = TRUE;
		}

		if( fire )
		{
			TryFire( BarrelPosition(), forward, pev );
		}
		else
		{
			m_fireLast = 0;
		}
	}
	else
	{
		m_fireLast = 0;
	}
}

// If barrel is offset, add in additional rotation
void CBaseTank :: AdjustAnglesForBarrel( Vector &angles, float distance )
{
	float r2, d2;

	if( m_barrelPos.y != 0 || m_barrelPos.z != 0 )
	{
		distance -= m_barrelPos.z;
		d2 = distance * distance;

		if( m_barrelPos.y )
		{
			r2 = m_barrelPos.y * m_barrelPos.y;
			angles.y += (180.0 / M_PI) * atan2( m_barrelPos.y, sqrt( d2 - r2 ) );
		}
		if ( m_barrelPos.z )
		{
			r2 = m_barrelPos.z * m_barrelPos.z;
			angles.x -= (180.0 / M_PI) * atan2( -m_barrelPos.z, sqrt( d2 - r2 ) );
		}
	}
}

// Check the FireMaster before actually firing
void CBaseTank :: TryFire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	// NOTE: reason - player pressed a "fire"
	// in other cases we can't building some interesting things
	// e.g. barrel spinning up before fire
	m_iState = STATE_IN_USE;

	if( UTIL_IsMasterTriggered( m_iszFireMaster, NULL ))
	{
		Fire( barrelEnd, forward, pevAttacker );
	}
}

// Fire targets and spawn sprites
void CBaseTank :: Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if( m_fireLast != 0 )
	{
		if( m_iszSpriteSmoke )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING( m_iszSpriteSmoke ), barrelEnd, TRUE );
			pSprite->AnimateAndDie( RANDOM_FLOAT( 15.0, 20.0 ) );
			pSprite->SetTransparency( kRenderTransAlpha, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, 255, kRenderFxNone );
			pSprite->SetLocalVelocity( Vector( 0.0f, 0.0f, RANDOM_FLOAT( 40, 80 )));
			pSprite->SetScale( m_spriteScale );
		}

		if( m_iszSpriteFlash )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING( m_iszSpriteFlash ), barrelEnd, TRUE );
			pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
			pSprite->SetScale( m_spriteScale );
			pSprite->AnimateAndDie( 60 );
			pSprite->SetParent( this );
		}

		SUB_UseTargets( this, USE_TOGGLE, 0 );
	}

	m_fireLast = gpGlobals->time;
}

void CBaseTank :: TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, TraceResult &tr )
{
	// get circular gaussian spread
	float x, y, z;
	do {
		x = RANDOM_FLOAT( -0.5f, 0.5f ) + RANDOM_FLOAT( -0.5f, 0.5f );
		y = RANDOM_FLOAT( -0.5f, 0.5f ) + RANDOM_FLOAT( -0.5f, 0.5f );
		z = x * x + y * y;
	} while( z > 1 );

	Vector vecDir = vecForward + x * vecSpread.x * gpGlobals->v_right + y * vecSpread.y * gpGlobals->v_up;
	Vector vecEnd = vecStart + vecDir * 4096;

	UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, edict(), &tr );
}
	
void CBaseTank :: StartRotSound( void )
{
	if( !pev->noise || FBitSet( pev->spawnflags, SF_TANK_SOUNDON ))
		return;

	SetBits( pev->spawnflags, SF_TANK_SOUNDON );
	EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ), 0.85f, ATTN_NORM );
}

void CBaseTank :: StopRotSound( void )
{
	if( FBitSet( pev->spawnflags, SF_TANK_SOUNDON ))
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));
	ClearBits( pev->spawnflags, SF_TANK_SOUNDON );
}

class CFuncTankGun : public CBaseTank
{
	DECLARE_CLASS( CFuncTankGun, CBaseTank );
public:
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
};

LINK_ENTITY_TO_CLASS( func_tank, CFuncTankGun );

void CFuncTankGun::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;

	if( m_fireLast != 0 )
	{
		// fireBullets needs gpGlobals->v_up, etc.
		UTIL_MakeAimVectors( GetAbsAngles( ));

		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;

		if( bulletCount > 0 )
		{
			for( i = 0; i < bulletCount; i++ )
			{
				switch( m_bulletType )
				{
				case TANK_BULLET_9MM:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, BULLET_MONSTER_9MM, 1, m_iBulletDamage, pevAttacker );
					break;
				case TANK_BULLET_MP5:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, BULLET_MONSTER_MP5, 1, m_iBulletDamage, pevAttacker );
					break;
				case TANK_BULLET_12MM:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, BULLET_MONSTER_12MM, 1, m_iBulletDamage, pevAttacker );
					break;
				case TANK_BULLET_NONE:
				default: break;
				}
			}

			BaseClass::Fire( barrelEnd, forward, pevAttacker );
		}
	}
	else
	{
		BaseClass::Fire( barrelEnd, forward, pevAttacker );
	}
}

class CFuncTankLaser : public CBaseTank
{
	DECLARE_CLASS( CFuncTankLaser, CBaseTank );
public:
	void	Activate( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	void	Think( void );
	CLaser	*GetLaser( void );

	DECLARE_DATADESC();
private:
	CLaser	*m_pLaser;
	float	m_laserTime;
};

LINK_ENTITY_TO_CLASS( func_tanklaser, CFuncTankLaser );

BEGIN_DATADESC( CFuncTankLaser )
	DEFINE_FIELD( m_pLaser, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_laserTime, FIELD_TIME ),
END_DATADESC()

void CFuncTankLaser :: Activate( void )
{
	if( !GetLaser( ) )
	{
		ALERT( at_error, "Laser tank with no env_laser!\n" );
		UTIL_Remove( this );
		return;
	}
	else
	{
		m_pLaser->TurnOff();
	}

	m_bulletType = TANK_BULLET_OTHER;

	BaseClass::Activate();
}

void CFuncTankLaser :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "laserentity" ))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue( pkvd );
	}
}

CLaser *CFuncTankLaser :: GetLaser( void )
{
	if( m_pLaser )
		return m_pLaser;

	CBaseEntity *pEntity;

	pEntity = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ));

	while( pEntity )
	{
		// Found the laser
		if( FClassnameIs( pEntity->pev, "env_laser" ))
		{
			m_pLaser = (CLaser *)pEntity;
			break;
		}
		else
		{
			pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->message ));
		}
	}
	return m_pLaser;
}

void CFuncTankLaser :: Think( void )
{
	if( m_pLaser && ( gpGlobals->time > m_laserTime ))
		m_pLaser->TurnOff();

	CBaseTank::Think();
}

void CFuncTankLaser :: Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	TraceResult tr;

	if( m_fireLast != 0 && GetLaser() )
	{
		// TankTrace needs gpGlobals->v_up, etc.
		UTIL_MakeAimVectors( GetAbsAngles( ));

		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		if( bulletCount )
		{
			for( int i = 0; i < bulletCount; i++ )
			{
				m_pLaser->SetAbsOrigin( barrelEnd );
				TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );
				
				m_laserTime = gpGlobals->time;
				m_pLaser->TurnOn();
				m_pLaser->pev->dmgtime = gpGlobals->time - 1.0f;
				m_pLaser->FireAtPoint( barrelEnd, tr );
				m_pLaser->SetNextThink( 0 );
			}

			BaseClass::Fire( barrelEnd, forward, pev );
		}
	}
	else
	{
		BaseClass::Fire( barrelEnd, forward, pev );
	}
}

class CFuncTankRocket : public CBaseTank
{
	DECLARE_CLASS( CFuncTankRocket, CBaseTank );
public:
	void Precache( void );
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
};

LINK_ENTITY_TO_CLASS( func_tankrocket, CFuncTankRocket );

void CFuncTankRocket :: Precache( void )
{
	UTIL_PrecacheOther( "rpg_rocket" );
	BaseClass::Precache();

	m_bulletType = TANK_BULLET_OTHER;
}

void CFuncTankRocket :: Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;

		if( bulletCount > 0 )
		{
			for( int i = 0; i < bulletCount; i++ )
			{
				CBaseEntity *pRocket = CBaseEntity::Create( "rpg_rocket", barrelEnd, GetAbsAngles(), edict() );
			}
			BaseClass::Fire( barrelEnd, forward, pev );
		}
	}
	else
	{
		BaseClass::Fire( barrelEnd, forward, pev );
	}
}

class CFuncTankMortar : public CBaseTank
{
	DECLARE_CLASS( CFuncTankMortar, CBaseTank );
public:
	void KeyValue( KeyValueData *pkvd );
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	void Precache( void );
};

LINK_ENTITY_TO_CLASS( func_tankmortar, CFuncTankMortar );

void CFuncTankMortar :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "iMagnitude" ))
	{
		pev->impulse = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue( pkvd );
	}
}

void CFuncTankMortar :: Precache( void )
{
	BaseClass::Precache();

	m_bulletType = TANK_BULLET_OTHER;
}

void CFuncTankMortar::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;

		// Only create 1 explosion
		if( bulletCount > 0 )
		{
			TraceResult tr;

			// TankTrace needs gpGlobals->v_up, etc.
			UTIL_MakeAimVectors( GetAbsAngles());
			TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );
			ExplosionCreate( tr.vecEndPos, GetAbsAngles(), edict(), pev->impulse, TRUE );
			BaseClass::Fire( barrelEnd, forward, pev );
		}
	}
	else
	{
		BaseClass::Fire( barrelEnd, forward, pev );
	}
}

//============================================================================
// FUNC TANK CONTROLS
//============================================================================
LINK_ENTITY_TO_CLASS( func_tankcontrols, CFuncTankControls );

BEGIN_DATADESC( CFuncTankControls )
	DEFINE_FIELD( m_pController, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_vecControllerUsePos, FIELD_VECTOR ),
	DEFINE_AUTO_ARRAY( m_iTankName, FIELD_STRING ),
	DEFINE_FIELD( m_cTanks, FIELD_INTEGER ),
	DEFINE_FIELD( m_fVerifyTanks, FIELD_BOOLEAN ),
END_DATADESC()

void CFuncTankControls :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;

	SET_MODEL( edict(), GetModel() );
	UTIL_SetSize( pev, pev->mins, pev->maxs );

	// store pev->target for backward compatibility
	if( m_cTanks < MAX_CONTROLLED_TANKS && !FStringNull( pev->target ))
	{
		m_iTankName[m_cTanks] = pev->target;
		m_cTanks++;
	}

	SetLocalAngles( g_vecZero );
	SetBits( m_iFlags, MF_TRIGGER );
}

void CFuncTankControls :: KeyValue( KeyValueData *pkvd )
{
	// get support for spirit field too
	if( FStrEq( pkvd->szKeyName, "parent" ) || FStrEq( pkvd->szKeyName, "movewith" ))
	{
		m_iParent = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( m_cTanks < MAX_CONTROLLED_TANKS ) 
	{
		// add this field to the target list
		// this assumes that additional fields are targetnames and their values are delay values.
		char tmp[128];
		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iTankName[m_cTanks] = ALLOC_STRING( tmp );
		m_cTanks++;
		pkvd->fHandled = TRUE;
	}
}

BOOL CFuncTankControls :: OnControls( CBaseEntity *pTest )
{
	if( m_hParent != NULL && FClassnameIs( m_hParent->pev, "func_tracktrain" ))
	{
		// transform local controller pos through tankcontrols pos
		Vector vecTransformedControlerUsePos = EntityToWorldTransform().VectorTransform( m_vecControllerUsePos );
		if(( vecTransformedControlerUsePos - pTest->GetAbsOrigin() ).Length() < 30 )
		{
			return TRUE;
		}
	}
	else if(( m_vecControllerUsePos - pTest->GetAbsOrigin() ).Length() < 30 )
	{
		return TRUE;
	}

	return FALSE;
}

void CFuncTankControls :: HandleTank ( CBaseEntity *pActivator, CBaseEntity *m_pTank, BOOL activate )
{
	// it's tank entity
	if( m_pTank && m_pTank->IsTank( ))
	{
		if( activate )
		{
			if(((CBaseTank *)m_pTank)->StartControl((CBasePlayer *)pActivator, this ))
			{
				m_iState = STATE_ON; // we have active tank!
			}
		}
		else
		{
			((CBaseTank *)m_pTank)->StopControl( this );
		}
	}
}

void CFuncTankControls :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	if( !m_pController && useType != USE_OFF )
	{
		if( !pActivator || !( pActivator->IsPlayer( )))
			return;

		if( m_iState != STATE_OFF || ((CBasePlayer *)pActivator)->m_pTank != NULL )
			return;

		// find all specified tanks
		for( int i = 0; i < m_cTanks; i++ )
		{
			CBaseEntity *tryTank = NULL;
	
			// find all tanks with current name
			while( (tryTank = UTIL_FindEntityByTargetname( tryTank, STRING( m_iTankName[i] ))))
			{
				HandleTank( pActivator, tryTank, TRUE );
			}			
		}

		if( m_iState == STATE_ON )
		{
			// we found at least one tank to use, so holster player's weapon
			m_pController = (CBasePlayer *)pActivator;
			m_pController->m_pTank = this;

			if( m_pController->m_pActiveItem )
			{
				m_pController->m_pActiveItem->Holster();
				m_pController->pev->weaponmodel = 0;
				// viewmodel reset in tank
			}

			m_pController->m_iHideHUD |= HIDEHUD_WEAPONS;

			// remember where the player's standing, so we can tell when he walks away
			if( m_hParent != NULL && FClassnameIs( m_hParent->pev, "func_tracktrain" ))
			{
				// transform controller pos into local space because parent can be moving
				m_vecControllerUsePos = m_pController->EntityToWorldTransform().VectorITransform( m_pController->GetAbsOrigin() );
			}
			else m_vecControllerUsePos = m_pController->GetAbsOrigin();
		}
	}
	else if( m_pController && useType != USE_ON )
	{
		// find all specified tanks
		for( int i = 0; i < m_cTanks; i++ )
		{
			CBaseEntity *tryTank = NULL;

			// find all tanks with current name
			while(( tryTank = UTIL_FindEntityByTargetname( tryTank, STRING( m_iTankName[i] ))))
			{
				HandleTank( pActivator, tryTank, FALSE );
			}			
		}

		// bring back player's weapons
		if( m_pController->m_pActiveItem )
			m_pController->m_pActiveItem->Deploy();

		m_pController->m_iHideHUD &= ~HIDEHUD_WEAPONS;
		m_pController->m_pTank = NULL;				

		m_pController = NULL;
		m_iState = STATE_OFF;
	}
}
