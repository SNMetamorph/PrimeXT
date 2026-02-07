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

#include "func_tracktrain.h"

// ---------------------------------------------------------------------
//
// Track Train
//
// ---------------------------------------------------------------------

BEGIN_DATADESC( CFuncTrackTrain )
	DEFINE_FIELD( m_ppath, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pDoor, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pSpeedControl, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_length, FIELD_FLOAT, "wheels" ),
	DEFINE_KEYFIELD( m_height, FIELD_FLOAT, "height" ),
	DEFINE_KEYFIELD( m_startSpeed, FIELD_FLOAT, "startspeed" ),
	DEFINE_FIELD( m_controlMins, FIELD_VECTOR ),
	DEFINE_FIELD( m_controlMaxs, FIELD_VECTOR ),
	DEFINE_FIELD( m_controlOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_KEYFIELD( m_sounds, FIELD_STRING, "sounds" ),
	DEFINE_KEYFIELD( m_soundStart, FIELD_STRING, "soundstart" ),
	DEFINE_KEYFIELD( m_soundStop, FIELD_STRING, "soundstop" ),
	DEFINE_KEYFIELD( m_eVelocityType, FIELD_INTEGER, "acceltype" ),
	DEFINE_KEYFIELD( m_eOrientationType, FIELD_INTEGER, "orientation" ),
	DEFINE_FIELD( m_flVolume, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_flBank, FIELD_FLOAT, "bank" ),
	DEFINE_FIELD( m_flDesiredSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flAccelSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flReachedDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_oldSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_dir, FIELD_FLOAT ),
	DEFINE_FUNCTION( Next ),
	DEFINE_FUNCTION( Find ),
	DEFINE_FUNCTION( NearestPath ),
	DEFINE_FUNCTION( DeadEnd ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_tracktrain, CFuncTrackTrain );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CFuncTrackTrain::CFuncTrackTrain()
{
#ifdef _DEBUG
	m_controlMins.Init();
	m_controlMaxs.Init();
#endif
	// These defaults match old func_tracktrains. Changing these defaults would
	// require a vmf_tweak of older content to keep it from breaking.
	m_eOrientationType = TrainOrientation_AtPathTracks;
	m_eVelocityType = TrainVelocity_EaseInEaseOut;
}

void CFuncTrackTrain :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "wheels" ))
	{
		m_length = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "height" ))
	{
		m_height = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "startspeed" ))
	{
		m_startSpeed = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "sounds" ))
	{
		m_sounds = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "soundstop" ))
	{
		m_soundStop = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "soundstart" ))
	{
		m_soundStart = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "volume" ))
	{
		m_flVolume = (float)( Q_atoi( pkvd->szValue ));
		m_flVolume *= 0.1f;
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "acceltype" ))
	{
		m_eVelocityType = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "orientation" ))
	{
		m_eOrientationType = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "bank" ))
	{
		m_flBank = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CFuncTrackTrain :: SetDirForward( bool bForward )
{
	if( bForward && ( m_dir != 1 ))
	{
		// Reverse direction.
		if ( m_ppath && m_ppath->GetPrevious() )
		{
			m_ppath = m_ppath->GetPrevious();
		}

		m_dir = 1;
	}
	else if( !bForward && ( m_dir != -1 ))
	{
		// Reverse direction.
		if ( m_ppath && m_ppath->GetNext() )
		{
			m_ppath = m_ppath->GetNext();
		}

		m_dir = -1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the speed of the train to the given value in units per second.
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: SetSpeed( float flSpeed, float flAccel )
{
	float flOldSpeed = pev->speed;
	m_flAccelSpeed = 0.0f;

	if( flAccel != 0.0f )
	{
		m_flDesiredSpeed = fabs( flSpeed ) * m_dir;

		if( pev->speed == 0 ) // little push to get us going
			pev->speed = IsDirForward() ? 0.1f : -0.1f;
		m_flAccelSpeed = flAccel;

		Next();
		return;		
	}

	pev->speed = m_flDesiredSpeed = fabs( flSpeed ) * m_dir;
	m_oldSpeed = flOldSpeed;

	if( pev->speed != flOldSpeed )
	{
		// Changing speed.
		if( pev->speed != 0 )
		{
			// Starting to move.
			Next();
		}
		else
		{
			// Stopping.
			Stop();
		}
	}

	ALERT( at_aiconsole, "TRAIN(%s), speed to %.2f\n", GetDebugName(), pev->speed );
}

void CFuncTrackTrain :: SetSpeedExternal( float flSpeed )
{
	float flOldSpeed = pev->speed;

	if( flSpeed > 0.0f )
		SetDirForward( true );
	else if( flSpeed < 0.0 )
		SetDirForward( false );

	pev->speed = m_flDesiredSpeed = fabs( flSpeed ) * m_dir;
	m_oldSpeed = flOldSpeed;

	if( pev->speed != flOldSpeed )
	{
		// Changing speed.
		if( pev->speed != 0 )
		{
			// Starting to move.
			Next();
		}
		else
		{
			// Stopping.
			Stop();
		}
	}

	ALERT( at_aiconsole, "TRAIN(%s), speed to %.2f\n", GetDebugName(), pev->speed );
}

//-----------------------------------------------------------------------------
// Purpose: Stops the train.
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: Stop( void )
{
	SetLocalVelocity( g_vecZero );
	SetLocalAvelocity( g_vecZero );
	m_oldSpeed = pev->speed;
	SetThink( NULL );
	pev->speed = 0;
	StopSound();
}

void CFuncTrackTrain :: Blocked( CBaseEntity *pOther )
{
	// Blocker is on-ground on the train
	if ( FBitSet( pOther->pev->flags, FL_ONGROUND ) && pOther->GetGroundEntity() == this )
	{
		float deltaSpeed = fabs( pev->speed );

		if( deltaSpeed > 50 )
			deltaSpeed = 50;

		Vector vecNewVelocity = pOther->GetAbsVelocity();

		if( !vecNewVelocity.z )
		{
			vecNewVelocity.z += deltaSpeed;
			pOther->SetAbsVelocity( vecNewVelocity );
		}
		return;
	}
	else
	{
		Vector vecNewVelocity = (pOther->GetAbsOrigin() - GetAbsOrigin()).Normalize() * pev->dmg;
		pOther->SetAbsVelocity( vecNewVelocity );
	}

	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_UNBLOCKABLE ))
	{
		// unblockable shouldn't damage the player in this case
		if ( pOther->IsPlayer() )
			return;
	}

	ALERT( at_aiconsole, "TRAIN(%s): Blocked by %s (dmg:%.2f)\n", GetDebugName(), pOther->GetClassname(), pev->dmg );
	if( pev->dmg <= 0 ) return;

	// we can't hurt this thing, so we're not concerned with it
	pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );
}


void CFuncTrackTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( IsLockedByMaster( ))
		return;

	if( useType == USE_RESET )
	{
		if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_FORWARDONLY ))
			return;	// can't moving backward

		if( !FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
			return; // only for non-controllable trains

		// will change only for non-moving train
		if( pev->speed == 0 )
			SetDirForward( !IsDirForward() );
	}
	else if( useType != USE_SET )
	{
		if( !ShouldToggle( useType, ( pev->speed != 0 )))
			return;

		if( pev->speed == 0 )
		{
			if( m_pDoor && m_pDoor->IsDoorControl( ))
			{
				m_pDoor->Use( pActivator, pCaller, USE_SET, 2.0f );
				return; // wait for door closing first
			}

			SetSpeed( m_maxSpeed );
		}
		else
		{
			SetSpeed( 0 );
		}
	}
	else
	{
		// g-cont. Don't controls, if tracktrain on trackchange
	          if( m_ppath == NULL ) 
			return;

		float delta = value;
		float accel = 0.0f;

		if( pCaller && pCaller->IsPlayer( ))
		{
			delta = ((int)(m_flDesiredSpeed * 4) / (int)m_maxSpeed) * 0.25 + 0.25 * delta;
			accel = 100.0f;
		}
		else delta *= m_dir; // momentary button can't sending negative values

		if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_FORWARDONLY ))
			delta = bound( 0.0f, delta, 1.0f );
		else delta = bound( -1.0f, delta, 1.0f ); // g-cont. limit the backspeed

		// don't reverse if decelerate
		if( pev->speed != 0 && IsDirForward() && delta < 0 )
			return;
		if( pev->speed != 0 && !IsDirForward() && delta > 0 )
			return;

		if( delta > 0.0f )
			SetDirForward( true );
		else if( delta < 0.0 )
			SetDirForward( false );
		delta = fabs( delta );
		SetSpeed( m_maxSpeed * delta, accel );

		ALERT( at_aiconsole, "TRAIN( %s ), speed to %.2f\n", GetTargetname(), pev->speed );
	}
}

