/*
multi_watcher.cpp - an entity that implements the “if-else” logical condition
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "multi_watcher.h"

LINK_ENTITY_TO_CLASS( multi_watcher, CMultiMaster );

BEGIN_DATADESC( CMultiMaster )
	DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iLogicMode, FIELD_INTEGER, "logic" ),
	DEFINE_FIELD( m_iSharedState, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iTargetName, FIELD_STRING ),
	DEFINE_AUTO_ARRAY( m_iTargetState, FIELD_INTEGER ),
END_DATADESC()

void CMultiMaster :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "logic" ))
	{
		m_iLogicMode = GetLogicModeForString( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "state" ))
	{
		m_iSharedState = GetStateForString( pkvd->szValue );
		pkvd->fHandled = TRUE;
		globalstate = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "offtarget" ))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "parent" ) || FStrEq( pkvd->szKeyName, "movewith" ))
	{
		m_iParent = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if( m_cTargets < MAX_MULTI_TARGETS )
		{
			char tmp[128];
			
			UTIL_StripToken( pkvd->szKeyName, tmp );
			m_iTargetName[m_cTargets] = ALLOC_STRING( tmp );
			m_iTargetState[m_cTargets] = GetStateForString( pkvd->szValue );
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}

int CMultiMaster :: GetLogicModeForString( const char *string )
{
	if( !Q_stricmp( string, "AND" ))
		return LOGIC_AND;
	else if( !Q_stricmp( string, "OR" ))
		return LOGIC_OR;
	else if( !Q_stricmp( string, "NAND" ) || !Q_stricmp( string, "!AND" ))
		return LOGIC_NAND;
	else if( !Q_stricmp( string, "NOR" ) || !Q_stricmp( string, "!OR" ))
		return LOGIC_NOR;
	else if( !Q_stricmp( string, "XOR" ) || !Q_stricmp( string, "^OR" ))
		return LOGIC_XOR;
	else if( !Q_stricmp( string, "XNOR" ) || !Q_stricmp( string, "^!OR" ))
		return LOGIC_XNOR;
	else if( Q_isdigit( string ))
		return Q_atoi( string );

	// assume error
	ALERT( at_error, "Unknown logic mode '%s' specified\n", string );
	return -1;
}

void CMultiMaster :: Spawn( void )
{
	// use local states instead
	if( !globalstate )
	{
		m_iSharedState = (STATE)-1;
	}

	if( !FBitSet( pev->spawnflags, SF_WATCHER_START_OFF ))
		SetNextThink( 0.1 );
}

bool CMultiMaster :: CheckState( STATE state, int targetnum )
{
	// global state for all targets
	if( m_iSharedState != -1 )
	{
		if( m_iSharedState == state )
			return TRUE;
		return FALSE;
	}

	if((STATE)m_iTargetState[targetnum] == state )
		return TRUE;
	return FALSE;
}

void CMultiMaster :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !ShouldToggle( useType, ( pev->nextthink != 0 )))
		return;

	if( pev->nextthink == 0 )
	{
 		SetNextThink( 0.01 );
	}
	else
	{
		// disabled watcher is always off
		m_iState = STATE_OFF;
		DontThink();
	}
}

void CMultiMaster :: Think ( void )
{
	if( EvalLogic( NULL )) 
	{
		if( m_iState == STATE_OFF )
		{
			m_iState = STATE_ON;
			UTIL_FireTargets( pev->target, this, this, USE_ON );
		}
	}
	else 
	{
		if( m_iState == STATE_ON )
		{
			m_iState = STATE_OFF;
			UTIL_FireTargets( pev->netname, this, this, USE_OFF );
		}
          }

 	SetNextThink( 0.01 );
}

BOOL CMultiMaster :: EvalLogic( CBaseEntity *pActivator )
{
	BOOL xorgot = FALSE;
	CBaseEntity *pEntity;

	for( int i = 0; i < m_cTargets; i++ )
	{
		pEntity = UTIL_FindEntityByTargetname( NULL, STRING( m_iTargetName[i] ), pActivator );
		if( !pEntity ) continue;

		// handle the states for this logic mode
		if( CheckState( pEntity->GetState( ), i ))
		{
			switch( m_iLogicMode )
			{
			case LOGIC_OR:
				return TRUE;
			case LOGIC_NOR:
				return FALSE;
			case LOGIC_XOR: 
				if( xorgot )
					return FALSE;
				xorgot = TRUE;
				break;
			case LOGIC_XNOR:
				if( xorgot )
					return TRUE;
				xorgot = TRUE;
				break;
			}
		}
		else // state is false
		{
			switch( m_iLogicMode )
			{
	         		case LOGIC_AND:
	         			return FALSE;
			case LOGIC_NAND:
				return TRUE;
			}
		}
	}

	// handle the default cases for each logic mode
	switch( m_iLogicMode )
	{
	case LOGIC_AND:
	case LOGIC_NOR:
		return TRUE;
	case LOGIC_XOR:
		return xorgot;
	case LOGIC_XNOR:
		return !xorgot;
	default:
		return FALSE;
	}
}
