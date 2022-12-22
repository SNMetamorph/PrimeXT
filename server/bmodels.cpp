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
#include "client.h"
#include "player.h"
#include "func_break.h"

// =================== FUNC_WALL ==============================================

class CFuncWall : public CBaseDelay
{
	DECLARE_CLASS( CFuncWall, CBaseDelay );
public:
	void Spawn( void );
	virtual void TurnOn( void );
	virtual void TurnOff( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( func_wall, CFuncWall );

void CFuncWall :: Spawn( void )
{
	pev->flags |= FL_WORLDBRUSH;
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	m_iState = STATE_OFF;

	SetLocalAngles( g_vecZero );
	SET_MODEL( edict(), GetModel() );

	// LRC (support generic switchable texlight)
	if( m_iStyle >= 32 )
		LIGHT_STYLE( m_iStyle, "a" );
	else if( m_iStyle <= -32 )
		LIGHT_STYLE( -m_iStyle, "z" );

	if( m_hParent != NULL || FClassnameIs( pev, "func_wall_toggle" ))
		m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
	else m_pUserData = WorldPhysic->CreateStaticBodyFromEntity( this );
}

void CFuncWall :: TurnOff( void )
{
	pev->frame = 0;
	m_iState = STATE_OFF;
}

void CFuncWall :: TurnOn( void )
{
	pev->frame = 1;
	m_iState = STATE_ON;
}

void CFuncWall :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	if( ShouldToggle( useType ))
	{
		if( GetState() == STATE_ON )
			TurnOff();
		else TurnOn();

		// LRC (support generic switchable texlight)
		if( m_iStyle >= 32 )
		{
			if( pev->frame )
				LIGHT_STYLE( m_iStyle, "z" );
			else
				LIGHT_STYLE( m_iStyle, "a" );
		}
		else if( m_iStyle <= -32 )
		{
			if( pev->frame )
				LIGHT_STYLE( -m_iStyle, "a" );
			else
				LIGHT_STYLE( -m_iStyle, "z" );
		}
	}
}

// =================== FUNC_WALL_TOGGLE ==============================================

#define SF_WALL_START_OFF		BIT( 0 )

class CFuncWallToggle : public CFuncWall
{
	DECLARE_CLASS( CFuncWallToggle, CFuncWall );
public:
	void Spawn( void );
	void TurnOff( void );
	void TurnOn( void );
};

LINK_ENTITY_TO_CLASS( func_wall_toggle, CFuncWallToggle );

void CFuncWallToggle :: Spawn( void )
{
	BaseClass::Spawn();

	if( FBitSet( pev->spawnflags, SF_WALL_START_OFF ))
		TurnOff();
	else TurnOn();
}

void CFuncWallToggle :: TurnOff( void )
{
	WorldPhysic->EnableCollision( this, FALSE );
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	RelinkEntity( FALSE );
	m_iState = STATE_OFF;
}

void CFuncWallToggle :: TurnOn( void )
{
	WorldPhysic->EnableCollision( this, TRUE );
	pev->solid = SOLID_BSP;
	pev->effects &= ~EF_NODRAW;
	RelinkEntity( FALSE );
	m_iState = STATE_ON;
}

// =================== FUNC_CONVEYOR ==============================================

#define SF_CONVEYOR_VISUAL		BIT( 0 )
#define SF_CONVEYOR_NOTSOLID		BIT( 1 )
#define SF_CONVEYOR_STARTOFF		BIT( 2 )

class CFuncConveyor : public CBaseDelay
{
	DECLARE_CLASS( CFuncConveyor, CBaseDelay );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_SET_MOVEDIR; }
	void UpdateSpeed( float speed );

	DECLARE_DATADESC();

	float	m_flMaxSpeed;
};

LINK_ENTITY_TO_CLASS( func_conveyor, CFuncConveyor );

BEGIN_DATADESC( CFuncConveyor )
	DEFINE_FIELD( m_flMaxSpeed, FIELD_FLOAT ),
END_DATADESC()

void CFuncConveyor :: Spawn( void )
{
	pev->flags |= FL_WORLDBRUSH;
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL( edict(), GetModel() );

	if( !FBitSet( pev->spawnflags, SF_CONVEYOR_VISUAL ))
		SetBits( pev->flags, FL_CONVEYOR );

	// is mapper forgot set angles?
	if( pev->movedir == g_vecZero )
		pev->movedir = Vector( 1, 0, 0 );

	// HACKHACK - This is to allow for some special effects
	if( FBitSet( pev->spawnflags, SF_CONVEYOR_NOTSOLID ))
	{
		pev->solid = SOLID_NOT;
		pev->skin = 0; // don't want the engine thinking we've got special contents on this brush
	}
	else
	{
		if( m_hParent )
			m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
		else m_pUserData = WorldPhysic->CreateStaticBodyFromEntity( this );
	}

	if( pev->speed == 0 )
		pev->speed = 100;

	m_flMaxSpeed = pev->speed;	// save initial speed

	if( FBitSet( pev->spawnflags, SF_CONVEYOR_STARTOFF ))
		UpdateSpeed( 0 );
	else UpdateSpeed( m_flMaxSpeed );
}

void CFuncConveyor :: UpdateSpeed( float speed )
{
	// make sure the sign is correct - positive for forward rotation,
	// negative for reverse rotation.
	speed = fabs( speed );
	if( pev->impulse ) speed *= -1;

	// encode it as an integer with 4 fractional bits
	int speedCode = (int)(fabs( speed ) * 16.0f);

	// HACKHACK -- This is ugly, but encode the speed in the rendercolor
	// to avoid adding more data to the network stream
	pev->rendercolor.x = (speed < 0) ? 1 : 0;
	pev->rendercolor.y = (speedCode >> 8);
	pev->rendercolor.z = (speedCode & 0xFF);

	// set conveyor state
	m_iState = ( speed != 0 ) ? STATE_ON : STATE_OFF;
	pev->speed = speed;	// update physical speed too
}

void CFuncConveyor :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	if( ShouldToggle( useType ))
	{
		if( useType == USE_SET )
		{
			if( value != 0 ) 
			{
				pev->impulse = value < 0 ? true : false;
				value = fabs( value );
				pev->dmg = bound( 0.1, value, 1 ) * m_flMaxSpeed;
			}
			else UpdateSpeed( 0 ); // stop
			return;
		}
		else if( useType == USE_RESET )
		{
			// restore last speed
			UpdateSpeed( pev->dmg );
			return;
		}
  
		pev->impulse = !pev->impulse;
		UpdateSpeed( m_flMaxSpeed );
	}
}

