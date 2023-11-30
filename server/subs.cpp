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
/*

===== subs.cpp ========================================================

  frequently used global functions

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "nodes.h"
#include "func_door.h"

#define MOVE_TOGGLE_NONE		0
#define MOVE_TOGGLE_LINEAR		1
#define MOVE_TOGGLE_ANGULAR		2
#define MOVE_TOGGLE_COMPLEX		3

// Landmark class
void CPointEntity :: Spawn( void )
{
	pev->solid = SOLID_NOT;
//	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}

// Null Entity, remove on startup
class CNullEntity : public CBaseEntity
{
	DECLARE_CLASS( CNullEntity, CBaseEntity );
public:
	void Spawn( void ) { REMOVE_ENTITY( edict() ); }
};

LINK_ENTITY_TO_CLASS( info_null, CNullEntity );
LINK_ENTITY_TO_CLASS( info_texlights, CNullEntity ); // don't complain about Merl's new info entities
LINK_ENTITY_TO_CLASS( info_compile_parameters, CNullEntity );
LINK_ENTITY_TO_CLASS( env_fireball, CNullEntity ); // no env_fireball in beta version

class CBaseDMStart : public CPointEntity
{
	DECLARE_CLASS( CBaseDMStart, CPointEntity );
public:
	void KeyValue( KeyValueData *pkvd );
	BOOL IsTriggered( CBaseEntity *pEntity );
};

// These are the new entry points to entities. 
LINK_ENTITY_TO_CLASS( info_player_deathmatch, CBaseDMStart );
LINK_ENTITY_TO_CLASS( info_player_start, CPointEntity );
LINK_ENTITY_TO_CLASS( info_landmark, CPointEntity );

void CBaseDMStart::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "master"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

BOOL CBaseDMStart::IsTriggered( CBaseEntity *pEntity )
{
	BOOL master = UTIL_IsMasterTriggered( pev->netname, pEntity );

	return master;
}

// Convenient way to delay removing oneself
void CBaseEntity :: SUB_Remove( void )
{
	UpdateOnRemove();
	if (pev->health > 0)
	{
		// this situation can screw up monsters who can't tell their entity pointers are invalid.
		pev->health = 0;
		ALERT( at_aiconsole, "SUB_Remove called on entity with health > 0\n");
	}

	REMOVE_ENTITY(ENT(pev));
}


// Convenient way to explicitly do nothing (passed to functions that require a method)
void CBaseEntity :: SUB_DoNothing( void )
{
}

// Global Savedata for Delay
BEGIN_DATADESC( CBaseDelay )
	DEFINE_KEYFIELD( m_flDelay, FIELD_FLOAT, "delay" ),
	DEFINE_KEYFIELD( m_flWait, FIELD_FLOAT, "wait" ),
	DEFINE_KEYFIELD( m_iszKillTarget, FIELD_STRING, "killtarget" ),
	DEFINE_KEYFIELD( m_sMaster, FIELD_STRING, "master" ),
	DEFINE_FIELD( m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( m_hActivator, FIELD_EHANDLE ),
	DEFINE_FUNCTION( DelayThink ),
END_DATADESC()

void CBaseDelay :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "delay" ))
	{
		m_flDelay = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "wait" ))
	{
		m_flWait = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "killtarget" ))
	{
		m_iszKillTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "master" ))
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseEntity::KeyValue( pkvd );
	}
}

BOOL CBaseDelay :: IsLockedByMaster( void )
{
	if( UTIL_IsMasterTriggered( m_sMaster, m_hActivator ))
		return FALSE;
	return TRUE;
}

BOOL CBaseDelay :: IsLockedByMaster( CBaseEntity *pActivator )
{
	if( UTIL_IsMasterTriggered( m_sMaster, pActivator ))
		return FALSE;
	return TRUE;
}

LINK_ENTITY_TO_CLASS( DelayedEvent, CBaseDelay );

void CBaseDelay :: DelayThink( void )
{
	SUB_UseTargets( m_hActivator, (USE_TYPE)m_iState, pev->scale, pev->netname );
	REMOVE_ENTITY( edict( ));
}

void CBaseDelay :: SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value, string_t m_iszAltTarget )
{
	// exit immediatly if we don't have a target or kill target
	if( FStringNull( pev->target ) && FStringNull( m_iszAltTarget ) && FStringNull( m_iszKillTarget ))
		return;

	// check for a delay
	if( m_flDelay != 0.0f )
	{
		// create a temp object to fire at a later time
		CBaseDelay *pTemp = GetClassPtr(( CBaseDelay *)NULL );
		pTemp->pev->classname = MAKE_STRING( "DelayedEvent" );

		pTemp->SetNextThink( m_flDelay );
		pTemp->SetThink( &CBaseDelay::DelayThink );

		// Save the useType
		pTemp->m_iState = (STATE)useType;
		pTemp->m_iszKillTarget = m_iszKillTarget;
		pTemp->m_flDelay = 0.0f; // prevent "recursion"
		pTemp->pev->netname = m_iszAltTarget;
		pTemp->m_hActivator = pActivator;
		pTemp->pev->target = pev->target;
		pTemp->pev->scale = value;
		return;
	}

	// kill the killtargets
	if( !FStringNull( m_iszKillTarget ))
	{
		CBaseEntity *pKillTarget = NULL;

		ALERT( at_aiconsole, "KillTarget: %s\n", STRING( m_iszKillTarget ));
		pKillTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_iszKillTarget ));

		while( !FNullEnt( pKillTarget ))
		{
			UTIL_Remove( pKillTarget );

			ALERT( at_aiconsole, "killing %s\n", pKillTarget->GetClassname( ));
			pKillTarget = UTIL_FindEntityByTargetname( pKillTarget, STRING( m_iszKillTarget ));
		}
	}
	
	// fire targets
	if( !FStringNull( m_iszAltTarget ))
	{
		UTIL_FireTargets( m_iszAltTarget, pActivator, this, useType, value );
	}
	else if( !FStringNull( pev->target ))
	{
		UTIL_FireTargets( GetTarget(), pActivator, this, useType, value );
	}
}

// Global Savedata for Toggle
BEGIN_DATADESC( CBaseToggle )
	DEFINE_FIELD( m_toggle_state, FIELD_INTEGER ),
	DEFINE_FIELD( m_flActivateFinished, FIELD_TIME ),
	DEFINE_KEYFIELD( m_flMoveDistance, FIELD_FLOAT, "distance" ),
	DEFINE_KEYFIELD( m_flLip, FIELD_FLOAT, "lip" ),
	DEFINE_FIELD( m_flTWidth, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTLength, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecPosition1, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecPosition2, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecPosition3, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecAngle1, FIELD_VECTOR ),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD( m_vecAngle2, FIELD_VECTOR ),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD( m_cTriggersLeft, FIELD_INTEGER ),
	DEFINE_FIELD( m_flHeight, FIELD_FLOAT ),
	DEFINE_FIELD( m_flWidth, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecFinalDest, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecFinalAngle, FIELD_VECTOR ),
	DEFINE_FIELD( m_bitsDamageInflict, FIELD_INTEGER ),	// damage type inflicted
	DEFINE_FIELD( m_movementType, FIELD_INTEGER ),
END_DATADESC()

int CBaseToggle::Restore( CRestore &restore )
{
	int status = BaseClass::Restore(restore);
	if ( !status )
		return 0;

	// ---------------------------------------------------------------
	// HACKHACK: We don't know the space of these vectors until now
	// if they are worldspace, fix them up.
	// ---------------------------------------------------------------
	if( m_hParent )
	{
		Vector parentSpaceOffset;

		// select the properly method
		if( UTIL_CanRotate( this ))
			parentSpaceOffset = restore.modelOriginOffset;
		else parentSpaceOffset = restore.modelSpaceOffset;

//		m_vecPosition1 += parentSpaceOffset;
//		m_vecPosition2 += parentSpaceOffset;
//		m_vecPosition3 += parentSpaceOffset;
//		m_vecFinalDest += parentSpaceOffset;
//		m_vecFloor += parentSpaceOffset;
	}

	return status;
}

void CBaseToggle::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flMoveDistance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CBaseToggle :: MoveDone( void )
{
	switch( m_movementType )
	{
	case MOVE_TOGGLE_LINEAR:
		LinearMoveDone();
		break;
	case MOVE_TOGGLE_ANGULAR:
		AngularMoveDone();
		break;
	case MOVE_TOGGLE_COMPLEX:
		ComplexMoveDone();
		break;
	}

	m_movementType = MOVE_TOGGLE_NONE;

	// call the user movedone callback
	BaseClass::MoveDone();
}

/*
=============
LinearMove

calculate local velocity and pev->nextthink to reach vecDest from
abs origin traveling at flSpeed
===============
*/
void CBaseToggle :: LinearMove( const Vector &vecDest, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "LinearMove:  no speed is defined!");
	
	m_vecFinalDest = vecDest;
	m_movementType = MOVE_TOGGLE_LINEAR;

	// Already there?
	if( vecDest == GetLocalOrigin( ))
	{
		SetMoveDoneTime( -1 );
		MoveDone();
		return;
	}
		
	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - GetLocalOrigin();
	
	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set nextthink to trigger a call to LinearMoveDone when dest is reached
	SetMoveDoneTime( flTravelTime );

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalVelocity( vecDestDelta / flTravelTime );
}


