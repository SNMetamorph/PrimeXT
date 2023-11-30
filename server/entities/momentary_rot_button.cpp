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

#include "momentary_rot_button.h"

BEGIN_DATADESC( CMomentaryRotButton )
	DEFINE_FIELD( m_bUpdateTarget, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_lastUsed, FIELD_INTEGER ),
	DEFINE_FIELD( m_direction, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_startPosition, FIELD_FLOAT, "m_flStartPos" ),
	DEFINE_KEYFIELD( m_returnSpeed, FIELD_FLOAT, "returnspeed" ),
	DEFINE_KEYFIELD( m_sounds, FIELD_STRING, "sounds" ),
	DEFINE_FIELD( m_start, FIELD_VECTOR ),
	DEFINE_FIELD( m_end, FIELD_VECTOR ),
	DEFINE_FUNCTION( ButtonUse ),
	DEFINE_FUNCTION( UseMoveDone ),
	DEFINE_FUNCTION( ReturnMoveDone ),
	DEFINE_FUNCTION( SetPositionMoveDone ),
	DEFINE_FUNCTION( UpdateThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( momentary_rot_button, CMomentaryRotButton );
LINK_ENTITY_TO_CLASS( momentary_rot_door, CMomentaryRotButton );

void CMomentaryRotButton :: Precache( void )
{
	const char *pszSound;

	pszSound = UTIL_ButtonSound( m_sounds );
	pev->noise = UTIL_PrecacheSound( pszSound );
}

void CMomentaryRotButton :: Spawn( void )
{
	// g-cont. just to have two seperate entities
	if( FClassnameIs( pev, "momentary_rot_door" ))
		SetBits( pev->spawnflags, SF_MOMENTARY_ROT_DOOR );

	Precache();

	AxisDir( pev );

	m_bUpdateTarget = true;

	if( pev->speed == 0 )
		pev->speed = 100;

	m_startPosition = bound( 0.0f, m_startPosition, 1.0f );

	if( m_flMoveDistance < 0 )
	{
		pev->movedir = pev->movedir * -1;
		m_flMoveDistance = -m_flMoveDistance;
	}

	m_direction = -1;

	m_start = GetLocalAngles() - pev->movedir * m_flMoveDistance * m_startPosition;
	m_end = GetLocalAngles() + pev->movedir * m_flMoveDistance * (1.0f - m_startPosition);

	pev->ideal_yaw = m_startPosition;
	m_iState = STATE_OFF;

	if( FBitSet( pev->spawnflags, SF_MOMENTARY_ROT_DOOR ))
		pev->solid = SOLID_BSP;
	else pev->solid = SOLID_NOT;

	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL( edict(), GetModel() );
	UTIL_SetOrigin( this, GetLocalOrigin( ));
	m_lastUsed = 0;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
	SetUse( &CMomentaryRotButton::ButtonUse );
}

void CMomentaryRotButton :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "returnspeed" ))
	{
		m_returnSpeed = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flStartPos" ))
	{
		m_startPosition = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "sounds"))
	{
		m_sounds = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CMomentaryRotButton :: PlaySound( void )
{
	EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise ), 1, ATTN_NORM );
}

float CMomentaryRotButton::GetPos( const Vector &vecAngles )
{
	float flScale;

	if(( pev->movedir.x < 0 ) || ( pev->movedir.y < 0 ) || ( pev->movedir.z < 0 ))
		flScale = -1;
	else flScale = 1;

	float flPos = flScale * AxisDelta( pev->spawnflags, vecAngles, m_start ) / m_flMoveDistance;

	return bound( 0.0f, flPos, 1.0f );
}

void CMomentaryRotButton :: SetPosition( float value )
{
	pev->ideal_yaw = bound( 0.0f, value, 1 );

	float flCurPos = GetPos( GetLocalAngles( ));

	if( flCurPos < pev->ideal_yaw )
	{
		// moving forward (from start to end).
		SetLocalAvelocity( pev->speed * pev->movedir );
		m_direction = 1;
	}
	else if( flCurPos > pev->ideal_yaw )
	{
		// moving backward (from end to start).
		SetLocalAvelocity( -pev->speed * pev->movedir );
		m_direction = -1;
	}
	else
	{
		// we're there already; nothing to do.
		SetLocalAvelocity( g_vecZero );
		return;
	}

	// g-cont. to avoid moving by user in back direction
	if( FBitSet( pev->spawnflags, SF_MOMENTARY_ROT_BUTTON_AUTO_RETURN ) && m_returnSpeed > 0 )
		m_lastUsed = 1;

	// play sound on set new pos
	PlaySound();

	SetMoveDone( &CMomentaryRotButton::SetPositionMoveDone );
	SetThink( &CMomentaryRotButton::UpdateThink );
	SetNextThink( 0 );

	// Think again in 0.1 seconds or the time that it will take us to reach our movement goal,
	// whichever is the shorter interval. This prevents us from overshooting and stuttering when we
	// are told to change position in very small increments.
	Vector vecNewAngles = m_start + pev->movedir * ( pev->ideal_yaw * m_flMoveDistance );
	float flAngleDelta = fabs( AxisDelta( pev->spawnflags, vecNewAngles, GetLocalAngles( )));
	float dt = flAngleDelta / pev->speed;

	if( dt < gpGlobals->frametime )
	{
		dt = gpGlobals->frametime;
		float speed = flAngleDelta / gpGlobals->frametime;
		SetLocalAvelocity( speed * pev->movedir * m_direction );
	}

	dt = bound( gpGlobals->frametime, dt, gpGlobals->frametime * 6 );

	SetMoveDoneTime( dt );
}

