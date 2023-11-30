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

#include "func_traindoor.h"	// for func_traindoor

LINK_ENTITY_TO_CLASS( func_traindoor, CBaseTrainDoor );

BEGIN_DATADESC( CBaseTrainDoor )
	DEFINE_KEYFIELD( m_iMoveSnd, FIELD_STRING, "movesnd" ),
	DEFINE_KEYFIELD( m_iStopSnd, FIELD_STRING, "stopsnd" ),
	DEFINE_FIELD( m_vecOldAngles, FIELD_VECTOR ),
	DEFINE_FIELD( door_state, FIELD_CHARACTER ),
	DEFINE_FIELD( m_pTrain, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_vecPosition1, FIELD_VECTOR ), // all traindoor positions are local
	DEFINE_FIELD( m_vecPosition2, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecPosition3, FIELD_VECTOR ),
	DEFINE_FUNCTION( FindTrain ),
	DEFINE_FUNCTION( DoorGoUp ),
	DEFINE_FUNCTION( DoorGoDown ),
	DEFINE_FUNCTION( DoorHitTop ),
	DEFINE_FUNCTION( DoorSlideUp ),
	DEFINE_FUNCTION( DoorSlideDown ),
	DEFINE_FUNCTION( DoorSlideWait ),		// wait before sliding
	DEFINE_FUNCTION( DoorHitBottom ),
	DEFINE_FUNCTION( ActivateTrain ),
END_DATADESC()

void CBaseTrainDoor :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "movesnd" ))
	{
		m_iMoveSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "stopsnd" ))
	{
		m_iStopSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "train" ))
	{
		// g-cont. just a replace movewith name
		m_iParent = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CBaseTrainDoor :: Spawn( void )
{
	Precache();

	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;

	SET_MODEL( edict(), GetModel( ));

	// is mapper forget set angles?
	if( pev->movedir == g_vecZero )
	{
		pev->movedir = Vector( 1, 0, 0 );
		SetLocalAngles( g_vecZero );
	}

	// save initial angles
	m_vecOldAngles = GetLocalAngles();	// ???
	m_vecPosition1 = GetLocalOrigin();	// member the initial position

	if( !pev->speed )
		pev->speed = 100;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
	door_state = TD_CLOSED;
	SetTouch( NULL );

	SetThink( &CBaseTrainDoor :: FindTrain );
	SetNextThink( 0.1 );
}

void CBaseTrainDoor :: FindTrain( void )
{
	CBaseEntity *pEntity = NULL;

	while(( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( m_iParent ))) != NULL )
	{
		// found the tracktrain
		if( FClassnameIs( pEntity->pev, "func_tracktrain" ) )
		{
			m_pTrain = (CFuncTrackTrain *)pEntity;
			m_pTrain->SetTrainDoor( this ); // tell train about door
			break;
		}
	}
}

void CBaseTrainDoor :: OverrideReset( void )
{
	FindTrain( );
}

void CBaseTrainDoor::Precache( void )
{
	int m_sound = UTIL_LoadSoundPreset( m_iMoveSnd );

	// set the door's "in-motion" sound
	switch( m_sound )
	{
	case 1:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove1.wav" );
		break;
	case 2:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove2.wav" );
		break;
	case 3:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove3.wav" );
		break;
	case 4:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove4.wav" );
		break;
	case 5:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove5.wav" );
		break;
	case 6:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove6.wav" );
		break;
	case 7:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove7.wav" );
		break;
	case 8:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove8.wav" );
		break;
	case 9:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove9.wav" );
		break;
	case 10:
		pev->noise1 = UTIL_PrecacheSound( "doors/doormove10.wav" );
		break;
	case 0:
		pev->noise1 = MAKE_STRING( "common/null.wav" );
		break;
	default:
		pev->noise1 = UTIL_PrecacheSound( m_sound );
		break;
	}

	m_sound = UTIL_LoadSoundPreset( m_iStopSnd );

	// set the door's 'reached destination' stop sound
	switch( m_sound )
	{
	case 1:
		pev->noise2 = UTIL_PrecacheSound( "doors/doorstop1.wav" );
		break;
	case 2:
		pev->noise2 = UTIL_PrecacheSound( "doors/doorstop2.wav" );
		break;
	case 3:
		pev->noise2 = UTIL_PrecacheSound( "doors/doorstop3.wav" );
		break;
	case 4:
		pev->noise2 = UTIL_PrecacheSound( "doors/doorstop4.wav" );
		break;
	case 5:
		pev->noise2 = UTIL_PrecacheSound( "doors/doorstop5.wav" );
		break;
	case 6:
		pev->noise2 = UTIL_PrecacheSound( "doors/doorstop6.wav" );
		break;
	case 7:
		pev->noise2 = UTIL_PrecacheSound( "doors/doorstop7.wav" );
		break;
	case 8:
		pev->noise2 = UTIL_PrecacheSound( "doors/doorstop8.wav" );
		break;
	case 0:
		pev->noise2 = MAKE_STRING( "common/null.wav" );
		break;
	default:
		pev->noise2 = UTIL_PrecacheSound( m_sound );
		break;
	}
}