/*
============
After moving, set origin to exact final destination, call "move done" function
============
*/
void CBaseToggle :: LinearMoveDone( void )
{
	UTIL_SetOrigin( this, m_vecFinalDest );
	SetLocalVelocity( g_vecZero );
}

/*
=============
AngularMove

calculate local velocity and pev->nextthink to reach vecDest from
abs origin traveling at flSpeed
Just like LinearMove, but rotational.
===============
*/
void CBaseToggle :: AngularMove( const Vector &vecDestAngle, float flSpeed )
{
	ASSERTSZ( flSpeed != 0, "AngularMove:  no speed is defined!" );
	
	m_vecFinalAngle = vecDestAngle;
	m_movementType = MOVE_TOGGLE_ANGULAR;

	// Already there?
	if( vecDestAngle == GetLocalAngles( ))
	{
		SetMoveDoneTime( -1 );
		MoveDone();
		return;
	}
	
	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDestAngle - GetLocalAngles();
	
	// divide by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set nextthink to trigger a call to AngularMoveDone when dest is reached
	SetMoveDoneTime( flTravelTime );

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalAvelocity( vecDestDelta / flTravelTime );
}

/*
============
After rotating, set angle to exact final angle, call "move done" function
============
*/
void CBaseToggle :: AngularMoveDone( void )
{
	UTIL_SetAngles( this, m_vecFinalAngle );
	SetLocalAvelocity( g_vecZero );
}

