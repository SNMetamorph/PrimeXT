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

#include "func_door.h"	// for func_traindoor

BEGIN_DATADESC( CBaseDoor )
	DEFINE_KEYFIELD( m_bHealthValue, FIELD_CHARACTER, "healthvalue" ),
	DEFINE_KEYFIELD( m_iMoveSnd, FIELD_STRING, "movesnd" ),
	DEFINE_KEYFIELD( m_iStopSnd, FIELD_STRING, "stopsnd" ),
	DEFINE_KEYFIELD( m_iLockedSound, FIELD_STRING, "locked_sound" ),
	DEFINE_KEYFIELD( m_bLockedSentence, FIELD_CHARACTER, "locked_sentence" ),
	DEFINE_KEYFIELD( m_iUnlockedSound, FIELD_STRING, "unlocked_sound" ),	
	DEFINE_KEYFIELD( m_bUnlockedSentence, FIELD_CHARACTER, "unlocked_sentence" ),	
	DEFINE_KEYFIELD( m_iChainTarget, FIELD_STRING, "chaintarget" ),
	DEFINE_FIELD( m_bDoorGroup, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( DoorTouch ),
	DEFINE_FUNCTION( DoorGoUp ),
	DEFINE_FUNCTION( DoorGoDown ),
	DEFINE_FUNCTION( DoorHitTop ),
	DEFINE_FUNCTION( DoorHitBottom ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_door, CBaseDoor );
LINK_ENTITY_TO_CLASS( func_water, CBaseDoor );

void CBaseDoor::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "movesnd" ))
	{
		m_iMoveSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "stopsnd" ))
	{
		m_iStopSnd = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "healthvalue" ))
	{
		m_bHealthValue = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "locked_sound" ))
	{
		m_iLockedSound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "locked_sentence" ))
	{
		m_bLockedSentence = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "unlocked_sound" ))
	{
		m_iUnlockedSound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "unlocked_sentence" ))
	{
		m_bUnlockedSentence = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "WaveHeight" ))
	{
		pev->scale = Q_atof( pkvd->szValue ) * (1.0f / 8.0f);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "chaintarget" ))
	{
		m_iChainTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass :: KeyValue( pkvd );
}

void CBaseDoor::Precache( void )
{
	const char *pszSound;

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

	// get door button sounds, for doors which are directly 'touched' to open
	if( m_iLockedSound )
	{
		pszSound = UTIL_ButtonSound( m_iLockedSound );
		m_ls.sLockedSound = UTIL_PrecacheSound( pszSound );
	}

	if( m_iUnlockedSound )
	{
		pszSound = UTIL_ButtonSound( m_iUnlockedSound );
		m_ls.sUnlockedSound = UTIL_PrecacheSound( pszSound );
	}

	// get sentence group names, for doors which are directly 'touched' to open

	// get sentence group names, for doors which are directly 'touched' to open
	switch( m_bLockedSentence )
	{
	case 1: m_ls.sLockedSentence = MAKE_STRING( "NA" ); break; // access denied
	case 2: m_ls.sLockedSentence = MAKE_STRING( "ND" ); break; // security lockout
	case 3: m_ls.sLockedSentence = MAKE_STRING( "NF" ); break; // blast door
	case 4: m_ls.sLockedSentence = MAKE_STRING( "NFIRE" ); break; // fire door
	case 5: m_ls.sLockedSentence = MAKE_STRING( "NCHEM" ); break; // chemical door
	case 6: m_ls.sLockedSentence = MAKE_STRING( "NRAD" ); break; // radiation door
	case 7: m_ls.sLockedSentence = MAKE_STRING( "NCON" ); break; // gen containment
	case 8: m_ls.sLockedSentence = MAKE_STRING( "NH" ); break; // maintenance door
	case 9: m_ls.sLockedSentence = MAKE_STRING( "NG" ); break; // broken door
	default: m_ls.sLockedSentence = 0; break;
	}

	switch( m_bUnlockedSentence )
	{
	case 1: m_ls.sUnlockedSentence = MAKE_STRING( "EA" ); break; // access granted
	case 2: m_ls.sUnlockedSentence = MAKE_STRING( "ED" ); break; // security door
	case 3: m_ls.sUnlockedSentence = MAKE_STRING( "EF" ); break; // blast door
	case 4: m_ls.sUnlockedSentence = MAKE_STRING( "EFIRE" ); break; // fire door
	case 5: m_ls.sUnlockedSentence = MAKE_STRING( "ECHEM" ); break; // chemical door
	case 6: m_ls.sUnlockedSentence = MAKE_STRING( "ERAD" ); break; // radiation door
	case 7: m_ls.sUnlockedSentence = MAKE_STRING( "ECON" ); break; // gen containment
	case 8: m_ls.sUnlockedSentence = MAKE_STRING( "EH" ); break; // maintenance door
	default: m_ls.sUnlockedSentence = 0; break;
	}
}

