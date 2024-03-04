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

#include "func_trackchange.h"

LINK_ENTITY_TO_CLASS( func_trackchange, CFuncTrackChange );

BEGIN_DATADESC( CFuncTrackChange )
	DEFINE_GLOBAL_FIELD( m_trackTop, FIELD_CLASSPTR ),
	DEFINE_GLOBAL_FIELD( m_trackBottom, FIELD_CLASSPTR ),
	DEFINE_GLOBAL_FIELD( m_train, FIELD_CLASSPTR ),
	DEFINE_GLOBAL_KEYFIELD( m_trackTopName, FIELD_STRING, "toptrack" ),
	DEFINE_GLOBAL_KEYFIELD( m_trackBottomName, FIELD_STRING, "bottomtrack" ),
	DEFINE_GLOBAL_KEYFIELD( m_trainName, FIELD_STRING, "train" ),
	DEFINE_FIELD( m_code, FIELD_INTEGER ),
	DEFINE_FIELD( m_targetState, FIELD_INTEGER ),
	DEFINE_FIELD( m_use, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flRadius, FIELD_FLOAT ),
	DEFINE_FUNCTION( GoUp ),
	DEFINE_FUNCTION( GoDown ),
	DEFINE_FUNCTION( Find ),
END_DATADESC()

void CFuncTrackChange :: Spawn( void )
{
	Setup();

	if( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ))
		m_vecPosition2 = m_vecPosition1;

	SetupRotation();

	if( FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
	{
		UTIL_SetOrigin( this, m_vecPosition2 );
		m_toggle_state = TS_AT_BOTTOM;
		SetLocalAngles( m_start );
		m_targetState = TS_AT_TOP;
	}
	else
	{
		UTIL_SetOrigin( this, m_vecPosition1 );
		m_toggle_state = TS_AT_TOP;
		SetLocalAngles( m_end );
		m_targetState = TS_AT_BOTTOM;
	}

	EnableUse();
	SetNextThink( 2.0f ); // let's train spawn
	SetThink( &CFuncTrackChange::Find );
	Precache();
}

void CFuncTrackChange :: Precache( void )
{
	// Can't trigger sound
	PRECACHE_SOUND( "buttons/button11.wav" );
	
	BaseClass::Precache();
}

void CFuncTrackChange :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "train" ))
	{
		m_trainName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "toptrack" ))
	{
		m_trackTopName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "bottomtrack" ))
	{
		m_trackBottomName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "radius" ))
	{
		m_flRadius = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue( pkvd ); // Pass up to base class
	}
}

void CFuncTrackChange::OverrideReset( void )
{
	SetMoveDoneTime( 1.0 );
	SetMoveDone( &CFuncTrackChange::Find );
}

void CFuncTrackChange :: Find( void )
{
	// Find track entities
	CBaseEntity *pTarget;

	pTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_trackTopName ));

	if( pTarget && FClassnameIs( pTarget, "path_track" ))
	{
		m_trackTop = (CPathTrack *)pTarget;
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_trackBottomName ));

		if( pTarget && FClassnameIs( pTarget, "path_track" ))
		{
			m_trackBottom = (CPathTrack *)pTarget;
			pTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_trainName ));

			if( pTarget && FClassnameIs( pTarget, "func_tracktrain" ))
			{
				m_train = (CFuncTrackTrain *)pTarget;
				Vector center = (pev->absmin + pev->absmax) * 0.5f;
				m_trackBottom = m_trackBottom->Nearest( center );
				m_trackTop = m_trackTop->Nearest( center );
				UpdateAutoTargets( m_toggle_state );
				SetThink( NULL );
				return;
			}
			else
			{
				ALERT( at_error, "Can't find train for track change! %s\n", STRING( m_trainName ));
			}
		}
		else
		{
			ALERT( at_error, "Can't find bottom track for track change! %s\n", STRING(m_trackBottomName) );
		}
	}
	else
	{
		ALERT( at_error, "Can't find top track for track change! %s\n", STRING(m_trackTopName) );
	}
}

