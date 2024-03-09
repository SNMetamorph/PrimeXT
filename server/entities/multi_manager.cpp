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

#include "multi_manager.h"

LINK_ENTITY_TO_CLASS( multi_manager, CMultiManager );

// Global Savedata for multi_manager
BEGIN_DATADESC( CMultiManager )
	DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
	DEFINE_FIELD( m_index, FIELD_INTEGER ),
	DEFINE_FIELD( m_startTime, FIELD_TIME ),
	DEFINE_AUTO_ARRAY( m_iTargetName, FIELD_STRING ),
	DEFINE_AUTO_ARRAY( m_flTargetDelay, FIELD_FLOAT ),
	DEFINE_FUNCTION( ManagerThink ),
#if _DEBUG
	DEFINE_FUNCTION( ManagerReport ),
#endif
END_DATADESC()

void CMultiManager :: KeyValue( KeyValueData *pkvd )
{
	// UNDONE: Maybe this should do something like this:
	// CBaseToggle::KeyValue( pkvd );
	// if ( !pkvd->fHandled )
	// ... etc.

	if( FStrEq( pkvd->szKeyName, "delay" ))
	{
		m_flDelay = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "master" ))
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
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
			m_flTargetDelay[m_cTargets] = Q_atof( pkvd->szValue );
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}


void CMultiManager :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetThink( &CMultiManager::ManagerThink );

	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while( swapped )
	{
		swapped = 0;

		for( int i = 1; i < m_cTargets; i++ )
		{
			if( m_flTargetDelay[i] < m_flTargetDelay[i-1] )
			{
				// swap out of order elements
				int name = m_iTargetName[i];
				float delay = m_flTargetDelay[i];
				m_iTargetName[i] = m_iTargetName[i-1];
				m_flTargetDelay[i] = m_flTargetDelay[i-1];
				m_iTargetName[i-1] = name;
				m_flTargetDelay[i-1] = delay;
				swapped = 1;
			}
		}
	}

	// HACKHACK: fix env_laser on a c2a1
	if( FStrEq( STRING( gpGlobals->mapname ), "c2a1" ) && FStrEq( GetTargetname(), "gargbeams_mm" ))
		pev->spawnflags = 0;

	if( FBitSet( pev->spawnflags, SF_MULTIMAN_START_ON ))
	{		
		SetThink( &CBaseEntity::SUB_CallUseToggle );
		SetNextThink( 0.1 );
	}
	
 	m_iState = STATE_OFF;
}

BOOL CMultiManager::HasTarget( string_t targetname )
{ 
	for( int i = 0; i < m_cTargets; i++ )
	{
		if( FStrEq( STRING( targetname ), STRING( m_iTargetName[i] )) )
			return TRUE;
	}
	return FALSE;
}

// Designers were using this to fire targets that may or may not exist -- 
// so I changed it to use the standard target fire code, made it a little simpler.
void CMultiManager :: ManagerThink ( void )
{
	float	time;

	time = gpGlobals->time - m_startTime;
	while( m_index < m_cTargets && m_flTargetDelay[m_index] <= time )
	{
		UTIL_FireTargets( STRING( m_iTargetName[m_index] ), m_hActivator, this, USE_TOGGLE, pev->frags );
		m_index++;
	}

	// have we fired all targets?
	if( m_index >= m_cTargets )
	{
		if( FBitSet( pev->spawnflags, SF_MULTIMAN_LOOP ))
		{
			// starts new cycle
			m_startTime = m_flDelay + gpGlobals->time;
			m_iState = STATE_TURN_ON;
			SetNextThink( m_flDelay );
			SetThink( &CMultiManager::ManagerThink );
			m_index = 0;
		}
		else
		{
			m_iState = STATE_OFF;
			SetThink( NULL );
			DontThink();
		}

		if( IsClone( ) || FBitSet( pev->spawnflags, SF_MULTIMAN_ONLYONCE ))
		{
			UTIL_Remove( this );
			return;
		}
	}
	else
	{
		pev->nextthink = m_startTime + m_flTargetDelay[m_index];
	}
}

CMultiManager *CMultiManager::Clone( void )
{
	CMultiManager *pMulti = GetClassPtr(( CMultiManager *)NULL );

	edict_t *pEdict = pMulti->pev->pContainingEntity;
	memcpy( pMulti->pev, pev, sizeof( *pev ));
	pMulti->pev->pContainingEntity = pEdict;

	pMulti->pev->spawnflags |= SF_MULTIMAN_CLONE;
	pMulti->pev->spawnflags &= ~SF_MULTIMAN_THREAD; // g-cont. to prevent recursion
	memcpy( pMulti->m_iTargetName, m_iTargetName, sizeof( m_iTargetName ));
	memcpy( pMulti->m_flTargetDelay, m_flTargetDelay, sizeof( m_flTargetDelay ));
	pMulti->m_cTargets = m_cTargets;

	return pMulti;
}


// The USE function builds the time table and starts the entity thinking.
void CMultiManager :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	pev->frags = value;

	if( IsLockedByMaster( ))
		return;

 	if( useType == USE_SET )
 	{ 
		m_index = 0; // reset fire index
		while( m_index < m_cTargets )
		{
			// firing all targets instantly
			UTIL_FireTargets( m_iTargetName[ m_index ], this, this, USE_TOGGLE );
			m_index++;
			if( m_hActivator == this )
				break; // break if current target - himself
		}
 	}

	if( FBitSet( pev->spawnflags, SF_MULTIMAN_LOOP ))
	{
		if( m_iState != STATE_OFF ) // if we're on, or turning on...
		{
			if( useType != USE_ON ) // ...then turn it off if we're asked to.
			{
				m_iState = STATE_OFF;
				if( IsClone() || FBitSet( pev->spawnflags, SF_MULTIMAN_ONLYONCE ))
				{
					SetThink( &CBaseEntity::SUB_Remove );
					SetNextThink( 0.1 );
				}
				else
				{
					SetThink( NULL );
					DontThink();
				}
			}
			return;
		}
		else if( useType == USE_OFF )
		{
			return;
		}
		// otherwise, start firing targets as normal.
	}

	// In multiplayer games, clone the MM and execute in the clone (like a thread)
	// to allow multiple players to trigger the same multimanager
	if( ShouldClone( ))
	{
		CMultiManager *pClone = Clone();
		pClone->Use( pActivator, pCaller, useType, value );
		return;
	}

	if( ShouldToggle( useType ))
	{
		if( useType == USE_ON || useType == USE_TOGGLE || useType == USE_RESET )
		{
			if( m_iState == STATE_OFF || useType == USE_RESET )
			{
				m_startTime = m_flDelay + gpGlobals->time;
				m_iState = STATE_TURN_ON;
				SetNextThink( m_flDelay );
				SetThink( &CMultiManager::ManagerThink );
				m_index = 0;
			}
		}	
		else if( useType == USE_OFF )
		{	
			m_iState = STATE_OFF;
			SetThink( NULL );
			DontThink();
		}
	}
}

#if _DEBUG
void CMultiManager :: ManagerReport ( void )
{
	for( int cIndex = 0; cIndex < m_cTargets; cIndex++ )
	{
		ALERT( at_console, "%s %f\n", STRING(m_iTargetName[cIndex]), m_flTargetDelay[cIndex] );
	}
}
#endif