void CBaseDoor::Spawn( void )
{
	if( pev->skin == CONTENTS_NONE )
	{
		// normal door
		if( FBitSet( pev->spawnflags, SF_DOOR_PASSABLE ))
			pev->solid = SOLID_NOT;
		else
			pev->solid = SOLID_BSP;
	}
	else
	{
		SetBits( pev->spawnflags, SF_DOOR_SILENT );
		pev->solid = SOLID_NOT; // special contents
	}

	Precache();

	pev->movetype = MOVETYPE_PUSH;
	SET_MODEL( edict(), GetModel() );

	// NOTE: original Half-Life was contain a bug in LinearMove function
	// while m_flWait was equal 0 then object has stopped forever. See code from quake:
	/*
		void LinearMove( Vector vecDest, float flSpeed )
		{
			...
			...
			...
			if( flTravelTime < 0.1f )
			{
				pev->velocity = g_vecZero;
				pev->nextthink = pev->ltime + 0.1f;
				return;
			}
		}
	*/
	// this block was removed from Half-Life and there no difference
	// between wait = 0 and wait = -1. But in Xash this bug was fixed
	// and level-designer errors is now actual. I'm set m_flWait to -1 for compatibility
	if( m_flWait == 0.0f )
		m_flWait = -1;

	if( pev->speed == 0 )
		pev->speed = 100;

	if( pev->movedir == g_vecZero )
		pev->movedir = Vector( 1.0f, 0.0f, 0.0f );

	m_vecPosition1 = GetLocalOrigin();

	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	Vector vecSize = pev->size - Vector( 2, 2, 2 );
	m_vecPosition2 = m_vecPosition1 + (pev->movedir * (DotProductAbs( pev->movedir, vecSize ) - m_flLip));

	ASSERTSZ( m_vecPosition1 != m_vecPosition2, "door start/end positions are equal" );

	if( FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ))
	{	
		UTIL_SetOrigin( this, m_vecPosition2 );
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = GetLocalOrigin();
	}
	else
	{
		UTIL_SetOrigin( this, m_vecPosition1 );
	}

	if (FStrEq(STRING(pev->classname), "func_water")) {
		m_pUserData = WorldPhysic->CreateTriggerFromEntity(this);
	}
	else {
		m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity(this);
	}

	m_iState = STATE_OFF;

	// if the door is flagged for USE button activation only, use NULL touch function
	if( FBitSet( pev->spawnflags, SF_DOOR_USE_ONLY ))
	{
		SetTouch( NULL );
	}
	else
	{
		// touchable button
		SetTouch( &CBaseDoor::DoorTouch );
	}
}

void CBaseDoor :: OnChangeParent( void )
{
	matrix4x4 parent = GetParentToWorldTransform();
	m_vecPosition1 = parent.VectorITransform( m_vecPosition1 );
	m_vecPosition2 = parent.VectorITransform( m_vecPosition2 );
}

void CBaseDoor :: OnClearParent( void )
{
	matrix4x4 parent = GetParentToWorldTransform();
	m_vecPosition1 = parent.VectorTransform( m_vecPosition1 );
	m_vecPosition2 = parent.VectorTransform( m_vecPosition2 );
}

void CBaseDoor :: SetToggleState( int state )
{
	if( (STATE)state == STATE_ON )
		UTIL_SetOrigin( this, m_vecPosition2 );
	else UTIL_SetOrigin( this, m_vecPosition1 );
}