void CMomentaryRotButton :: ButtonUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator )) return;

	// always update target for direct use
	m_bUpdateTarget = true;

	if( useType == USE_SET )
	{
		if( pActivator->IsPlayer( ))
		{
			CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

			if( pPlayer->m_afPhysicsFlags & PFLAG_USING )
				UpdateAllButtons();
			else SetPosition( value );	// just targetting "from player" indirect
		}
		else SetPosition( value );	// not from player
	}
	else if( useType == USE_RESET )
	{
		// stop moving immediately
		UseMoveDone();
	}
}

void CMomentaryRotButton :: UpdateAllButtons( void )
{
	// NOTE: all the momentary buttons linked with same targetname
	// will be updated only in forward direction
	// in backward direction each button uses private AutoReturn code

	// update all rot buttons attached to my target
	CBaseEntity *pTarget = NULL;

	while( 1 )
	{
		pTarget = UTIL_FindEntityByTarget( pTarget, STRING( pev->target ));

		if( FNullEnt( pTarget ))
			break;

		if( FClassnameIs( pTarget->pev, "momentary_rot_button" ) || FClassnameIs( pTarget->pev, "momentary_rot_door" ))
		{
			CMomentaryRotButton *pEntity = (CMomentaryRotButton *)pTarget;

			if( pEntity != this )
			{
				// indirect use. disable update targets to avoid fire it twice
				// and prevent possible recursion
				pEntity->m_bUpdateTarget = false;
			}
			pEntity->UpdateButton();
		}
	}
}

void CMomentaryRotButton :: UpdateButton( void )
{
	// reverse our direction and play movement sound every time the player
	// pauses between uses.
	bool bPlaySound = false;
	
	if( !m_lastUsed )
	{
		bPlaySound = true;
		m_direction = -m_direction;
	}

	m_lastUsed = 1;

	float flPos = GetPos( GetLocalAngles() );
	UpdateSelf( flPos, bPlaySound );

	// Think every frame while we are moving.
	// HACK: Don't reset the think time if we already have a pending think.
	// This works around an issue with host_thread_mode > 0 when the player's
	// clock runs ahead of the server.
	if( !m_pfnThink )
	{
		SetThink( &CMomentaryRotButton::UpdateThink );
		SetNextThink( 0 );
	}
}

void CMomentaryRotButton :: UpdateThink( void )
{
	float value = GetPos( GetLocalAngles( ));
	UpdateTarget( value );
	SetNextThink( 0 );
}

void CMomentaryRotButton :: UpdateTarget( float value )
{
	if( !FStringNull( pev->target ) && m_bUpdateTarget )
	{
		CBaseEntity *pTarget = NULL;
		while( 1 )
		{
			pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ));

			if( FNullEnt( pTarget ))
				break;

			pTarget->Use( this, this, USE_SET, value );
		}
	}
}

void CMomentaryRotButton :: UpdateSelf( float value, bool bPlaySound )
{
	// set our move clock to 0.1 seconds in the future so we stop spinning unless we are
	// used again before then.
	SetMoveDoneTime( 0.1 );

	if( m_direction > 0 && value >= 1.0 )
	{
		// if we hit the end, zero our avelocity and snap to the end angles.
		SetLocalAvelocity( g_vecZero );
		SetLocalAngles( m_end );
		m_iState = STATE_ON;
		return;
	}
	else if( m_direction < 0 && value <= 0 )
	{
		// if we returned to the start, zero our avelocity and snap to the start angles.
		SetLocalAvelocity( g_vecZero );
		SetLocalAngles( m_start );
		m_iState = STATE_OFF;
		return;
	}

	// i'm "in use" player turn me :-)
	m_iState = STATE_IN_USE;
	
	if( bPlaySound ) PlaySound();

	SetLocalAvelocity(( m_direction * pev->speed ) * pev->movedir );
	SetMoveDone( &CMomentaryRotButton::UseMoveDone );
}