// =================== FUNC_TRANSPORTER ==============================================
// like conveyor but with more intuitive controls

#define SF_TRANSPORTER_STARTON	BIT( 0 )
#define SF_TRANSPORTER_SOLID		BIT( 1 )

class CFuncTransporter : public CBaseDelay
{
	DECLARE_CLASS( CFuncTransporter, CBaseDelay );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_SET_MOVEDIR; }
	void UpdateSpeed( float speed );
	void Think( void );

	DECLARE_DATADESC();

	float	m_flMaxSpeed;
	float	m_flWidth;
};

LINK_ENTITY_TO_CLASS( func_transporter, CFuncTransporter );

BEGIN_DATADESC( CFuncTransporter )
	DEFINE_FIELD( m_flMaxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flWidth, FIELD_FLOAT ),
END_DATADESC()

void CFuncTransporter :: Spawn( void )
{
	pev->flags |= FL_WORLDBRUSH;
	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;

	SET_MODEL( edict(), GetModel() );
	SetBits( pev->effects, EF_CONVEYOR );

	// is mapper forgot set angles?
	if( pev->movedir == g_vecZero )
		pev->movedir = Vector( 1, 0, 0 );

	model_t *bmodel;

	// get a world struct
	if(( bmodel = (model_t *)MODEL_HANDLE( pev->modelindex )) == NULL )
	{
		ALERT( at_warning, "%s: unable to fetch model pointer %s\n", GetDebugName(), GetModel( ));
		m_flWidth = 64.0f;	// default
	}
	else
	{
		// lookup for the scroll texture
		for( int i = 0; i < bmodel->nummodelsurfaces; i++ )
		{
			msurface_t *face = &bmodel->surfaces[bmodel->firstmodelsurface + i];
			if( FBitSet( face->flags, SURF_CONVEYOR ))
			{
				m_flWidth = face->texinfo->texture->width;
				break;
			}
		}

		if( m_flWidth <= 0.0 )
		{
			ALERT( at_warning, "%s: unable to find scrolling texture\n", GetDebugName() );
			m_flWidth = 64.0f;	// default
		}
	}

	if( FBitSet( pev->spawnflags, SF_TRANSPORTER_SOLID ))
	{
		if( m_hParent )
			m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
		else m_pUserData = WorldPhysic->CreateStaticBodyFromEntity( this );
		SetBits( pev->flags, FL_CONVEYOR );
	}
	else
	{
		pev->solid = SOLID_NOT;
		pev->skin = 0; // don't want the engine thinking we've got special contents on this brush
	}

	if( pev->speed == 0 )
		pev->speed = 100;

	m_flMaxSpeed = pev->speed;	// save initial speed

	if( FBitSet( pev->spawnflags, SF_TRANSPORTER_STARTON ))
		UpdateSpeed( m_flMaxSpeed );
	else UpdateSpeed( 0 );
	SetNextThink( 0 );
}

void CFuncTransporter :: UpdateSpeed( float speed )
{
	// set conveyor state
	m_iState = ( speed != 0 ) ? STATE_ON : STATE_OFF;
	pev->speed = speed;
}

void CFuncTransporter :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	if( ShouldToggle( useType ))
	{
		if( useType == USE_SET )
		{
			UpdateSpeed( value );
		}
		else
		{
			if( GetState() == STATE_ON )
				UpdateSpeed( 0 );
			else UpdateSpeed( m_flMaxSpeed );
		}
	}
}

void CFuncTransporter :: Think( void )
{
	if( pev->speed != 0 )
	{
		pev->fuser1 += pev->speed * gpGlobals->frametime * (1.0f / m_flWidth); 

		// don't exceed some limits to prevent precision loosing
		while( pev->fuser1 > m_flWidth )
			pev->fuser1 -= m_flWidth;
		while( pev->fuser1 < -m_flWidth )
			pev->fuser1 += m_flWidth;
	}

	// think every frame
	SetNextThink( gpGlobals->frametime );
}

// =================== FUNC_ILLUSIONARY ==============================================

class CFuncIllusionary : public CBaseEntity
{
	DECLARE_CLASS( CFuncIllusionary, CBaseEntity );
public:
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( func_illusionary, CFuncIllusionary );

void CFuncIllusionary :: Spawn( void )
{
	pev->movetype = MOVETYPE_NONE;  
	pev->solid = SOLID_NOT; // always solid_not 
	SET_MODEL( edict(), GetModel() );
	
	// I'd rather eat the network bandwidth of this than figure out how to save/restore
	// these entities after they have been moved to the client, or respawn them ala Quake
	// Perhaps we can do this in deathmatch only.
	// MAKE_STATIC( edict() );
}

// =================== FUNC_MONSTERCLIP ==============================================

class CFuncMonsterClip : public CBaseEntity
{
	DECLARE_CLASS( CFuncMonsterClip, CBaseEntity );
public:
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( func_monsterclip, CFuncMonsterClip );

void CFuncMonsterClip :: Spawn( void )
{
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	SetBits( pev->flags, FL_MONSTERCLIP );
	SetBits( m_iFlags, MF_TRIGGER );
	pev->effects |= EF_NODRAW;

	// link into world
	SET_MODEL( edict(), GetModel() );
}

// =================== FUNC_ROTATING ==============================================

#define SF_ROTATING_INSTANT		BIT( 0 )
#define SF_ROTATING_BACKWARDS		BIT( 1 )
#define SF_ROTATING_Z_AXIS		BIT( 2 )
#define SF_ROTATING_X_AXIS		BIT( 3 )
#define SF_ROTATING_ACCDCC		BIT( 4 )	// brush should accelerate and decelerate when toggled
#define SF_ROTATING_HURT		BIT( 5 )	// rotating brush that inflicts pain based on rotation speed
#define SF_ROTATING_NOT_SOLID		BIT( 6 )	// some special rotating objects are not solid.
#define SF_ROTATING_SND_SMALL_RADIUS	BIT( 7 )
#define SF_ROTATING_SND_MEDIUM_RADIUS	BIT( 8 )
#define SF_ROTATING_SND_LARGE_RADIUS	BIT( 9 )
#define SF_ROTATING_STOP_AT_START_POS	BIT( 10 )

class CFuncRotating : public CBaseDelay
{
	DECLARE_CLASS( CFuncRotating, CBaseDelay );
public:
	void Spawn( void  );
	void Precache( void  );
	void SpinUp ( void );
	void SpinDown ( void );
	void ReverseMove( void );
	void RotateFriction( void );
	void KeyValue( KeyValueData* pkvd);
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_HOLD_ANGLES; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void UpdateSpeed( float flNewSpeed );
	bool SpinDown( float flTargetSpeed );
	void SetTargetSpeed( float flSpeed );
	void HurtTouch ( CBaseEntity *pOther );
	void Rotate( void );
	void RampPitchVol( void );
	void Blocked( CBaseEntity *pOther );
	float GetNextMoveInterval( void ) const;