static float Fix( float angle )
{
	while ( angle < 0 )
		angle += 360;
	while ( angle > 360 )
		angle -= 360;

	return angle;
}


static void FixupAngles( Vector &v )
{
	v.x = Fix( v.x );
	v.y = Fix( v.y );
	v.z = Fix( v.z );
}

#define TRAIN_MINPITCH	60
#define TRAIN_MAXPITCH	200
#define TRAIN_MAXSPEED	1000	// approx max speed for sound pitch calculation

void CFuncTrackTrain :: StopSound( void )
{
	// if sound playing, stop it
	if( m_soundPlaying && pev->noise )
	{
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));
		EMIT_SOUND_DYN( edict(), CHAN_ITEM, STRING( pev->noise2 ), m_flVolume, ATTN_NORM, 0, 100 );
	}

	m_soundPlaying = 0;
}

// update pitch based on speed, start sound if not playing
// NOTE: when train goes through transition, m_soundPlaying should go to 0, 
// which will cause the looped sound to restart.
void CFuncTrackTrain :: UpdateSound( void )
{
	if( !pev->noise )
		return;

	float flSpeedRatio = 0;

	if ( FBitSet( pev->spawnflags, SF_TRACKTRAIN_SPEEDBASED_PITCH ))
	{
		flSpeedRatio = bound( 0.0f, fabs( pev->speed ) / m_maxSpeed, 1.0f );
	}
	else
	{
		flSpeedRatio = bound( 0.0f, fabs( pev->speed ) / TRAIN_MAXSPEED, 1.0f );
	}

	float flpitch = RemapVal( flSpeedRatio, 0.0f, 1.0f, TRAIN_MINPITCH, TRAIN_MAXPITCH );

	if( !m_soundPlaying )
	{
		// play startup sound for train
		EMIT_SOUND_DYN( edict(), CHAN_ITEM, STRING( pev->noise1 ), m_flVolume, ATTN_NORM, 0, 100 );
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noise ), m_flVolume, ATTN_NORM, 0, (int)flpitch );
		m_soundPlaying = 1;
	} 
	else
	{
		// update pitch
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noise), m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, (int)flpitch );
	}
}