void CBaseDoor :: Activate( void )
{
	CBaseDoor	*pDoorList[64];
	m_bDoorGroup = true;

	// force movement groups to sync!!!
	int doorCount = GetDoorMovementGroup( pDoorList, ARRAYSIZE( pDoorList ));

	for( int i = 0; i < doorCount; i++ )
	{
		if( pDoorList[i]->pev->movedir == pev->movedir )
		{
			bool error = false;

			if( pDoorList[i]->IsRotatingDoor() )
			{
				error = ( pDoorList[i]->GetLocalAngles() != GetLocalAngles()) ? true : false;
			}
			else 
			{
				error = ( pDoorList[i]->GetLocalOrigin() != GetLocalOrigin()) ? true : false;
			}

			if( error )
			{
				// don't do group blocking
				m_bDoorGroup = false;

				// UNDONE: This should probably fixup m_vecPosition1 & m_vecPosition2
				ALERT( at_aiconsole, "Door group %s has misaligned origin!\n", GetTargetname( ));
			}
		}
	}
}

void CBaseDoor :: ChainUse( USE_TYPE useType, float value )
{
	// to prevent recursion
	if( m_isChaining ) return;

	CBaseEntity *pEnt = NULL;

	while(( pEnt = UTIL_FindEntityByTargetname( pEnt, STRING( m_iChainTarget ))) != NULL )
	{
		if( pEnt == this ) continue;

		CBaseDoor *pDoor = (CBaseDoor *)pEnt;

		if( pDoor )
		{
			pDoor->SetChaining( true );
			pDoor->Use( m_hActivator, this, useType, value );
			pDoor->SetChaining( false );
		}
	}
}

void CBaseDoor :: ChainTouch( CBaseEntity *pOther )
{
	// to prevent recursion
	if( m_isChaining ) return;

	CBaseEntity *pEnt = NULL;

	while(( pEnt = UTIL_FindEntityByTargetname( pEnt, STRING( m_iChainTarget ))) != NULL )
	{
		if( pEnt == this ) continue;

		CBaseDoor *pDoor = (CBaseDoor *)pEnt;

		if( pDoor )
		{
			pDoor->SetChaining( true );
			pDoor->Touch( pOther );
			pDoor->SetChaining( false );
		}
	}
}

void CBaseDoor::DoorTouch( CBaseEntity *pOther )
{
	if( !FStringNull( m_iChainTarget ))
		ChainTouch( pOther );

	m_bDoorTouched = true;

	// ignore touches by anything but players and pushables
	if( !pOther->IsPlayer() && !pOther->IsPushable()) return;

	m_hActivator = pOther; // remember who activated the door

	// If door has master, and it's not ready to trigger, 
	// play 'locked' sound
	if( IsLockedByMaster( ))
		PlayLockSounds( pev, &m_ls, TRUE, FALSE );
	
	// If door is somebody's target, then touching does nothing.
	// You have to activate the owner (e.g. button).
	// g-cont. but allow touch for chain doors
	if( !FStringNull( pev->targetname ) && FStringNull( m_iChainTarget ))
	{
		// play locked sound
		PlayLockSounds( pev, &m_ls, TRUE, FALSE );
		return; 
	}

	if( DoorActivate( ))
	{
		// temporarily disable the touch function, until movement is finished.
		SetTouch( NULL );
	}
}

void CBaseDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( !FStringNull( m_iChainTarget ))
		ChainUse( useType, value );

	m_bDoorTouched = false;

	if( IsLockedByMaster( ))
	{
		PlayLockSounds( pev, &m_ls, TRUE, FALSE );
		return;
	}

	if( FBitSet( pev->spawnflags, SF_DOOR_ONOFF_MODE ))
	{
		if( useType == USE_ON )
		{
			if( m_iState == STATE_OFF )
			{
				// door should open
				if( m_hActivator != NULL && m_hActivator->IsPlayer( ))
					m_hActivator->TakeHealth( m_bHealthValue, DMG_GENERIC );

		 		PlayLockSounds( pev, &m_ls, FALSE, FALSE );
		 		DoorGoUp();
			}
			return;
		}
		else if( useType == USE_OFF )
		{
			if( m_iState == STATE_ON )
			{
		         		DoorGoDown();
			}
	         		return;
		}
		else if( useType == USE_SET )
		{
			// change texture
			pev->frame = !pev->frame;
			return;
		}
	}

	// if not ready to be used, ignore "use" command.
	if( m_iState == STATE_OFF || ( FBitSet( pev->spawnflags, SF_DOOR_NO_AUTO_RETURN ) && m_iState == STATE_ON ) )
		DoorActivate();
}

