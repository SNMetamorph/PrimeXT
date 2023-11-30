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

#include "multisource.h"

BEGIN_DATADESC( CMultiSource )
	DEFINE_AUTO_ARRAY( m_rgEntities, FIELD_EHANDLE ),
	DEFINE_AUTO_ARRAY( m_rgTriggered, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_globalstate, FIELD_STRING, "globalstate" ),
	DEFINE_FIELD( m_iTotal, FIELD_INTEGER ),
	DEFINE_FUNCTION( Register ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( multisource, CMultiSource );

void CMultiSource :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "offtarget" ))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "globalstate" ))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CMultiSource :: Spawn()
{ 
	// set up think for later registration
	SetBits( pev->spawnflags, SF_MULTI_INIT ); // until it's initialized
	SetThink( &CMultiSource::Register );
	SetNextThink( 0.1 );
}

void CMultiSource :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	int i = 0;

	// find the entity in our list
	while( i < m_iTotal )
	{
		if( m_rgEntities[i++] == pCaller )
			break;
	}

	// if we didn't find it, report error and leave
	if( i > m_iTotal )
	{
		if( pCaller->GetTargetname( ))
			ALERT( at_error, "multisource \"%s\": Used by non-member %s \"%s\"\n", GetTargetname(), pCaller->GetTargetname( ));
		else ALERT( at_error, "multisource \"%s\": Used by non-member %s\n", GetTargetname(), pCaller->GetClassname( ));
		return;	
	}

	// store the state before the change, so we can compare it to the new state
	STATE s = GetState();

	// do the change
	m_rgTriggered[i-1] ^= 1;

	// did we change state?
	if( s == GetState( )) return;

	if( s == STATE_ON && !FStringNull( pev->netname ))
	{
		// the change disabled me and I have a "fire on disable" field
		ALERT( at_aiconsole, "Multisource %s deactivated (%d inputs)\n", GetTargetname(), m_iTotal );

		if( m_globalstate )
			UTIL_FireTargets( STRING( pev->netname ), NULL, this, USE_OFF, 0 );
		else UTIL_FireTargets( STRING( pev->netname ), NULL, this, USE_TOGGLE, 0 );
	}
	else if( s == STATE_OFF )
	{
		// the change activated me
		ALERT( at_aiconsole, "Multisource %s enabled (%d inputs)\n", GetTargetname(), m_iTotal );

		if( m_globalstate )
			UTIL_FireTargets( STRING( pev->target ), NULL, this, USE_ON, 0 );
		else UTIL_FireTargets( STRING( pev->target ), NULL, this, USE_TOGGLE, 0 );
	}
}

STATE CMultiSource :: GetState( void )
{
	// still initializing?
	if( FBitSet( pev->spawnflags, SF_MULTI_INIT ))
		return STATE_OFF;

	int i = 0;

	while( i < m_iTotal )
	{
		if( m_rgTriggered[i] == 0 )
			break;
		i++;
	}

	if( i == m_iTotal )
	{
		if( !m_globalstate || gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
			return STATE_ON;
	}
	return STATE_OFF;
}

void CMultiSource :: Register( void )
{ 
	m_iTotal = 0;
	memset( m_rgEntities, 0, MAX_MASTER_TARGETS * sizeof( EHANDLE ));

	SetThink( NULL );

	// search for all entities which target this multisource (pev->target)
	CBaseEntity *pTarget = UTIL_FindEntityByTarget( NULL, GetTargetname( ));

	while( pTarget && ( m_iTotal < MAX_MASTER_TARGETS ))
	{
		m_rgEntities[m_iTotal++] = pTarget;
		pTarget = UTIL_FindEntityByTarget( pTarget, GetTargetname( ));
	}

	// search for all monsters which target this multisource (TriggerTarget)
	pTarget = UTIL_FindEntityByMonsterTarget( NULL, GetTargetname( ));

	while( pTarget && ( m_iTotal < MAX_MASTER_TARGETS ))
	{
		m_rgEntities[m_iTotal++] = pTarget;
		pTarget = UTIL_FindEntityByMonsterTarget( pTarget, GetTargetname( ));
	}

	pTarget = UTIL_FindEntityByClassname( NULL, "multi_manager" );

	while( pTarget && ( m_iTotal < MAX_MASTER_TARGETS ))
	{
		if( pTarget->HasTarget( pev->targetname ))
			m_rgEntities[m_iTotal++] = pTarget;
		pTarget = UTIL_FindEntityByClassname( pTarget, "multi_manager" );
	}

	ClearBits( pev->spawnflags, SF_MULTI_INIT );
}