void CBaseToggle :: ComplexMove( const Vector &vecDest, const Vector &vecDestAngle, float flSpeed )
{
	ASSERTSZ( flSpeed != 0, "ComplexMove:  no speed is defined!" );

	m_vecFinalDest = vecDest;
	m_vecFinalAngle = vecDestAngle;
	m_movementType = MOVE_TOGGLE_COMPLEX;

	// Already there?
	if( vecDest == GetLocalOrigin() && vecDestAngle == GetLocalAngles( ))
	{
		SetMoveDoneTime( -1 );
		MoveDone();
		return;
	}
	
	// Calculate TravelTime and final angles
	Vector vecDestLDelta = vecDest - GetLocalOrigin();
	Vector vecDestADelta = vecDestAngle - GetLocalAngles();
	
	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestLDelta.Length() / flSpeed;
          
	// set nextthink to trigger a call to LinearMoveDone when dest is reached
	SetMoveDoneTime( flTravelTime );

	// set linear and angular velocity now
	SetLocalVelocity( vecDestLDelta / flTravelTime );
	SetLocalAvelocity( vecDestADelta / flTravelTime );
}

void CBaseToggle :: ComplexMoveDone( void )
{
	UTIL_SetOrigin( this, m_vecFinalDest );
         	UTIL_SetAngles( this, m_vecFinalAngle );

	SetLocalVelocity( g_vecZero );
	SetLocalAvelocity( g_vecZero );
}

// mapping toggle-states to global states
STATE CBaseToggle :: GetState ( void )
{
	switch( m_toggle_state )
	{
	case TS_AT_TOP: return STATE_ON;
	case TS_AT_BOTTOM: return STATE_OFF;
	case TS_GOING_UP: return STATE_TURN_ON;
	case TS_GOING_DOWN:	return STATE_TURN_OFF;
	default: return STATE_OFF; // This should never happen.
	}
}

float CBaseToggle :: AxisValue( int flags, const Vector &angles )
{
	if ( FBitSet( flags, SF_DOOR_ROTATE_Z ))
		return angles.z;
	if ( FBitSet( flags, SF_DOOR_ROTATE_X ))
		return angles.x;

	return angles.y;
}

void CBaseToggle :: AxisDir( entvars_t *pev )
{
	if( FBitSet( pev->spawnflags, SF_DOOR_ROTATE_Z ))
		pev->movedir = Vector ( 0, 0, 1 );	// around z-axis
	else if( FBitSet( pev->spawnflags, SF_DOOR_ROTATE_X ))
		pev->movedir = Vector ( 1, 0, 0 );	// around x-axis
	else
		pev->movedir = Vector ( 0, 1, 0 );		// around y-axis
}

float CBaseToggle :: AxisDelta( int flags, const Vector &angle1, const Vector &angle2 )
{
	if ( FBitSet( flags, SF_DOOR_ROTATE_Z ))
		return angle1.z - angle2.z;
	
	if ( FBitSet( flags, SF_DOOR_ROTATE_X ))
		return angle1.x - angle2.x;

	return angle1.y - angle2.y;
}