	DECLARE_DATADESC();

private:
	byte	m_bStopAtStartPos;
	float	m_flFanFriction;
	float	m_flAttenuation;
	float	m_flTargetSpeed;
	float	m_flMaxSpeed;
	float	m_flVolume;
	Vector	m_angStart;
	string_t	m_sounds;
};

LINK_ENTITY_TO_CLASS( func_rotating, CFuncRotating );

BEGIN_DATADESC( CFuncRotating )
	DEFINE_FIELD( m_bStopAtStartPos, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFanFriction, FIELD_FLOAT ),
	DEFINE_FIELD( m_flAttenuation, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTargetSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMaxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_angStart, FIELD_VECTOR ),
	DEFINE_FIELD( m_flVolume, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_sounds, FIELD_STRING, "sounds" ),
	DEFINE_FUNCTION( SpinUp ),
	DEFINE_FUNCTION( SpinDown ),
	DEFINE_FUNCTION( HurtTouch ),
	DEFINE_FUNCTION( RotateFriction ),
	DEFINE_FUNCTION( ReverseMove ),
	DEFINE_FUNCTION( Rotate ),
END_DATADESC()

void CFuncRotating :: KeyValue( KeyValueData* pkvd)
{
	if( FStrEq( pkvd->szKeyName, "fanfriction" ))
	{
		m_flFanFriction = Q_atof( pkvd->szValue ) / 100.0f;
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "spawnorigin" ))
	{
		Vector tmp = Q_atov( pkvd->szValue );
		if( tmp != g_vecZero ) SetAbsOrigin( tmp );
	}
	else if( FStrEq( pkvd->szKeyName, "sounds" ))
	{
		m_sounds = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "volume" ))
	{
		m_flVolume = bound( 0.0f, Q_atof( pkvd->szValue ) / 10.0f, 1.0f );
		pkvd->fHandled = TRUE;
	}
	else
	{ 
		CBaseDelay::KeyValue( pkvd );
	}
}

void CFuncRotating :: Spawn( void )
{
	// maintain compatibility with previous maps
	if( m_flVolume == 0.0 ) m_flVolume = 1.0;

	// if the designer didn't set a sound attenuation, default to one.
	m_flAttenuation = ATTN_NORM;
	
	if( FBitSet( pev->spawnflags, SF_ROTATING_SND_SMALL_RADIUS ))
	{
		m_flAttenuation = ATTN_IDLE;
	}
	else if( FBitSet( pev->spawnflags, SF_ROTATING_SND_MEDIUM_RADIUS ))
	{
		m_flAttenuation = ATTN_STATIC;
	}
	else if( FBitSet( pev->spawnflags, SF_ROTATING_SND_LARGE_RADIUS ))
	{
		m_flAttenuation = ATTN_NORM;
	}

	// prevent divide by zero if level designer forgets friction!
	if( m_flFanFriction == 0 ) m_flFanFriction = 1.0f;

	if( FBitSet( pev->spawnflags, SF_ROTATING_Z_AXIS ))
	{
		pev->movedir = Vector( 0, 0, 1 );
	}
	else if( FBitSet( pev->spawnflags, SF_ROTATING_X_AXIS ))
	{
		pev->movedir = Vector( 1, 0, 0 );
	}
	else
	{ 
		pev->movedir = Vector( 0, 1, 0 ); // y-axis
	}

	// check for reverse rotation
	if( FBitSet( pev->spawnflags, SF_ROTATING_BACKWARDS ))
	{
		pev->movedir = pev->movedir * -1;
	}

	// some rotating objects like fake volumetric lights will not be solid.
	if( FBitSet( pev->spawnflags, SF_ROTATING_NOT_SOLID ))
	{
		pev->solid = SOLID_NOT;
		pev->skin = CONTENTS_EMPTY;
		pev->movetype = MOVETYPE_PUSH;
	}
	else
	{
		pev->solid = SOLID_BSP;
		pev->movetype = MOVETYPE_PUSH;
	}

	SET_MODEL( edict(), GetModel() );

	// did level designer forget to assign speed?
	m_flMaxSpeed = fabs( pev->speed );
	if( m_flMaxSpeed == 0 ) m_flMaxSpeed = 100;

	SetLocalAngles( m_vecTempAngles );	// all the child entities is set. Time to move hierarchy
	UTIL_SetOrigin( this, GetLocalOrigin( ));

	pev->speed = 0;
	m_angStart = GetLocalAngles();
	m_iState = STATE_OFF;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );

	// instant-use brush?
	if( FBitSet( pev->spawnflags, SF_ROTATING_INSTANT ))
	{		
		SetThink( &CBaseEntity::SUB_CallUseToggle );
		SetNextThink( 0.2 ); // leave a magic delay for client to start up
	}	

	// can this brush inflict pain?
	if( FBitSet( pev->spawnflags, SF_ROTATING_HURT ))
	{
		SetTouch( &CFuncRotating::HurtTouch );
	}
	
	Precache( );
}

float CFuncRotating::GetNextMoveInterval( void ) const
{
	if( m_bStopAtStartPos )
	{
		return gpGlobals->frametime;
	}
	return 0.1f;
}