void CFuncTrackTrain :: ArriveAtNode( CPathTrack *pNode )
{
	// Fire the pass target if there is one
	if ( pNode->pev->message && !FBitSet( pev->spawnflags, SF_TRACKTRAIN_NO_FIRE_ON_PASS ))
	{
		UTIL_FireTargets( pNode->pev->message, this, this, USE_TOGGLE, 0 );
		if ( FBitSet( pNode->pev->spawnflags, SF_PATH_FIREONCE ) )
			pNode->pev->message = NULL_STRING;
	}

	if ( pNode->m_iszFireFow && IsDirForward( ))
	{
		UTIL_FireTargets( pNode->m_iszFireFow, this, this, USE_TOGGLE, 0 );
	}

	if ( pNode->m_iszFireRev && !IsDirForward())
	{
		UTIL_FireTargets( pNode->m_iszFireRev, this, this, USE_TOGGLE, 0 );
	}

	//
	// Disable train controls if this path track says to do so.
	//
	if( FBitSet( pNode->pev->spawnflags, SF_PATH_DISABLE_TRAIN ))
		SetBits( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL );
			
	// Don't override the train speed if it's under user control.
	if ( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
	{
		if ( pNode->pev->speed != 0 )
		{
			// don't copy speed from target if it is 0 (uninitialized)
			SetSpeed( pNode->pev->speed );
			ALERT( at_aiconsole, "TrackTrain %s arrived at %s, speed to %4.2f\n", GetDebugName(), pNode->GetDebugName(), pNode->pev->speed );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pnext - 
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: UpdateTrainVelocity( CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval )
{
	Vector velDesired;

	switch( m_eVelocityType )
	{
	case TrainVelocity_Instantaneous:
		velDesired = nextPos - GetLocalOrigin();
		velDesired = velDesired.Normalize();
		velDesired *= fabs( pev->speed );
		SetLocalVelocity( velDesired );
		break;
	case TrainVelocity_LinearBlend:
	case TrainVelocity_EaseInEaseOut:
		if( m_flAccelSpeed != 0.0f )
		{
			float flPrevSpeed = pev->speed;
			float flNextSpeed = m_flDesiredSpeed;

			if( flPrevSpeed != flNextSpeed )
			{
				pev->speed = UTIL_Approach( m_flDesiredSpeed, pev->speed, m_flAccelSpeed * flInterval );
			}
		}
		else if( pPrev && pNext )
		{
			// Get the speed to blend from.
			float flPrevSpeed = pev->speed;
			if( pPrev->pev->speed != 0 && FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
			{
				flPrevSpeed = pPrev->pev->speed;
			}

			// Get the speed to blend to.
			float flNextSpeed = flPrevSpeed;
			if( pNext->pev->speed != 0 && FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
			{
				flNextSpeed = pNext->pev->speed;
			}

			flNextSpeed = fabs( flNextSpeed );
			flPrevSpeed = fabs( flPrevSpeed );

			// If they're different, do the blend.
			if( flPrevSpeed != flNextSpeed )
			{
				Vector vecSegment = pNext->GetLocalOrigin() - pPrev->GetLocalOrigin();
				float flSegmentLen = vecSegment.Length();

				if( flSegmentLen )
				{
					Vector vecCurOffset = GetLocalOrigin() - pPrev->GetLocalOrigin();
					float p = vecCurOffset.Length() / flSegmentLen;
					if( m_eVelocityType == TrainVelocity_EaseInEaseOut )
					{
						p = SimpleSplineRemapVal( p, 0.0f, 1.0f, 0.0f, 1.0f );
					}

					pev->speed = m_dir * ( flPrevSpeed * ( 1.0 - p ) + flNextSpeed * p );
				}
			}
			else
			{
				pev->speed = m_dir * flPrevSpeed;
			}
		}
//Msg( "pev->speed %g\n", pev->speed );
		velDesired = nextPos - GetLocalOrigin();
		velDesired = velDesired.Normalize();
		velDesired *= fabs( pev->speed );
		SetLocalVelocity( velDesired );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pnext - 
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: UpdateTrainOrientation( CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval )
{
	// Trains *can* work in local space, but only if all elements of the track share
	// the same move parent as the train.
	assert( !pPrev || (pPrev->m_hParent.Get() == m_hParent.Get() ) );

	switch( m_eOrientationType )
	{
	case TrainOrientation_Fixed:
		// Fixed orientation. Do nothing.
		break;
	case TrainOrientation_AtPathTracks:
		UpdateOrientationAtPathTracks( pPrev, pNext, nextPos, flInterval );
		break;
	case TrainOrientation_EaseInEaseOut:
	case TrainOrientation_LinearBlend:
		UpdateOrientationBlend( m_eOrientationType, pPrev, pNext, nextPos, flInterval );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adjusts our angles as we hit each path track. This is for support of
//			trains with wheels that round corners a la HL1 trains.
// FIXME: move into path_track, have the angles come back from LookAhead
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: UpdateOrientationAtPathTracks( CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval )
{
	if( !m_ppath )
		return;

	Vector nextFront = GetLocalOrigin();
	CPathTrack *pNextNode = NULL;

	nextFront.z -= m_height;
	if ( m_length > 0 )
	{
		m_ppath->LookAhead( nextFront, IsDirForward() ? m_length : -m_length, 0, &pNextNode );
	}
	else
	{
		m_ppath->LookAhead( nextFront, IsDirForward() ? 100.0f : -100.0f, 0, &pNextNode );
	}
	nextFront.z += m_height;

	Vector vecFaceDir = nextFront - GetLocalOrigin();
	if ( !IsDirForward() )
	{
		vecFaceDir *= -1;
	}

	Vector angles = UTIL_VecToAngles( vecFaceDir );
	angles.x = -angles.x;
	// The train actually points west
	angles.y += 180;

	// Wrapped with this bool so we don't affect old trains
	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
	{
		if( pNextNode && pNextNode->GetOrientationType() == TrackOrientation_FacePathAngles )
		{
			angles = pNextNode->GetOrientation( IsDirForward() );
		}
	}

	Vector curAngles = GetLocalAngles();

	if ( !pPrev || (vecFaceDir.x == 0 && vecFaceDir.y == 0) )
		angles = curAngles;

	DoUpdateOrientation( curAngles, angles, flInterval );
}


//-----------------------------------------------------------------------------
// Purpose: Blends our angles using one of two orientation blending types.
//			ASSUMES that eOrientationType is either LinearBlend or EaseInEaseOut.
//			FIXME: move into path_track, have the angles come back from LookAhead
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: UpdateOrientationBlend( int eOrientationType, CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval )
{
	// Get the angles to blend from.
	Vector angPrev = pPrev->GetOrientation( IsDirForward() );

	// Get the angles to blend to. 
	Vector angNext;
	if ( pNext )
	{
		angNext = pNext->GetOrientation( IsDirForward() );
	}
	else
	{
		// At a dead end, just use the last path track's angles.
		angNext = angPrev;
	}

	if ( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOPITCH ))
	{
		angNext[PITCH] = angPrev[PITCH];
	}

	// Calculate our parametric distance along the path segment from 0 to 1.
	float p = 0;
	if( pPrev && ( angPrev != angNext ) )
	{
		Vector vecSegment = pNext->GetLocalOrigin() - pPrev->GetLocalOrigin();
		float flSegmentLen = vecSegment.Length();
		if( flSegmentLen ) p = m_flReachedDist / flSegmentLen;
	}

	if ( eOrientationType == TrainOrientation_EaseInEaseOut )
	{
		p = SimpleSplineRemapVal( p, 0.0f, 1.0f, 0.0f, 1.0f );
	}

//	Msg( "UpdateOrientationFacePathAngles: %s->%s, p=%f\n", pPrev->GetDebugName(), pNext->GetDebugName(), p );

	Vector4D qtPrev;
	Vector4D qtNext;
	
	AngleQuaternion( angPrev, qtPrev );
	AngleQuaternion( angNext, qtNext );

	Vector angNew = angNext;
	float flAngleDiff = QuaternionAngleDiff( qtPrev, qtNext );
	if( flAngleDiff )
	{
		Vector4D qtNew;
		QuaternionSlerp( qtPrev, qtNext, p, qtNew );
		QuaternionAngle( qtNew, angNew );
	}

	if ( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOPITCH ))
	{
		angNew[PITCH] = angPrev[PITCH];
	}

	angNew.x = -angNew.x;
	DoUpdateOrientation( GetLocalAngles(), angNew, flInterval );
}


//-----------------------------------------------------------------------------
// Purpose: Sets our angular velocity to approach the target angles over the given interval.
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: DoUpdateOrientation( const Vector &curAngles, const Vector &angles, float flInterval )
{
	float vy, vx;
	if ( !FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOPITCH ))
	{
		vx = UTIL_AngleDistance( Fix( angles.x ), Fix( curAngles.x ));
	}
	else
	{
		vx = 0;
	}

	vy = UTIL_AngleDistance( Fix( angles.y ), Fix( curAngles.y ));
	
	// HACKHACK: Clamp really small angular deltas to avoid rotating movement on things
	// that are close enough
	if ( fabs(vx) < 0.1 )
	{
		vx = 0;
	}
	if ( fabs(vy) < 0.1 )
	{
		vy = 0;
	}

	if ( flInterval == 0 )
	{
		// Avoid dividing by zero
		flInterval = 0.1;
	}

	Vector vecAngVel( vx / flInterval, vy / flInterval, GetLocalAvelocity().z );

	if ( m_flBank != 0 )
	{
		if ( vecAngVel.y < -5 )
		{
			vecAngVel.z = UTIL_AngleDistance( UTIL_ApproachAngle( -m_flBank, curAngles.z, m_flBank * 2 ), curAngles.z );
		}
		else if ( vecAngVel.y > 5 )
		{
			vecAngVel.z = UTIL_AngleDistance( UTIL_ApproachAngle( m_flBank, curAngles.z, m_flBank * 2 ), curAngles.z );
		}
		else
		{
			vecAngVel.z = UTIL_AngleDistance( UTIL_ApproachAngle( 0, curAngles.z, m_flBank * 4 ), curAngles.z ) * 4;
		}
	}

	SetLocalAvelocity( vecAngVel );
}

void CFuncTrackTrain :: TeleportToPathTrack( CPathTrack *pTeleport )
{
	Vector angCur = GetLocalAngles();

	Vector nextPos = pTeleport->GetLocalOrigin();
	Vector look = nextPos;
	pTeleport->LookAhead( look, m_length, 0 );

	Vector nextAngles;
	if( look == nextPos )
	{
		nextAngles = GetLocalAngles();
	}
	else
	{
		nextAngles = pTeleport->GetOrientation(( pev->speed > 0 ));
		if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOPITCH ))
		{
			nextAngles[PITCH] = angCur[PITCH];
		}
	}

	Teleport( &pTeleport->GetLocalOrigin(), &nextAngles, NULL );
	SetLocalAvelocity( g_vecZero );
}

void CFuncTrackTrain :: Next( void )
{
	if ( !pev->speed )
	{
		ALERT( at_aiconsole, "TRAIN(%s): Speed is 0\n", STRING(pev->targetname) );
		Stop();
		return;
	}

	if ( !m_ppath )
	{	
		ALERT( at_aiconsole, "TRAIN(%s): Lost path\n", STRING(pev->targetname) );
		Stop();
		return;
	}

	UpdateSound();

	Vector nextPos = GetLocalOrigin();
	float flSpeed = pev->speed;

	nextPos.z -= m_height;
	CPathTrack *pNextNext = NULL;
	CPathTrack *pNext = m_ppath->LookAhead( nextPos, flSpeed * 0.1, 1, &pNextNext );
	nextPos.z += m_height;

	// If we're moving towards a dead end, but our desired speed goes in the opposite direction
	// this fixes us from stalling
	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ) && (( flSpeed < 0 ) != ( m_flDesiredSpeed < 0 )))
	{
		if( !pNext ) pNext = m_ppath;
	}

	// Trains *can* work in local space, but only if all elements of the track share
	// the same move parent as the train.
	ASSERT( !pNext || ( pNext->m_hParent.Get() == m_hParent.Get() ));

	if( pNext )
	{
		// can't compute correct distance in 3D because angle interpolation change the origin
		if( pNext == m_ppath )
			m_flReachedDist += fabs( pev->speed ) * gpGlobals->frametime;
		else m_flReachedDist = 0.0f;

		UpdateTrainVelocity( pNext, pNextNext, nextPos, gpGlobals->frametime );
		UpdateTrainOrientation( pNext, pNextNext, nextPos, 0.1 );

		if( pNext != m_ppath )
		{
			CPathTrack *pFire;

			if( IsDirForward( ))
				pFire = pNext;
			else pFire = m_ppath;

			//
			// We have reached a new path track. Fire its OnPass output.
			//
			m_ppath = pNext;
			ArriveAtNode( pFire );

			//
			// See if we should teleport to the next path track.
			//
			CPathTrack *pTeleport = pNext->GetNext();
			if(( pTeleport != NULL ) && FBitSet( pTeleport->pev->spawnflags, SF_PATH_TELEPORT ))
			{
				TeleportToPathTrack( pTeleport );
			}
		}

		SetMoveDoneTime( 0.5f );
		SetNextThink( 0.0f );
		SetMoveDone( NULL );
		SetThink( &CFuncTrackTrain::Next );
	}
	else
	{
		//
		// We've reached the end of the path, stop.
		//
		StopSound();
		SetLocalVelocity( nextPos - GetLocalOrigin( ));
		float distance = GetLocalVelocity().Length();
		SetLocalAvelocity( g_vecZero );
		m_oldSpeed = pev->speed;
		m_flDesiredSpeed = 0;
		pev->speed = 0;

		// Move to the dead end
		
		// Are we there yet?
		if ( distance > 0 )
		{
			// no, how long to get there?
			float flTime = distance / fabs( m_oldSpeed );
			SetLocalVelocity( GetLocalVelocity() * (m_oldSpeed / distance) );
			SetMoveDoneTime( flTime );
			SetMoveDone( &CFuncTrackTrain::DeadEnd );
			DontThink();
		}
		else
		{
			DeadEnd();
		}
	}
}

void CFuncTrackTrain :: DeadEnd( void )
{
	// Fire the dead-end target if there is one
	CPathTrack *pTrack, *pNext;

	pTrack = m_ppath;

	ALERT( at_aiconsole, "TRAIN(%s): Dead end ", GetDebugName() );
	// Find the dead end path node
	// HACKHACK -- This is bugly, but the train can actually stop moving at a different node depending on it's speed
	// so we have to traverse the list to it's end.
	if ( pTrack )
	{
		if ( m_oldSpeed < 0 )
		{
			do
			{
				pNext = pTrack->ValidPath( pTrack->GetPrevious(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
		else
		{
			do
			{
				pNext = pTrack->ValidPath( pTrack->GetNext(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
	}

	m_oldSpeed = pev->speed;
	m_flDesiredSpeed = 0;
	pev->speed = 0;
	SetAbsVelocity( g_vecZero );
	SetLocalAvelocity( g_vecZero );

	if ( pTrack )
	{
		ALERT( at_aiconsole, "at %s\n", pTrack->GetDebugName() );
		if ( pTrack->pev->netname )
			UTIL_FireTargets( pTrack->GetNetname(), this, this, USE_TOGGLE, 0 );
	}
	else
		ALERT( at_aiconsole, "\n" );
}

void CFuncTrackTrain :: SetControls( CBaseEntity *pControls )
{
	Vector offset = pControls->GetLocalOrigin() - m_controlOrigin;

	m_controlMins = pControls->pev->mins + offset;
	m_controlMaxs = pControls->pev->maxs + offset;
}

BOOL CFuncTrackTrain :: OnControls( CBaseEntity *pTest )
{
	Vector offset = pTest->GetAbsOrigin() - GetLocalOrigin();

	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
		return FALSE;

	// Transform offset into local coordinates
	matrix3x3 tmp = matrix3x3( GetLocalAngles() );
	// rotate into local space
	Vector local = tmp.VectorIRotate( offset );

	if ( local.x >= m_controlMins.x && local.y >= m_controlMins.y && local.z >= m_controlMins.z &&
		 local.x <= m_controlMaxs.x && local.y <= m_controlMaxs.y && local.z <= m_controlMaxs.z )
		 return TRUE;

	return FALSE;
}

void CFuncTrackTrain :: Find( void )
{
	m_ppath = (CPathTrack *)UTIL_FindEntityByTargetname( NULL, GetTarget() );
	if ( !m_ppath )
		return;

	if ( !FClassnameIs( m_ppath, "path_track" ) )
	{
		ALERT( at_error, "func_track_train must be on a path of path_track\n" );
		m_ppath = NULL;
		return;
	}

	Vector nextPos = m_ppath->GetLocalOrigin();
	Vector look = nextPos;
	m_ppath->LookAhead( look, m_length, 0 );
	nextPos.z += m_height;
	look.z += m_height;

	Vector nextAngles = UTIL_VecToAngles( look - nextPos );
	nextAngles.x = -nextAngles.x;
	// The train actually points west
	nextAngles.y += 180;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOPITCH )
		nextAngles.x = 0;

	Teleport( &nextPos, &nextAngles, NULL );
	SetLocalAvelocity( g_vecZero );

	ArriveAtNode( m_ppath );
	pev->speed = m_startSpeed;

	SetNextThink( 0.1f );
	SetThink( &CFuncTrackTrain::Next );
}

void CFuncTrackTrain :: NearestPath( void )
{
	CBaseEntity *pTrack = NULL;
	CBaseEntity *pNearest = NULL;
	float dist, closest;

	closest = 1024;

	while ((pTrack = UTIL_FindEntityInSphere( pTrack, GetAbsOrigin(), 1024 )) != NULL)
	{
		// filter out non-tracks
		if ( !(pTrack->pev->flags & (FL_CLIENT|FL_MONSTER)) && FClassnameIs( pTrack, "path_track" ) )
		{
			dist = (GetLocalOrigin() - pTrack->GetLocalOrigin()).Length();
			if ( dist < closest )
			{
				closest = dist;
				pNearest = pTrack;
			}
		}
	}

	if ( !pNearest )
	{
		ALERT( at_console, "Can't find a nearby track !!!\n" );
		SetThink( NULL);
		return;
	}

	ALERT( at_aiconsole, "TRAIN: %s, Nearest track is %s\n", GetDebugName(), pNearest->GetDebugName() );
	// If I'm closer to the next path_track on this path, then it's my real path
	pTrack = ((CPathTrack *)pNearest)->GetNext();
	if ( pTrack )
	{
		if ( (GetLocalOrigin() - pTrack->GetLocalOrigin()).Length() < (GetLocalOrigin() - pNearest->GetLocalOrigin()).Length() )
			pNearest = pTrack;
	}

	m_ppath = (CPathTrack *)pNearest;

	if ( pev->speed != 0 )
	{
		SetMoveDoneTime( 0.1 );
		SetMoveDone( &CFuncTrackTrain::Next );
	}
}


void CFuncTrackTrain :: OverrideReset( void )
{
	// NOTE: all entities are spawned so we don't need
	// to make delay before searching nearest path
	if( !m_ppath )
	{
		NearestPath();
		SetThink( NULL );
	}
}

CFuncTrackTrain *CFuncTrackTrain :: Instance( edict_t *pent )
{ 
	CBaseEntity *pEntity = CBaseEntity::Instance( pent );
	if( pEntity && FClassnameIs( pEntity, "func_tracktrain" ))
		return (CFuncTrackTrain *)pEntity;
	return NULL;
}

/*QUAKED func_train (0 .5 .8) ?
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
sounds
1) ratchet metal
*/

void CFuncTrackTrain :: Spawn( void )
{
	if( m_maxSpeed == 0 )
	{
		if( pev->speed == 0 )
			m_maxSpeed = 100;
		else m_maxSpeed = pev->speed;
	}

	SetAbsVelocity( g_vecZero );
	SetLocalAvelocity( g_vecZero );
	pev->speed = 0;
	m_dir = 1;

	if ( FStringNull(pev->target) )
		ALERT( at_console, "FuncTrackTrain '%s' has no target.\n", GetDebugName());

	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_UNBLOCKABLE ))
		SetBits( pev->flags, FL_UNBLOCKABLE );

	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_PASSABLE ))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	
	SET_MODEL( edict(), GetModel() );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	RelinkEntity( TRUE );

	// NOTE: cull the tracktrain by PHS because it's may out of PVS
	// and hanging the moving sound
	SetBits( pev->effects, EF_REQUEST_PHS );

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );

	// Cache off placed origin for train controls
	m_controlOrigin = GetLocalOrigin();

	m_controlMins = pev->mins;
	m_controlMaxs = pev->maxs;
	m_controlMaxs.z += 72;
// start trains on the next frame, to make sure their targets have had
// a chance to spawn/activate
	SetThink( &CFuncTrackTrain::Find );
	SetNextThink( 0.0f );
	Precache();
}

void CFuncTrackTrain :: Precache( void )
{
	if( !m_flVolume )
		m_flVolume = 1.0f;

	int m_sound = UTIL_LoadSoundPreset( m_sounds );

	switch( m_sound )
	{
	case 0: pev->noise = 0; break; // no sound
	case 1: pev->noise = UTIL_PrecacheSound( "plats/ttrain1.wav" ); break;
	case 2: pev->noise = UTIL_PrecacheSound( "plats/ttrain2.wav" ); break;
	case 3: pev->noise = UTIL_PrecacheSound( "plats/ttrain3.wav" ); break;
	case 4: pev->noise = UTIL_PrecacheSound( "plats/ttrain4.wav" ); break;
	case 5: pev->noise = UTIL_PrecacheSound( "plats/ttrain6.wav" ); break;
	case 6: pev->noise = UTIL_PrecacheSound( "plats/ttrain7.wav" ); break;
	default: pev->noise = UTIL_PrecacheSound( m_sound ); break;
	}

	if( m_soundStart != NULL_STRING )
		pev->noise1 = UTIL_PrecacheSound( m_soundStart );
	else pev->noise1 = UTIL_PrecacheSound( "plats/ttrain_start1.wav" );

	if( m_soundStop != NULL_STRING )
		pev->noise2 = UTIL_PrecacheSound( m_soundStop );
	else pev->noise2 = UTIL_PrecacheSound( "plats/ttrain_brake1.wav" );
}

void CFuncTrackTrain :: UpdateOnRemove( void )
{
	StopSound();
	BaseClass::UpdateOnRemove();
}