BOOL CBaseDoor::DoorActivate( void )
{
	if( IsLockedByMaster( ))
		return 0;

	if( FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN ) && m_iState == STATE_ON )
	{
		// door should close
		DoorGoDown();
	}
	else
	{
		// door should open
		if( m_hActivator != NULL && m_hActivator->IsPlayer( ))
			m_hActivator->TakeHealth( m_bHealthValue, DMG_GENERIC );

		// play door unlock sounds
		PlayLockSounds( pev, &m_ls, FALSE, FALSE );
		DoorGoUp();
	}
	return 1;
}

void CBaseDoor::DoorGoUp( void )
{
	entvars_t	*pevActivator;

	// It could be going-down, if blocked.
	ASSERT( m_iState == STATE_OFF || m_iState == STATE_TURN_OFF );

	// emit door moving and stop sounds on CHAN_STATIC so that the multicast doesn't
	// filter them out and leave a client stuck with looping door sounds!
	if( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ))
		EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise1 ), 1, ATTN_NORM );

	m_iState = STATE_TURN_ON;
	SetMoveDone( &CBaseDoor::DoorHitTop );

	if( IsRotatingDoor( ))
	{
		float sign = 1.0;

		// !!! BUGBUG Triggered doors don't work with this yet
		if( m_hActivator != NULL && m_bDoorTouched )
		{
			pevActivator = m_hActivator->pev;

			// Y axis rotation, move away from the player			
			if( !FBitSet( pev->spawnflags, SF_DOOR_ONEWAY ) && pev->movedir.y )
			{
				// Positive is CCW, negative is CW, so make 'sign' 1 or -1 based on which way we want to open.
				// Important note:  All doors face East at all times, and twist their local angle to open.
				// So you can't look at the door's facing to determine which way to open.

				Vector nearestPoint;

				CalcNearestPoint( m_hActivator->GetAbsOrigin(), &nearestPoint );
				Vector activatorToNearestPoint = nearestPoint - m_hActivator->GetAbsOrigin();
				activatorToNearestPoint.z = 0;

				Vector activatorToOrigin = GetAbsOrigin() - m_hActivator->GetAbsOrigin();
				activatorToOrigin.z = 0;

				// Point right hand at door hinge, curl hand towards closest spot on door, if thumb
				// is up, open door CW. -- Department of Basic Cross Product Understanding for Noobs
				// g-cont. MWA-HA-HA!
				Vector cross = activatorToOrigin.Cross( activatorToNearestPoint );
				if( cross.z > 0.0f ) sign = -1.0f;	
			}
		}

		AngularMove( m_vecAngle2 * sign, pev->speed );
	}
	else
	{
		LinearMove( m_vecPosition2, pev->speed );
	}
}

void CBaseDoor::DoorHitTop( void )
{
	if( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ))
	{
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise1 ));
		EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise2 ), 1, ATTN_NORM );
	}

	ASSERT( m_iState == STATE_TURN_ON );
	m_iState = STATE_ON;
	
	// toggle-doors don't come down automatically, they wait for refire.
	if( FBitSet( pev->spawnflags, SF_DOOR_NO_AUTO_RETURN ))
	{
		// re-instate touch method, movement is complete
		if( !FBitSet( pev->spawnflags, SF_DOOR_USE_ONLY ))
			SetTouch( &CBaseDoor::DoorTouch );
	}
	else
	{
		// in flWait seconds, DoorGoDown will fire, unless wait is -1, then door stays open
		SetMoveDoneTime( m_flWait );
		SetMoveDone( &CBaseDoor::DoorGoDown );

		if( m_flWait == -1 )
		{
			SetNextThink( -1 );
		}
	}

	// fire the close target (if startopen is set, then "top" is closed) - netname is the close target
	if( FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ))
	{
		UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
	}
	else
	{
		UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
	}

	if( FBitSet( pev->spawnflags, SF_DOOR_ONOFF_MODE ))
		SUB_UseTargets( m_hActivator, USE_OFF, 0 ); 
	else SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 ); // this isn't finished
}