void CFuncRotating :: Precache( void )
{
	// set up fan sounds
	if( !FStringNull( pev->message ))
	{
		// if a path is set for a wave, use it
		pev->noise3 = UTIL_PrecacheSound( STRING( pev->message ));
	}
	else
	{
		int m_sound = UTIL_LoadSoundPreset( m_sounds );

		// otherwise use preset sound
		switch( m_sound )
		{
		case 1:
			pev->noise3 = UTIL_PrecacheSound( "fans/fan1.wav" );
			break;
		case 2:
			pev->noise3 = UTIL_PrecacheSound( "fans/fan2.wav" );
			break;
		case 3:
			pev->noise3 = UTIL_PrecacheSound( "fans/fan3.wav" );
			break;
		case 4:
			pev->noise3 = UTIL_PrecacheSound( "fans/fan4.wav" );
			break;
		case 5:
			pev->noise3 = UTIL_PrecacheSound( "fans/fan5.wav" );
			break;
		case 0:
			pev->noise3 = UTIL_PrecacheSound( "common/null.wav" );
			break;
		default:
			pev->noise3 = UTIL_PrecacheSound( m_sound );
			break;
		}
	}
	
	if( GetLocalAvelocity() != g_vecZero && !pev->friction )
	{
		// if fan was spinning, and we went through transition or save/restore,
		// make sure we restart the sound. 1.5 sec delay is magic number. KDB
		SetMoveDone( &CFuncRotating::SpinUp );
		SetMoveDoneTime( 0.2 );
	}
}

//
// Touch - will hurt others based on how fast the brush is spinning
//
void CFuncRotating :: HurtTouch ( CBaseEntity *pOther )
{
	entvars_t	*pevOther = pOther->pev;

	// we can't hurt this thing, so we're not concerned with it
	if( !pOther->pev->takedamage ) return;

	// calculate damage based on rotation speed
	pev->dmg = GetLocalAvelocity().Length() / 10;

	pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );

	Vector vecNewVelocity = (pOther->GetAbsOrigin() - Center()).Normalize();
	pOther->SetAbsVelocity( vecNewVelocity * pev->dmg );
}

//
// RampPitchVol - ramp pitch and volume up to final values, based on difference
// between how fast we're going vs how fast we plan to go
//
#define FANPITCHMIN		30
#define FANPITCHMAX		100

void CFuncRotating :: RampPitchVol( void )
{
	// calc volume and pitch as % of maximum vol and pitch.
	float fpct = fabs( GetLocalAvelocity().Length() ) / m_flMaxSpeed;
	float fvol = bound( 0.0f, m_flVolume * fpct, 1.0f ); // slowdown volume ramps down to 0

	float fpitch = FANPITCHMIN + ( FANPITCHMAX - FANPITCHMIN ) * fpct;	
	
	int pitch = bound( 0, fpitch, 255 );

	if( pitch == PITCH_NORM )
	{
		pitch = PITCH_NORM - 1;
	}

	// change the fan's vol and pitch
	EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noise3 ), fvol, m_flAttenuation, SND_CHANGE_PITCH|SND_CHANGE_VOL, pitch );

}

void CFuncRotating :: UpdateSpeed( float flNewSpeed )
{
	float flOldSpeed = pev->speed;
	pev->speed = bound( -m_flMaxSpeed, flNewSpeed, m_flMaxSpeed );

	if( m_bStopAtStartPos )
	{
		int checkAxis = 2;

		// see if we got close to the starting orientation
		if( pev->movedir[0] != 0 )
		{
			checkAxis = 0;
		}
		else if( pev->movedir[1] != 0 )
		{
			checkAxis = 1;
		}

		float angDelta = anglemod( GetLocalAngles()[checkAxis] - m_angStart[checkAxis] );

		if( angDelta > 180.0f )
			angDelta -= 360.0f;

		if( flNewSpeed < 100 )
		{
			if( flNewSpeed <= 25 && fabs( angDelta ) < 1.0f )
			{
				m_flTargetSpeed = 0;
				m_bStopAtStartPos = false;
				pev->speed = 0.0f;

				SetLocalAngles( m_angStart );
			}
			else if( fabs( angDelta ) > 90.0f )
			{
				// keep rotating at same speed for now
				pev->speed = flOldSpeed;
			}
			else
			{
				float minSpeed =  fabs( angDelta );
				if( minSpeed < 20 ) minSpeed = 20;
	
				pev->speed = flOldSpeed > 0.0f ? minSpeed : -minSpeed;
			}
		}
	}

	SetLocalAvelocity( pev->movedir * pev->speed );

	if(( flOldSpeed == 0 ) && ( pev->speed != 0 ))
	{
		// starting to move - emit the sound.
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noise3 ), 0.01f, m_flAttenuation, 0, FANPITCHMIN );
		RampPitchVol();
	}
	else if(( flOldSpeed != 0 ) && ( pev->speed == 0 ))
	{
		// stopping - stop the sound.
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise3 ));
		SetMoveDoneTime( -1 );
		m_iState = STATE_OFF;
	}
	else
	{
		// changing speed - adjust the pitch and volume.
		RampPitchVol();
	}
}

//
// SpinUp - accelerates a non-moving func_rotating up to it's speed
//
void CFuncRotating :: SpinUp( void )
{
	// calculate our new speed.
	float flNewSpeed = fabs( pev->speed ) + 0.2f * m_flMaxSpeed * m_flFanFriction;
	bool bSpinUpDone = false;

	if( fabs( flNewSpeed ) >= fabs( m_flTargetSpeed ))
	{
		// Reached our target speed.
		flNewSpeed = m_flTargetSpeed;
		bSpinUpDone = !m_bStopAtStartPos;
	}
	else if( m_flTargetSpeed < 0 )
	{
		// spinning up in reverse - negate the speed.
		flNewSpeed *= -1;
	}

	m_iState = STATE_TURN_ON;

	// Apply the new speed, adjust sound pitch and volume.
	UpdateSpeed( flNewSpeed );

	// If we've met or exceeded target speed, stop spinning up.
	if( bSpinUpDone )
	{
		SetMoveDone( &CFuncRotating::Rotate );
		Rotate();
	} 

	SetMoveDoneTime( GetNextMoveInterval() );
}

//-----------------------------------------------------------------------------
// Purpose: Decelerates the rotator from a higher speed to a lower one.
// Input  : flTargetSpeed - Speed to spin down to.
// Output : Returns true if we reached the target speed, false otherwise.
//-----------------------------------------------------------------------------
bool CFuncRotating::SpinDown( float flTargetSpeed )
{
	// Bleed off a little speed due to friction.
	bool bSpinDownDone = false;
	float flNewSpeed = fabs( pev->speed ) - 0.1f * m_flMaxSpeed * m_flFanFriction;

	if( flNewSpeed < 0 )
	{
		flNewSpeed = 0;
	}

	if( fabs( flNewSpeed ) <= fabs( flTargetSpeed ))
	{
		// Reached our target speed.
		flNewSpeed = flTargetSpeed;
		bSpinDownDone = !m_bStopAtStartPos;
	}
	else if( pev->speed < 0 )
	{
		// spinning down in reverse - negate the speed.
		flNewSpeed *= -1;
	}

	m_iState = STATE_TURN_OFF;

	// Apply the new speed, adjust sound pitch and volume.
	UpdateSpeed( flNewSpeed );

	// If we've met or exceeded target speed, stop spinning down.
	return bSpinDownDone;
}