TRAIN_CODE CFuncTrackChange :: EvaluateTrain( CPathTrack *pcurrent )
{
	// Go ahead and work, we don't have anything to switch, so just be an elevator
	if( !pcurrent || !m_train )
		return TRAIN_SAFE;

	if( m_train->m_ppath == pcurrent || ( pcurrent->m_pprevious && m_train->m_ppath == pcurrent->m_pprevious ) ||
		 ( pcurrent->m_pnext && m_train->m_ppath == pcurrent->m_pnext ))
	{
		if( m_train->GetSpeed() != 0 )
			return TRAIN_BLOCKING;

		Vector dist = GetLocalOrigin() - m_train->GetLocalOrigin();
		float length = dist.Length2D();

		if( length < m_train->m_length ) // empirically determined close distance
			return TRAIN_FOLLOWING;
		else if( length > ( 150 + m_train->m_length ))
			return TRAIN_SAFE;

		return TRAIN_BLOCKING;
	}
	return TRAIN_SAFE;
}

void CFuncTrackChange :: UpdateTrain( Vector &dest )
{
	float time = GetMoveDoneTime();

	m_train->SetAbsVelocity( GetAbsVelocity() );
	m_train->SetLocalAvelocity( GetLocalAvelocity() );
	m_train->SetMoveDoneTime( time );

	// attempt at getting the train to rotate properly around the origin of the trackchange
	if( time <= 0 )
		return;

	Vector offset = m_train->GetLocalOrigin() - GetLocalOrigin();
	Vector local, delta = dest - GetLocalAngles();

	// Transform offset into local coordinates
	UTIL_MakeInvVectors( delta, gpGlobals );
	local.x = DotProduct( offset, gpGlobals->v_forward );
	local.y = DotProduct( offset, gpGlobals->v_right );
	local.z = DotProduct( offset, gpGlobals->v_up );

	local = local - offset;
	m_train->SetAbsVelocity( GetAbsVelocity() + ( local * (1.0f / time )) );
}

void CFuncTrackChange :: GoDown( void )
{
	if( m_code == TRAIN_BLOCKING )
		return;

	// HitBottom may get called during CFuncPlat::GoDown(), so set up for that
	// before you call GoDown()

	UpdateAutoTargets( TS_GOING_DOWN );

	// If ROTMOVE, move & rotate
	if( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ) )
	{
		SetMoveDone( &CFuncPlat::CallHitBottom );
		m_toggle_state = TS_GOING_DOWN;
		AngularMove( m_start, pev->speed );
	}
	else
	{
		CFuncPlat :: GoDown();
		SetMoveDone( &CFuncPlat::CallHitBottom );
		RotMove( m_start, GetMoveDoneTime() );
	}

	// Otherwise, rotate first, move second

	// If the train is moving with the platform, update it
	if( m_code == TRAIN_FOLLOWING )
	{
		UpdateTrain( m_start );
		m_train->m_ppath = NULL;
	}
}

//
// Platform is at bottom, now starts moving up
//
void CFuncTrackChange :: GoUp( void )
{
	if( m_code == TRAIN_BLOCKING )
		return;

	// HitTop may get called during CFuncPlat::GoUp(), so set up for that
	// before you call GoUp();

	UpdateAutoTargets( TS_GOING_UP );

	if( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ))
	{
		m_toggle_state = TS_GOING_UP;
		SetMoveDone( &CFuncPlat::CallHitTop );
		AngularMove( m_end, pev->speed );
	}
	else
	{
		// If ROTMOVE, move & rotate
		CFuncPlat :: GoUp();
		SetMoveDone( &CFuncPlat::CallHitTop );
		RotMove( m_end, GetMoveDoneTime( ));
	}
	
	// Otherwise, move first, rotate second

	// If the train is moving with the platform, update it
	if( m_code == TRAIN_FOLLOWING )
	{
		UpdateTrain( m_end );
		m_train->m_ppath = NULL;
	}
}