void CBaseDoor::DoorGoDown( void )
{
	if( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ))
		EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise1 ), 1, ATTN_NORM );
	
	ASSERT( m_iState == STATE_ON || m_iState == STATE_TURN_ON );
	m_iState = STATE_TURN_OFF;

	SetMoveDone( &CBaseDoor::DoorHitBottom );

	if( IsRotatingDoor( ))
		AngularMove( m_vecAngle1, pev->speed );
	else LinearMove( m_vecPosition1, pev->speed );
}

void CBaseDoor::DoorHitBottom( void )
{
	if( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ))
	{
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise1 ));
		EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise2 ), 1, ATTN_NORM );
	}

	ASSERT( m_iState == STATE_TURN_OFF );
	m_iState = STATE_OFF;

	// re-instate touch method, cycle is complete
	if( FBitSet( pev->spawnflags, SF_DOOR_USE_ONLY ))
	{
		// use only door
		SetTouch( NULL );
	}
	else
	{
		// touchable door
		SetTouch( &CBaseDoor::DoorTouch );
	}

	// fire the close target (if startopen is set, then "top" is closed) - netname is the close target
	if( !FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ))
	{
		UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
	}
	else
	{
		UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
	}

	if( FBitSet( pev->spawnflags, SF_DOOR_ONOFF_MODE ))
		SUB_UseTargets( m_hActivator, USE_ON, 0 );
	else SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
}

// lists all doors in the same movement group as this one
int CBaseDoor :: GetDoorMovementGroup( CBaseDoor *pDoorList[], int listMax )
{
	CBaseEntity *pTarget = NULL;
	int count = 0;

	// block all door pieces with the same targetname here.
	if( !FStringNull( pev->targetname ))
	{
		while(( pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->targetname ))) != NULL )
		{
			if( pTarget != this && FClassnameIs( pTarget, GetClassname() ))
			{
				CBaseDoor *pDoor = (CBaseDoor *)pTarget;

				if( pDoor && count < listMax )
				{
					pDoorList[count] = pDoor;
					count++;
				}
			}
		}
	}

	return count;
}

void CBaseDoor::Blocked( CBaseEntity *pOther )
{
	// hurt the blocker a little.
	if( pev->dmg ) pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast
	if( m_flWait >= 0 )
	{
		if( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ))
			STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise1 ));

		if( m_iState == STATE_TURN_OFF )
		{
			DoorGoUp();
		}
		else
		{
			DoorGoDown();
		}
	}

	// block all door pieces with the same targetname here.
	if( !FStringNull( pev->targetname ))
	{
		CBaseDoor *pDoorList[64];
		int doorCount = GetDoorMovementGroup( pDoorList, ARRAYSIZE( pDoorList ));

		for( int i = 0; i < doorCount; i++ )
		{
			CBaseDoor *pDoor = pDoorList[i];

			if( pDoor->m_flWait >= 0)
			{
				if( m_bDoorGroup && pDoor->pev->movedir == pev->movedir && pDoor->GetAbsVelocity() == GetAbsVelocity() && pDoor->GetLocalAvelocity() == GetLocalAvelocity( ))
				{
					pDoor->m_iPhysicsFrame = g_ulFrameCount; // don't run physics this frame if you haven't run yet

					// this is the most hacked, evil, bastardized thing I've ever seen. kjb
					if( !pDoor->IsRotatingDoor( ))
					{
						// set origin to realign normal doors
						pDoor->SetLocalOrigin( GetLocalOrigin( ));
						pDoor->SetAbsVelocity( g_vecZero ); // stop!

					}
					else
					{
						// set angles to realign rotating doors
						pDoor->SetLocalAngles( GetLocalAngles( ));
						pDoor->SetLocalAvelocity( g_vecZero );
					}
				}
			
				if( pDoor->m_iState == STATE_TURN_OFF )
					pDoor->DoorGoUp();
				else pDoor->DoorGoDown();
			}
		}
	}
}