//
// SpinDown - decelerates a moving func_rotating to a standstill.
//
void CFuncRotating :: SpinDown( void )
{
	// If we've met or exceeded target speed, stop spinning down.
	if( SpinDown( m_flTargetSpeed ))
	{
		if( m_iState != STATE_OFF )
		{
			SetMoveDone( &CFuncRotating::Rotate );
			Rotate();
		}
	}
	else
	{
		SetMoveDoneTime( GetNextMoveInterval() );
	}
}

void CFuncRotating :: Rotate( void )
{
	// NOTE: only full speed moving set state to "On"
	if( fabs( pev->speed ) == fabs( m_flMaxSpeed ))
		m_iState = STATE_ON;

	SetMoveDoneTime( 0.1f );

	if( m_bStopAtStartPos )
	{
		SetMoveDoneTime( GetNextMoveInterval() );

		int checkAxis = 2;

		// see if we got close to the starting orientation
		if( pev->movedir[0] != 0 )
		{
			checkAxis = 0;
		}
		else if( pev->movedir[1] != 0 )
		{
			checkAxis = 1;
		}

		float angDelta = anglemod( GetLocalAngles()[checkAxis] - m_angStart[checkAxis] );
		if( angDelta > 180.0f )
			angDelta -= 360.0f;

		Vector avel = GetLocalAvelocity();

		// delta per tick
		Vector avelpertick = avel * gpGlobals->frametime;

		if( fabs( angDelta ) < fabs( avelpertick[checkAxis] ))
		{
			SetTargetSpeed( 0 );
			SetLocalAngles( m_angStart );
			m_bStopAtStartPos = false;
		}
	}
}

void CFuncRotating :: RotateFriction( void )
{
	// angular impulse support
	if( GetLocalAvelocity() != g_vecZero )
	{
		m_iState = STATE_ON;
		SetMoveDoneTime( 0.1 );
		RampPitchVol();
	}
	else
	{
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise3 ));
		SetMoveDoneTime( -1 );
		m_iState = STATE_OFF;
	}
}

void CFuncRotating::ReverseMove( void )
{
	if( SpinDown( 0 ))
	{
		// we've reached zero - spin back up to the target speed.
		SetTargetSpeed( m_flTargetSpeed );
	}
	else
	{
		SetMoveDoneTime( GetNextMoveInterval() );
	}
}

void CFuncRotating::SetTargetSpeed( float flSpeed )
{
	if( flSpeed == 0.0f && FBitSet( pev->spawnflags, SF_ROTATING_STOP_AT_START_POS ))
		m_bStopAtStartPos = true;

	// make sure the sign is correct - positive for forward rotation,
	// negative for reverse rotation.
	flSpeed = fabs( flSpeed );

	if( pev->impulse )
	{
		flSpeed *= -1;
	}

	m_flTargetSpeed = flSpeed;
	pev->friction = 0.0f; // clear impulse friction

	// If we don't accelerate, change to the new speed instantly.
	if( !FBitSet( pev->spawnflags, SF_ROTATING_ACCDCC ))
	{
		UpdateSpeed( m_flTargetSpeed );
		SetMoveDone( &CFuncRotating::Rotate );
	}
	else
	{
		// Otherwise deal with acceleration/deceleration:
		if((( pev->speed > 0 ) && ( m_flTargetSpeed < 0 )) || (( pev->speed < 0 ) && ( m_flTargetSpeed > 0 )))
		{
			// check for reversing directions.
			SetMoveDone( &CFuncRotating::ReverseMove );
		}
		else if( fabs( pev->speed ) < fabs( m_flTargetSpeed ))
		{
			// If we are below the new target speed, spin up to the target speed.
			SetMoveDone( &CFuncRotating::SpinUp );
		}
		else if( fabs( pev->speed ) > fabs( m_flTargetSpeed ))
		{
			// If we are above the new target speed, spin down to the target speed.
			SetMoveDone( &CFuncRotating::SpinDown );
		}
		else
		{
			// we are already at the new target speed. Just keep rotating.
			SetMoveDone( &CFuncRotating::Rotate );
		}
	}

	SetMoveDoneTime( GetNextMoveInterval() );
}

//=========================================================
// Rotating Use - when a rotating brush is triggered
//=========================================================
void CFuncRotating :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	m_bStopAtStartPos = false;

	if( useType == USE_SET )
	{
		if( value != 0 ) 
		{
			// never toggle direction from momentary entities
			if( pCaller && !FClassnameIs( pCaller, "momentary_rot_button" ) && !FClassnameIs( pCaller, "momentary_rot_door" ))
				pev->impulse = (value < 0) ? true : false;
			value = fabs( value );
			SetTargetSpeed( bound( 0, value, 1 ) * m_flMaxSpeed );
		}
		else
		{
			SetTargetSpeed( 0 );
		}
		return;
	}

	// a liitle easter egg
	if( useType == USE_RESET )
	{
		if( value == 0.0f )
		{
			if( m_iState == STATE_OFF )
				pev->impulse = !pev->impulse;
			return;
		}

		if( m_iState == STATE_OFF )
		{
			// apply angular impulse (XashXT feature)
			SetLocalAvelocity( pev->movedir * (bound( -1, value, 1 ) * m_flMaxSpeed ));
			SetMoveDone( &CFuncRotating::RotateFriction );
			pev->friction = 1.0f;
			RotateFriction();
		}
		return;
	}

	if( pev->speed != 0 )
		SetTargetSpeed( 0 );
	else SetTargetSpeed( m_flMaxSpeed );
}

// rotatingBlocked - An entity has blocked the brush
void CFuncRotating :: Blocked( CBaseEntity *pOther )
{
	pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );

	if( gpGlobals->time < pev->dmgtime )
		return;

	pev->dmgtime = gpGlobals->time + 0.5f;
	UTIL_FireTargets( pev->target, this, this, USE_TOGGLE );
}

// =================== FUNC_PENDULUM ==============================================

#define SF_PENDULUM_START_ON		BIT( 0 )
#define SF_PENDULUM_SWING		BIT( 1 )
#define SF_PENDULUM_PASSABLE		BIT( 3 )
#define SF_PENDULUM_AUTO_RETURN	BIT( 4 )

