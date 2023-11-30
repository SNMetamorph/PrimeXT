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

#include "momentary_door.h"	// for func_traindoor

LINK_ENTITY_TO_CLASS( momentary_door, CMomentaryDoor );

BEGIN_DATADESC( CMomentaryDoor )
	DEFINE_KEYFIELD( m_iMoveSnd, FIELD_STRING, "movesnd" ),
	DEFINE_FUNCTION( StopMoveSound ),
END_DATADESC()

void CMomentaryDoor :: Precache( void )
{
	int m_sound = UTIL_LoadSoundPreset( m_iMoveSnd );

	// set the door's "in-motion" sound
	switch( m_sound )
	{
	case 1:
		pev->noise = UTIL_PrecacheSound( "doors/doormove1.wav" );
		break;
	case 2:
		pev->noise = UTIL_PrecacheSound( "doors/doormove2.wav" );
		break;
	case 3:
		pev->noise = UTIL_PrecacheSound( "doors/doormove3.wav" );
		break;
	case 4:
		pev->noise = UTIL_PrecacheSound( "doors/doormove4.wav" );
		break;
	case 5:
		pev->noise = UTIL_PrecacheSound( "doors/doormove5.wav" );
		break;
	case 6:
		pev->noise = UTIL_PrecacheSound( "doors/doormove6.wav" );
		break;
	case 7:
		pev->noise = UTIL_PrecacheSound( "doors/doormove7.wav" );
		break;
	case 8:
		pev->noise = UTIL_PrecacheSound( "doors/doormove8.wav" );
		break;
	case 0:
		pev->noise = MAKE_STRING( "common/null.wav" );
		break;
	default:
		pev->noise = UTIL_PrecacheSound( m_sound );
		break;
	}
}

void CMomentaryDoor :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "movesnd" ))
	{
		m_iMoveSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CMomentaryDoor :: Spawn( void )
{
	if( FBitSet( pev->spawnflags, SF_DOOR_PASSABLE ))
		pev->solid = SOLID_NOT;
	else
		pev->solid = SOLID_BSP;

	pev->movetype = MOVETYPE_PUSH;

	if( pev->speed <= 0 )
		pev->speed = 100;

	if( pev->movedir == g_vecZero )
		pev->movedir = Vector( 1.0f, 0.0f, 0.0f );

	Precache();
	SetTouch( NULL );

	SET_MODEL( edict(), GetModel() );

	// if move distance is set to zero, use with width of the 
	// brush to determine the size of the move distance
	if( m_flMoveDistance <= 0 )
	{
		Vector vecSize = pev->size - Vector( 2, 2, 2 );
		m_flMoveDistance = DotProductAbs( pev->movedir, vecSize ) - m_flLip;
	}

	m_vecPosition1 = GetLocalOrigin();
	m_vecPosition2 = m_vecPosition1 + (pev->movedir * m_flMoveDistance);

	if( FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ))
	{	
		UTIL_SetOrigin( this, m_vecPosition2 );

		// NOTE: momentary door doesn't have absolute positions
		// so we need swap it here for right movement
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = GetLocalOrigin();
	}
	else
	{
		UTIL_SetOrigin( this, m_vecPosition1 );
	}

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
	m_iState = STATE_OFF;
}

void CMomentaryDoor :: OnChangeParent( void )
{
	matrix4x4 parent = GetParentToWorldTransform();
	m_vecPosition1 = parent.VectorITransform( m_vecPosition1 );
	m_vecPosition2 = parent.VectorITransform( m_vecPosition2 );
}

void CMomentaryDoor :: OnClearParent( void )
{
	matrix4x4 parent = GetParentToWorldTransform();
	m_vecPosition1 = parent.VectorTransform( m_vecPosition1 );
	m_vecPosition2 = parent.VectorTransform( m_vecPosition2 );
}

void CMomentaryDoor :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( IsLockedByMaster( pActivator ))
		return;

	if( useType == USE_RESET )
	{
		// stop moving immediately
		MoveDone();
		return;
	}

	float speed = 0.0f;

	// allow on-off mode that simulate standard door
	if( FBitSet( pev->spawnflags, SF_DOOR_ONOFF_MODE ))
	{
		if( useType == USE_ON )
		{
			useType = USE_SET;
			value = 1.0f;
		}
		else if( useType == USE_OFF )
		{
			useType = USE_SET;
			value = 0.0f;
		}
	}

	// momentary buttons will pass down a float in here
	if( useType != USE_SET ) return;

	value = bound( 0.0f, value, 1.0f );
	Vector move = m_vecPosition1 + (value * ( m_vecPosition2 - m_vecPosition1 ));

	// NOTE: historically momentary_door is completely ignore pev->speed
	// and get local speed that based on distance between new and current position
	// with interval 0.1 secs. This flag is allow constant speed like for func_door
	if( FBitSet( pev->spawnflags, SF_DOOR_CONSTANT_SPEED ))
	{
		speed = pev->speed;
	}
	else
	{	
		// classic Valve method to determine speed
		Vector delta = move - GetLocalOrigin();
		speed = delta.Length() * 10;
	}

	if( move != g_vecZero )
	{
		// This entity only thinks when it moves, so if it's thinking, it's in the process of moving
		// play the sound when it starts moving
		if( m_iState == STATE_OFF ) EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ), 1, ATTN_NORM );

		m_iState = STATE_ON; // we are "in-moving"

		// clear think (that stops sounds)
		SetThink( NULL );

		LinearMove( move, speed );
	}
}

void CMomentaryDoor :: Blocked( CBaseEntity *pOther )
{
	// hurt the blocker a little.
	if( pev->dmg ) pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );
}

void CMomentaryDoor :: MoveDone( void )
{
	// stop sounds at the next think, rather than here as another
	// SetPosition call might immediately follow the end of this move
	SetThink( &CMomentaryDoor::StopMoveSound );
	SetNextThink( 0.1f );

	CBaseToggle::MoveDone();

	if( GetLocalOrigin() == m_vecPosition2 )
	{
		SUB_UseTargets( m_hActivator, USE_ON, 0 );
	}
	else if( GetLocalOrigin() == m_vecPosition1 )
	{
		SUB_UseTargets( m_hActivator, USE_OFF, 0 );
	}
}

void CMomentaryDoor :: StopMoveSound( void )
{
	STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));
	m_iState = STATE_OFF; // really off
	SetNextThink( -1 );
	SetThink( NULL );
}