void CBaseTrainDoor :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( IsLockedByMaster( ))
		return;

	if( m_pTrain )
	{
		if( m_pTrain->GetLocalVelocity() != g_vecZero || m_pTrain->GetLocalAvelocity() != g_vecZero )
		{	
			if( !FBitSet( pev->spawnflags, SF_TRAINDOOR_OPEN_IN_MOVING ))
			{
				// very dangerous open the doors while train is moving :-)
				return;
			}
		}
	}

	if( !FBitSet( pev->spawnflags, SF_TRAINDOOR_OPEN_IN_MOVING ))
	{
		if( useType == USE_SET )
		{
			if( value == 2.0f )
			{
				// start train after door is closing 
				pev->impulse = 1;
			}
			else if( value == 3.0f )
			{
				// start train after door is closing 
				pev->impulse = 2;
			}
		}
	}

	if( FBitSet( pev->spawnflags, SF_TRAINDOOR_ONOFF_MODE ))
          {
		if( useType == USE_ON )
		{
			if( door_state != TD_CLOSED )
				return;
		}
		else if( useType == USE_OFF )
		{
			if( door_state != TD_OPENED )
				return;
		}
          }

	if( door_state == TD_OPENED )
	{
		// door should close
		DoorSlideDown();
	}
	else if( door_state == TD_CLOSED )
	{
		// door should open
		DoorGoUp();
	}
}

STATE CBaseTrainDoor :: GetState ( void )
{
	switch ( door_state )
	{
	case TD_OPENED:
		return STATE_ON;
	case TD_CLOSED:
		return STATE_OFF;
	case TD_SHIFT_UP:
	case TD_SLIDING_UP:
		 return STATE_TURN_ON;
	case TD_SLIDING_DOWN:
	case TD_SHIFT_DOWN:
		return STATE_TURN_OFF;
	default: return STATE_OFF; // This should never happen.
	}
}

void CBaseTrainDoor :: ActivateTrain( void )
{
	switch( pev->impulse )
	{
	case 1:	// activate train
		if( m_pTrain )
		{
			m_pTrain->pev->speed = m_pTrain->GetMaxSpeed();
			m_pTrain->Next();
		}
		break;
	case 2:	// activate trackchange
		if( m_hActivator )
			m_hActivator->Use( this, this, USE_TOGGLE, 0 );
		break;
	}

	SetThink( &CBaseTrainDoor :: FindTrain );
	pev->impulse = 0;
}

//
// The door has been shifted. Waiting for sliding
//
void CBaseTrainDoor::DoorSlideWait( void )
{
	if( door_state == TD_SLIDING_DOWN )
	{
		// okay. we are in poisition. Wait for 0.5 secs
		SetThink( &CBaseTrainDoor:: DoorGoDown );
	}
	else if( door_state == TD_SHIFT_UP )
	{
		// okay. we are in poisition. Wait for 0.5 secs
		SetThink( &CBaseTrainDoor:: DoorSlideUp );
	}

	SetNextThink( 0.5 );
}

