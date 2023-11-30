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

#include "env_global.h"

BEGIN_DATADESC( CEnvGlobal )
	DEFINE_KEYFIELD( m_globalstate, FIELD_STRING, "globalstate" ),
	DEFINE_KEYFIELD( m_triggermode, FIELD_INTEGER, "triggermode" ),
	DEFINE_KEYFIELD( m_initialstate, FIELD_INTEGER, "initialstate" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_global, CEnvGlobal );

void CEnvGlobal :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "triggermode" ))
	{
		m_triggermode = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "initialstate" ))
	{
		m_initialstate = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "globalstate" ))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CEnvGlobal :: Spawn( void )
{
	if( !m_globalstate )
	{
		REMOVE_ENTITY( edict() );
		return;
	}

	if( FBitSet( pev->spawnflags, SF_GLOBAL_SET ))
	{
		if ( !gGlobalState.EntityInTable( m_globalstate ) )
			gGlobalState.EntityAdd( m_globalstate, gpGlobals->mapname, (GLOBALESTATE)m_initialstate );
	}
}

void CEnvGlobal :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	GLOBALESTATE oldState = gGlobalState.EntityGetState( m_globalstate );
	GLOBALESTATE newState;

	switch( m_triggermode )
	{
	case 0:
		newState = GLOBAL_OFF;
		break;
	case 1:
		newState = GLOBAL_ON;
		break;
	case 2:
		newState = GLOBAL_DEAD;
		break;
	case 3:
	default:
		if( oldState == GLOBAL_ON )
			newState = GLOBAL_OFF;
		else if( oldState == GLOBAL_OFF )
			newState = GLOBAL_ON;
		else newState = oldState;
		break;
	}

	if( gGlobalState.EntityInTable( m_globalstate ))
		gGlobalState.EntitySetState( m_globalstate, newState );
	else gGlobalState.EntityAdd( m_globalstate, gpGlobals->mapname, newState );
}