class CPendulum : public CBaseDelay
{
	DECLARE_CLASS( CPendulum, CBaseDelay );
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Swing( void );
	void	Stop( void );
	void	RopeTouch( CBaseEntity *pOther ); // this touch func makes the pendulum a rope
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_HOLD_ANGLES; }
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Touch( CBaseEntity *pOther );
	void	Blocked( CBaseEntity *pOther );

	DECLARE_DATADESC();
	
	float	m_accel;	// Acceleration
	float	m_distance;
	float	m_time;
	float	m_damp;
	float	m_maxSpeed;
	float	m_dampSpeed;
	Vector	m_center;
	Vector	m_start;
};

LINK_ENTITY_TO_CLASS( func_pendulum, CPendulum );

BEGIN_DATADESC( CPendulum )
	DEFINE_FIELD( m_accel, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_distance, FIELD_FLOAT, "distance" ),
	DEFINE_FIELD( m_time, FIELD_TIME ),
	DEFINE_FIELD( m_damp, FIELD_FLOAT ),
	DEFINE_FIELD( m_maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_dampSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_center, FIELD_VECTOR ),
	DEFINE_FIELD( m_start, FIELD_VECTOR ),
	DEFINE_FUNCTION( Swing ),
	DEFINE_FUNCTION( Stop ),
	DEFINE_FUNCTION( RopeTouch ),
END_DATADESC()

void CPendulum :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "distance" ))
	{
		m_distance = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "damp" ))
	{
		m_damp = Q_atof( pkvd->szValue ) * 0.001;
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CPendulum :: Spawn( void )
{
	// set the axis of rotation
	CBaseToggle::AxisDir( pev );

	if( FBitSet( pev->spawnflags, SF_PENDULUM_PASSABLE ))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;

	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL( edict(), GetModel() );

	if( m_distance == 0 )
	{
		ALERT( at_error, "%s [%i] has invalid distance\n", GetClassname(), entindex());
		return;
	}

	if( pev->speed == 0 )
		pev->speed = 100;

	SetLocalAngles( m_vecTempAngles );

	// calculate constant acceleration from speed and distance
	m_accel = (pev->speed * pev->speed) / ( 2 * fabs( m_distance ));
	m_maxSpeed = pev->speed;
	m_start = GetLocalAngles();
	m_center = GetLocalAngles() + ( m_distance * 0.5f ) * pev->movedir;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );

	if( FBitSet( pev->spawnflags, SF_PENDULUM_START_ON ))
	{
		SetThink( &CBaseEntity::SUB_CallUseToggle );
		SetNextThink( 0.1 );
	}
	pev->speed = 0;

	if( FBitSet( pev->spawnflags, SF_PENDULUM_SWING ))
	{
		SetTouch( &CPendulum::RopeTouch );
	}
}

void CPendulum :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ) || m_distance == 0 )
		return;

	if( pev->speed )
	{
		// pendulum is moving, stop it and auto-return if necessary
		if( FBitSet( pev->spawnflags, SF_PENDULUM_AUTO_RETURN ))
		{		
			float delta = CBaseToggle::AxisDelta( pev->spawnflags, GetLocalAngles(), m_start );

			SetLocalAvelocity( m_maxSpeed * pev->movedir );
			SetNextThink( delta / m_maxSpeed );
			SetThink( &CPendulum::Stop );
		}
		else
		{
			pev->speed = 0; // dead stop
			SetThink( NULL );
			SetLocalAvelocity( g_vecZero );
			m_iState = STATE_OFF;
		}
	}
	else
	{
		SetNextThink( 0.1f );		// start the pendulum moving
		SetThink( &CPendulum::Swing );
		m_time = gpGlobals->time;		// save time to calculate dt
		m_dampSpeed = m_maxSpeed;
	}
}

void CPendulum :: Stop( void )
{
	SetLocalAngles( m_start );
	pev->speed = 0; // dead stop
	SetThink( NULL );
	SetLocalAvelocity( g_vecZero );
	m_iState = STATE_OFF;
}

void CPendulum::Blocked( CBaseEntity *pOther )
{
	m_time = gpGlobals->time;
}

void CPendulum :: Swing( void )
{
	float delta, dt;
	
	delta = CBaseToggle::AxisDelta( pev->spawnflags, GetLocalAngles(), m_center );

	dt = gpGlobals->time - m_time;	// how much time has passed?
	m_time = gpGlobals->time;		// remember the last time called

	// integrate velocity
	if( delta > 0 && m_accel > 0 )
		pev->speed -= m_accel * dt;
	else pev->speed += m_accel * dt;

	pev->speed = bound( -m_maxSpeed, pev->speed, m_maxSpeed );

	m_iState = STATE_ON;

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalAvelocity( pev->speed * pev->movedir );

	// call this again
	SetNextThink( 0.1f );
	SetMoveDoneTime( 0.1f );

	if( m_damp )
	{
		m_dampSpeed -= m_damp * m_dampSpeed * dt;

		if( m_dampSpeed < 30.0 )
		{
			SetLocalAngles( m_center );
			pev->speed = 0;
			SetThink( NULL );
			SetLocalAvelocity( g_vecZero );
			m_iState = STATE_OFF;
		}
		else if( pev->speed > m_dampSpeed )
		{
			pev->speed = m_dampSpeed;
		}
		else if( pev->speed < -m_dampSpeed )
		{
			pev->speed = -m_dampSpeed;
		}
	}
}

void CPendulum :: Touch ( CBaseEntity *pOther )
{
	if( pev->dmg <= 0 )
		return;

	// we can't hurt this thing, so we're not concerned with it
	if( !pOther->pev->takedamage )
		return;

	// calculate damage based on rotation speed
	float damage = pev->dmg * pev->speed * 0.01;

	if( damage < 0 ) damage = -damage;

	pOther->TakeDamage( pev, pev, damage, DMG_CRUSH );

	Vector vNewVel = (pOther->GetAbsOrigin( ) - GetAbsOrigin( )).Normalize();
	pOther->SetAbsVelocity( vNewVel * damage );
}

void CPendulum :: RopeTouch ( CBaseEntity *pOther )
{
	if( !pOther->IsPlayer( ))
	{
		// not a player!
		ALERT( at_console, "Not a client\n" );
		return;
	}

	if( pOther->edict() == pev->enemy )
	{
		// this player already on the rope.
		return;
	}

	pev->enemy = pOther->edict();
	pOther->SetAbsVelocity( g_vecZero );
	pOther->pev->movetype = MOVETYPE_NONE;
}