// Normal track change
void CFuncTrackChange :: UpdateAutoTargets( int toggleState )
{
	// g-cont. update targets same as func_door
	if( toggleState == TS_AT_BOTTOM )
	{
		if( !FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
			UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
		else
			UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}
	else if( toggleState == TS_AT_TOP )
	{
		if( FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
			UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
		else
			UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}

	if( !m_trackTop || !m_trackBottom )
		return;

	if( toggleState == TS_AT_TOP )
		ClearBits( m_trackTop->pev->spawnflags, SF_PATH_DISABLED );
	else
		SetBits( m_trackTop->pev->spawnflags, SF_PATH_DISABLED );

	if( toggleState == TS_AT_BOTTOM )
		ClearBits( m_trackBottom->pev->spawnflags, SF_PATH_DISABLED );
	else
		SetBits( m_trackBottom->pev->spawnflags, SF_PATH_DISABLED );
}

void CFuncTrackChange :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( FBitSet( pev->spawnflags, SF_TRACK_ONOFF_MODE ))
	{
		if( useType == USE_ON )
		{
			if( m_toggle_state != TS_AT_BOTTOM )
				return;	// ignore
		}
		else if( useType == USE_OFF )
		{
			if( m_toggle_state != TS_AT_TOP )
				return;
		}
		else if( useType == USE_SET || useType == USE_RESET )
			return; // completely ignore
	}


	// g-cont. if trainname not specified trackchange always search tracktrain in the model radius 
	if( FStringNull( m_trainName ))
	{
		// train not specified - search train in radius of trackchange or custom specfied radius
		float radius = m_flRadius;
		if( !radius ) radius = ( Q_max( pev->size.x, Q_max( pev->size.y, pev->size.z ))) / 2.0f;
		CBaseEntity *pFind = NULL;

		while(( pFind = UTIL_FindEntityInSphere( pFind, GetAbsOrigin(), radius )) != NULL )
		{
			if( FClassnameIs( pFind->pev, "func_tracktrain" ))
			{
				m_train = (CFuncTrackTrain *)pFind;
				ALERT( at_aiconsole, "Found train %s\n", STRING( pFind->pev->targetname ));
				break;
			}
		}

//		if( m_train == NULL ) 
//			ALERT( at_error, "%s: couldn't find train to operate\n", GetTargetname( ));
	}

	if( m_train && m_train->m_pDoor && m_train->m_pDoor->IsDoorControl( ))
	{
		m_train->m_pDoor->Use( this, this, USE_SET, 3.0f );
		return;
	}

	if( m_toggle_state != TS_AT_TOP && m_toggle_state != TS_AT_BOTTOM )
		return;

	// If train is in "safe" area, but not on the elevator, play alarm sound
	if( m_toggle_state == TS_AT_TOP )
		m_code = EvaluateTrain( m_trackTop );
	else if( m_toggle_state == TS_AT_BOTTOM )
		m_code = EvaluateTrain( m_trackBottom );
	else
		m_code = TRAIN_BLOCKING;

	if( m_code == TRAIN_BLOCKING )
	{
		// Play alarm and return
		EMIT_SOUND( edict(), CHAN_VOICE, "buttons/button11.wav", 1, ATTN_NORM );
		return;
	}

	// Otherwise, it's safe to move
	// If at top, go down
	// at bottom, go up

	DisableUse();
	if( m_toggle_state == TS_AT_TOP )
		GoDown();
	else
		GoUp();
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncTrackChange :: HitBottom( void )
{
	BaseClass :: HitBottom();

	if( m_code == TRAIN_FOLLOWING )
	{
		m_train->SetTrack( m_trackBottom );
	}

	SetMoveDone( NULL );
	SetMoveDoneTime( -1 );

	UpdateAutoTargets( m_toggle_state );

	EnableUse();
}

//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncTrackChange :: HitTop( void )
{
	BaseClass :: HitTop();

	if( m_code == TRAIN_FOLLOWING )
	{
		m_train->SetTrack( m_trackTop );
	}
	
	// Don't let the plat go back down
	SetMoveDone( NULL );
	SetMoveDoneTime( -1 );
	UpdateAutoTargets( m_toggle_state );
	EnableUse();
}