void CMomentaryRotButton :: UseMoveDone( void )
{
	SetLocalAvelocity( g_vecZero );

	// make sure our targets stop where we stopped.
	float flPos = GetPos( GetLocalAngles( ));
	UpdateTarget( flPos );

	m_lastUsed = 0;

	if( FBitSet( pev->spawnflags, SF_MOMENTARY_ROT_BUTTON_AUTO_RETURN ) && m_returnSpeed > 0 )
	{
		SetMoveDone( &CMomentaryRotButton::ReturnMoveDone );
		m_direction = -1;

		if( flPos >= 1.0f )
		{
			// disable use until button is waiting
			SetUse( NULL );

			// delay before autoreturn.
			SetMoveDoneTime( m_flDelay + 0.1f );
		}
		else SetMoveDoneTime( 0.1f );
	}
	else
	{
		SetThink( NULL );
		SetMoveDone( NULL );
	}
}

void CMomentaryRotButton :: ReturnMoveDone( void )
{
	float value = GetPos( GetLocalAngles() );

	SetUse( &CMomentaryRotButton::ButtonUse );

	if( value <= 0 )
	{
		// Got back to the start, stop spinning.
		SetLocalAvelocity( g_vecZero );
		SetLocalAngles( m_start );

		m_iState = STATE_OFF;

		UpdateTarget( 0 );

		SetMoveDoneTime( -1 );
		SetMoveDone( NULL );

		SetNextThink( -1 );
		SetThink( NULL );
	}
	else
	{
		m_iState = STATE_TURN_OFF;

		SetLocalAvelocity( -m_returnSpeed * pev->movedir );
		SetMoveDoneTime( 0.1f );

		SetThink( &CMomentaryRotButton::UpdateThink );
		SetNextThink( 0.01f );
	}
}

void CMomentaryRotButton :: SetPositionMoveDone( void )
{
	float flCurPos = GetPos( GetLocalAngles( ));

	if((( flCurPos >= pev->ideal_yaw ) && ( m_direction == 1 )) || (( flCurPos <= pev->ideal_yaw ) && ( m_direction == -1 )))
	{
		// g-cont. we need auto return after direct set position?
		if( FBitSet( pev->spawnflags, SF_MOMENTARY_ROT_BUTTON_AUTO_RETURN ) && m_returnSpeed > 0 )
		{
			SetMoveDone( &CMomentaryRotButton::ReturnMoveDone );
			m_direction = -1;

			if( flCurPos >= 1.0f )
			{
				// disable use until button is waiting
				SetUse( NULL );

				// delay before autoreturn.
				SetMoveDoneTime( m_flDelay + 0.1f );
			}
			else SetMoveDoneTime( 0.1f );
		}
		else
		{
			m_iState = STATE_OFF;

			// we reached or surpassed our movement goal.
			SetLocalAvelocity( g_vecZero );

			// BUGBUG: Won't this get the player stuck?
			SetLocalAngles( m_start + pev->movedir * ( pev->ideal_yaw * m_flMoveDistance ));
			SetNextThink( -1 );
			SetMoveDoneTime( -1 );
			UpdateTarget( pev->ideal_yaw );
		}

		m_lastUsed = 0;
		return;
	}

	// set right state
	if( flCurPos >= pev->ideal_yaw )
		m_iState = STATE_TURN_OFF;

	if( flCurPos <= pev->ideal_yaw )
		m_iState = STATE_TURN_ON;

	Vector vecNewAngles = m_start + pev->movedir * ( pev->ideal_yaw * m_flMoveDistance );
	float flAngleDelta = fabs( AxisDelta( pev->spawnflags, vecNewAngles, GetLocalAngles( )));
	float dt = flAngleDelta / pev->speed;

	if( dt < gpGlobals->frametime )
	{
		dt = gpGlobals->frametime;
		float speed = flAngleDelta / gpGlobals->frametime;
		SetLocalAvelocity( speed * pev->movedir * m_direction );
	}

	dt = bound( gpGlobals->frametime, dt, gpGlobals->frametime * 6 );

	SetMoveDoneTime( dt );
}

CMomentaryRotButton *CMomentaryRotButton::Instance(edict_t *pent)
{
	return (CMomentaryRotButton *)GET_PRIVATE(pent);
}

float CMomentaryRotButton::GetPosition(void)
{
	return GetPos(GetLocalAngles());
}

int CMomentaryRotButton::ObjectCaps(void)
{
	int flags = (CBaseToggle::ObjectCaps() & (~FCAP_ACROSS_TRANSITION));
	if (FBitSet(pev->spawnflags, SF_MOMENTARY_ROT_DOOR))
		return flags;
	return flags | FCAP_CONTINUOUS_USE;
}