// =================== FUNC_CLOCK ==============================================

#define SECONDS_PER_MINUTE	60
#define SECONDS_PER_HOUR	3600
#define SECODNS_PER_DAY	43200

class CFuncClock : public CBaseDelay
{
	DECLARE_CLASS( CFuncClock, CBaseDelay );
public:
	void Spawn ( void );
	void Think( void );
	void Activate( void );
	void KeyValue( KeyValueData *pkvd );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();
private:
	Vector	m_vecCurtime;
	Vector	m_vecFinaltime;
	float	m_flCurTime;	// current unconverted time in seconds
	int	m_iClockType;	// also contain seconds count for different modes
	int	m_iHoursCount;
	BOOL	m_fInit;		// this clock already init
};

LINK_ENTITY_TO_CLASS( func_clock, CFuncClock );

BEGIN_DATADESC( CFuncClock )
	DEFINE_FIELD( m_vecCurtime, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecFinaltime, FIELD_VECTOR ),
	DEFINE_FIELD( m_flCurTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_iClockType, FIELD_INTEGER ),
	DEFINE_FIELD( m_iHoursCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_fInit, FIELD_BOOLEAN ),
END_DATADESC()

void CFuncClock :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "type" ))
	{
		switch( Q_atoi( pkvd->szValue ))
		{
		case 1:
			m_iClockType = SECONDS_PER_HOUR;
			break;
		case 2:
			m_iClockType = SECODNS_PER_DAY;
			break;
		default:
			m_iClockType = SECONDS_PER_MINUTE;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "curtime" ))
	{
		m_vecCurtime = Q_atov( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "event" ))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

void CFuncClock :: Spawn ( void )
{
	CBaseToggle::AxisDir( pev );

	m_iState = STATE_ON;
	pev->solid = SOLID_NOT;

	SET_MODEL( edict(), GetModel() );
	
	if( m_iClockType == SECODNS_PER_DAY )
	{
		// normalize our time
		if( m_vecCurtime.x > 11 ) m_vecCurtime.x = 0;
		if( m_vecCurtime.y > 59 ) m_vecCurtime.y = 0;
		if( m_vecCurtime.z > 59 ) m_vecCurtime.z = 0;
		
		// member full hours
		m_iHoursCount = m_vecCurtime.x;
		
		// calculate seconds
		m_vecFinaltime.z = m_vecCurtime.z * ( SECONDS_PER_MINUTE / 60 );
                    m_vecFinaltime.y = m_vecCurtime.y * ( SECONDS_PER_HOUR / 60 ) + m_vecFinaltime.z;
                    m_vecFinaltime.x = m_vecCurtime.x * ( SECODNS_PER_DAY / 12 ) + m_vecFinaltime.y;
	}
}

void CFuncClock :: Activate( void )
{
	if( m_fInit ) return;

	if( m_iClockType == SECODNS_PER_DAY && m_vecCurtime != g_vecZero )
	{
		// try to find minutes and seconds entity
		CBaseEntity *pEntity = NULL;
		while( ( pEntity = UTIL_FindEntityInSphere( pEntity, GetLocalOrigin(), pev->size.z )))
		{
			if( FClassnameIs( pEntity, "func_clock" ))
			{
				CFuncClock *pClock = (CFuncClock *)pEntity;
				// write start hours, minutes and seconds
				switch( pClock->m_iClockType )
				{
				case SECODNS_PER_DAY:
					// NOTE: here we set time for himself through FindEntityInSphere
					pClock->m_flCurTime = m_vecFinaltime.x;
					break;
				case SECONDS_PER_HOUR:
					pClock->m_flCurTime = m_vecFinaltime.y;
					break;
				default:
					pClock->m_flCurTime = m_vecFinaltime.z;
					break;
				}
			}
		}
	}

	// clock start	
	SetNextThink( 0 );
	m_fInit = 1;
}

void CFuncClock :: Think( void )
{
	float seconds, ang, pos;

	seconds = gpGlobals->time + m_flCurTime;
	pos = seconds / m_iClockType;
	pos = pos - floor( pos );
	ang = 360 * pos;

	SetLocalAngles( pev->movedir * ang );

	if( m_iClockType == SECODNS_PER_DAY )
	{
		int hours = GetLocalAngles().Length() / 30;
		if( m_iHoursCount != hours )
		{
			// member new hour
			m_iHoursCount = hours;
			if( hours == 0 ) hours = 12;	// merge for 0.00.00

			// send hours info
			UTIL_FireTargets( pev->netname, this, this, USE_SET, hours );
			UTIL_FireTargets( pev->netname, this, this, USE_ON );
		}
	}

	RelinkEntity( FALSE );

	// set clock resolution
	SetNextThink( 1.0f );
}

// =================== FUNC_LIGHT ==============================================

#define SF_LIGHT_START_ON		BIT( 0 )

class CFuncLight : public CBreakable
{
	DECLARE_CLASS( CFuncLight, CBreakable );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void Flicker( void );
	void Die( void );

	DECLARE_DATADESC();
private:
	int	m_iFlickerMode;
	Vector	m_vecLastDmgPoint;
	float	m_flNextFlickerTime;
};
LINK_ENTITY_TO_CLASS( func_light, CFuncLight );

BEGIN_DATADESC( CFuncLight )
	DEFINE_FIELD( m_iFlickerMode, FIELD_INTEGER ),
	DEFINE_FIELD( m_flNextFlickerTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecLastDmgPoint, FIELD_POSITION_VECTOR ),
	DEFINE_FUNCTION( Flicker ),
END_DATADESC()

void CFuncLight :: Spawn( void )
{	
	m_Material = matGlass;

	CBreakable::Spawn();
	SET_MODEL( edict(), GetModel() );

	// probably map compiler haven't func_light support
	if( m_iStyle <= 0 || m_iStyle >= 256 )
	{
		if( GetTargetname()[0] )
			ALERT( at_error, "%s with name %s has bad lightstyle %i. Disabled\n", GetClassname(), GetTargetname(), m_iStyle );
		else ALERT( at_error, "%s [%i] has bad lightstyle %i. Disabled\n", GetClassname(), entindex(), m_iStyle );
		m_iState = STATE_DEAD; // lamp is dead
	}
	
	if( FBitSet( pev->spawnflags, SF_LIGHT_START_ON ))
		Use( this, this, USE_ON, 0 );
	else Use( this, this, USE_OFF, 0 );

	if( pev->health <= 0 )
		pev->takedamage = DAMAGE_NO;
	else pev->takedamage = DAMAGE_YES;
}