//
// Starts the door going to its "up" position (simply ToggleData->vecPosition2).
//
void CBaseTrainDoor::DoorGoUp( void )
{
	// It could be going-down, if blocked.
	ASSERT( door_state == TD_CLOSED );

	door_state = TD_SHIFT_UP;

	UTIL_MakeVectors( m_vecOldAngles );

	STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise1 ));
	EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise2 ), 1, ATTN_NORM );

	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	Vector vecSize = pev->size - Vector( 2, 2, 2 );
	float depth = (fabs( gpGlobals->v_right.x * vecSize.x ) + fabs( gpGlobals->v_right.y * vecSize.y ) + fabs( gpGlobals->v_right.z * vecSize.z ) - m_flLip);

	if( pev->spawnflags & SF_TRAINDOOR_INVERSE )
		depth = -depth;

	VectorMatrix( pev->movedir, gpGlobals->v_right, gpGlobals->v_up );
	m_vecPosition3 = m_vecPosition1 + gpGlobals->v_right * -depth;

	SetMoveDone( &CBaseTrainDoor:: DoorSlideWait );
	LinearMove( m_vecPosition3, pev->speed );
}

//
// The door has been shifted. Move slide now
//
void CBaseTrainDoor::DoorSlideUp( void )
{
	// emit door moving and stop sounds on CHAN_STATIC so that the multicast doesn't
	// filter them out and leave a client stuck with looping door sounds!
	EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise1 ), 1, ATTN_NORM );

	UTIL_MakeVectors( m_vecOldAngles );

	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	Vector vecSize = pev->size - Vector( 2, 2, 2 );
	float width = (fabs( gpGlobals->v_forward.x * vecSize.x ) + fabs( gpGlobals->v_forward.y * vecSize.y ) + fabs( gpGlobals->v_forward.z * vecSize.z ));

	door_state = TD_SLIDING_UP;

	m_vecPosition2 = m_vecPosition3 + pev->movedir * width;

	SetMoveDone( &CBaseTrainDoor:: DoorHitTop );
	LinearMove( m_vecPosition2, pev->speed );
}

//
// The door has reached the "up" position.  Either go back down, or wait for another activation.
//
void CBaseTrainDoor :: DoorHitTop( void )
{
//	STOP_SOUND( edict(), CHAN_STATIC, pev->noise1 );

	ASSERT( door_state == TD_SLIDING_UP );
	door_state = TD_OPENED;

	if( pev->impulse )
	{
		switch( pev->impulse )
		{
		case 1:	// deactivate train
			if( m_pTrain )
				m_pTrain->pev->speed = 0;
			break;
		case 2:	// cancel trackchange
			m_hActivator = NULL;
			break;
		}
		
		pev->impulse = 0;
	}	
	
	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
}

void CBaseTrainDoor :: DoorSlideDown( void )
{
	EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise1 ), 1, ATTN_NORM );
	
	ASSERT( door_state == TD_OPENED );
	door_state = TD_SLIDING_DOWN;

	SetMoveDone( &CBaseTrainDoor:: DoorSlideWait );
	LinearMove( m_vecPosition3, pev->speed );
}

//
// Starts the door going to its "down" position (simply ToggleData->vecPosition1).
//
void CBaseTrainDoor :: DoorGoDown( void )
{
	ASSERT( door_state == TD_SLIDING_DOWN );
	door_state = TD_SHIFT_DOWN;

	SetMoveDone( &CBaseTrainDoor:: DoorHitBottom );
	LinearMove( m_vecPosition1, pev->speed );
}

//
// The door has reached the "down" position.  Back to quiescence.
//
void CBaseTrainDoor :: DoorHitBottom( void )
{
	STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise1 ));
	EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise2 ), 1, ATTN_NORM );

	ASSERT( door_state == TD_SHIFT_DOWN );
	door_state = TD_CLOSED;

	if( pev->impulse )
	{
		SetThink( &CBaseTrainDoor::ActivateTrain );
		SetNextThink( 1.0 );
	}

	SUB_UseTargets( m_hActivator, USE_ON, 0 );
}

void CBaseTrainDoor::Blocked( CBaseEntity *pOther )
{
	if( door_state == TD_SHIFT_UP )
	{
		DoorGoDown( );
	}
	else if( door_state == TD_SHIFT_DOWN )
	{
		DoorGoUp( );
	}
	else if( door_state == TD_SLIDING_UP )
	{
		DoorSlideDown( );
	}
	else if( door_state == TD_SLIDING_DOWN )
	{
		DoorSlideUp( );
	}
}