void CFuncLight :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	
	if( IsLockedByMaster( ))
		return;

	if( m_iState == STATE_DEAD )
		return; // lamp is broken

	if( useType == USE_TOGGLE )
	{
		if( m_iState == STATE_OFF )
			useType = USE_ON;
		else useType = USE_OFF;
	}

	if( useType == USE_ON )
	{
		if( m_flDelay )
		{
			// make flickering delay
			m_iState = STATE_TURN_ON;
			LIGHT_STYLE( m_iStyle, "mmamammmmammamamaaamammma" );
			pev->frame = 0; // light texture is on
			SetThink( &CFuncLight::Flicker );
			SetNextThink( m_flDelay );
		}
		else
		{         // instant enable
			m_iState = STATE_ON;
			LIGHT_STYLE( m_iStyle, "k" );
			pev->frame = 0; // light texture is on
			UTIL_FireTargets( pev->target, this, this, USE_ON );
		}
	}
	else if( useType == USE_OFF )
	{
		LIGHT_STYLE( m_iStyle, "a" );
		UTIL_FireTargets( pev->target, this, this, USE_OFF );
		pev->frame = 1;// light texture is off
		m_iState = STATE_OFF;
	}
	else if( useType == USE_SET )
	{
		// a script die (dramatic effect)
		Die();
	}
}

void CFuncLight :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if( pev->takedamage == DAMAGE_NO )
		return;

	if( m_iState == STATE_DEAD )
		return;

	const char *pTextureName = NULL;
	Vector start = pevAttacker->origin + pevAttacker->view_ofs;
	Vector end = start + vecDir * 1024;
	edict_t *pHit = ptr->pHit;

	if( pHit )
		pTextureName = TRACE_TEXTURE( pHit, start, end );

	if( pTextureName != NULL && ( !Q_strncmp( pTextureName, "+0~", 3 ) || *pTextureName == '~' ))
	{
		// take damage only at light texture
		UTIL_Sparks( ptr->vecEndPos );
		m_vecLastDmgPoint = ptr->vecEndPos;
	}

	CBreakable::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int CFuncLight :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if( bitsDamageType & DMG_BLAST )
		flDamage *= 3.0f;
	else if( bitsDamageType & DMG_CLUB )
		flDamage *= 2.0f;
	else if( bitsDamageType & DMG_SONIC )
		flDamage *= 1.2f;
	else if( bitsDamageType & DMG_SHOCK )
		flDamage *= 10.0f;//!!!! over voltage
	else if( bitsDamageType & DMG_BULLET )
		flDamage *= 0.5f; // half damage at bullet

	pev->health -= flDamage;

	if( pev->health <= 0.0f )
	{
		Die();
		return 0;
	}

	CBreakable::DamageSound();

	return 1;
}

void CFuncLight :: Die( void )
{
	// lamp is random choose die style
	if( m_iState == STATE_OFF )
	{
		pev->frame = 1; // light texture is off
		LIGHT_STYLE( m_iStyle, "a" );
		DontThink();
	}
	else
	{         // simple randomization
		m_iFlickerMode = RANDOM_LONG( 1, 2 );
		SetThink( &CFuncLight::Flicker );
		SetNextThink( 0.1f + RANDOM_LONG( 0.1f, 0.2f ));
	}

	m_iState = STATE_DEAD;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;
	UTIL_FireTargets( pev->target, this, this, USE_OFF );

	switch( RANDOM_LONG( 0, 1 ))
	{
	case 0:
		EMIT_SOUND( edict(), CHAN_VOICE, "debris/bustglass1.wav", 0.7, ATTN_IDLE );
		break;
	case 1:
		EMIT_SOUND( edict(), CHAN_VOICE, "debris/bustglass2.wav", 0.8, ATTN_IDLE );
		break;
	}

	Vector vecSpot = GetAbsOrigin() + (pev->mins + pev->maxs) * 0.5f;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_BREAKMODEL );
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );
		WRITE_COORD( pev->size.x );
		WRITE_COORD( pev->size.y );
		WRITE_COORD( pev->size.z );
		WRITE_COORD( 0 ); 
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_BYTE( 10 ); 
		WRITE_SHORT( m_idShard );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 25 );
		WRITE_BYTE( BREAK_GLASS );
	MESSAGE_END();
}

void CFuncLight :: Flicker( void )
{
	if( m_iState == STATE_TURN_ON )
	{
		LIGHT_STYLE( m_iStyle, "k" );
		UTIL_FireTargets( pev->target, this, this, USE_ON );
		m_iState = STATE_ON;
		DontThink();
		return;
	}

	if( m_iFlickerMode == 1 )
	{
		pev->frame = 1;
		LIGHT_STYLE( m_iStyle, "a" );
		SetThink( NULL );
		return;
	}

	if( m_iFlickerMode == 2 )
	{
		switch( RANDOM_LONG( 0, 3 ))
		{
		case 0:
			LIGHT_STYLE( m_iStyle, "abcaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
			break;
		case 1:
			LIGHT_STYLE( m_iStyle, "acaaabaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
			break;
		case 2:
			LIGHT_STYLE( m_iStyle, "aaafbaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
			break;
		case 3:
			LIGHT_STYLE( m_iStyle, "aaaaaaaaaaaaagaaaaaaaaacaaaacaaaa" );
			break;
		}		

		m_flNextFlickerTime = RANDOM_FLOAT( 0.5f, 10.0f );
                   	UTIL_Sparks( m_vecLastDmgPoint );

		switch( RANDOM_LONG( 0, 2 ))
		{
		case 0:
			EMIT_SOUND( edict(), CHAN_VOICE, "buttons/spark1.wav", 0.4, ATTN_IDLE );
			break;
		case 1:
			EMIT_SOUND( edict(), CHAN_VOICE, "buttons/spark2.wav", 0.3, ATTN_IDLE );
			break;
		case 2:
			EMIT_SOUND( edict(), CHAN_VOICE, "buttons/spark3.wav", 0.35, ATTN_IDLE );
			break;
		}

		if( m_flNextFlickerTime > 6.5f )
			m_iFlickerMode = 1; // stop sparking
	}

	SetNextThink( m_flNextFlickerTime ); 
}
