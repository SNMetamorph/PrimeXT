/*
multi_switcher.cpp - target switcher that can shuffle targets randomly
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

#include "multi_switcher.h"

LINK_ENTITY_TO_CLASS( multi_switcher, CSwitcher );

// Global Savedata for switcher
BEGIN_DATADESC( CSwitcher )
	DEFINE_FIELD( m_index, FIELD_INTEGER ),
	DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iTargetName, FIELD_STRING ),
END_DATADESC()

void CSwitcher :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "mode" ))
	{
		pev->button = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( m_cTargets < MAX_MULTI_TARGETS ) 
	{
		// add this field to the target list
		// this assumes that additional fields are targetnames and their values are delay values.
		
		char tmp[128];
		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iTargetName[m_cTargets] = ALLOC_STRING( tmp );
		m_iTargetOrder[m_cTargets] = Q_atoi( pkvd->szValue ); 
		m_cTargets++;
		pkvd->fHandled = TRUE;
	}
}

void CSwitcher :: Spawn( void )
{
	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while( swapped )
	{
		swapped = 0;

		for( int i = 1; i < m_cTargets; i++ )
		{
			if( m_iTargetOrder[i] < m_iTargetOrder[i-1] )
			{
				// swap out of order elements
				int name = m_iTargetName[i];
				int order = m_iTargetOrder[i];
				m_iTargetName[i] = m_iTargetName[i-1];
				m_iTargetOrder[i] = m_iTargetOrder[i-1];
				m_iTargetName[i-1] = name;
				m_iTargetOrder[i-1] = order;
				swapped = 1;
			}
		}
	}

	m_iState = STATE_OFF;
	m_index = 0;

	if( FBitSet( pev->spawnflags, SF_SWITCHER_START_ON ))
	{
 		SetNextThink( m_flDelay );
		m_iState = STATE_ON;
	}	
}

void CSwitcher :: Think( void )
{
	if( pev->button == MODE_INCREMENT )
	{
		// increase target number
		m_index++;
		if( m_index >= m_cTargets )
			m_index = 0;
	}
	else if( pev->button == MODE_DECREMENT )
	{
		m_index--;
		if( m_index < 0 )
			m_index = m_cTargets - 1;
	}
	else if( pev->button == MODE_RANDOM_VALUE )
	{
		m_index = RANDOM_LONG( 0, m_cTargets - 1 );
	}

	SetNextThink( m_flDelay );
}

void CSwitcher :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( IsLockedByMaster( ))
		return;

	if( useType == USE_SET )
	{
		// set new target for activate (direct choose or increment\decrement)
		if( FBitSet( pev->spawnflags, SF_SWITCHER_START_ON ))
		{
			m_iState = STATE_ON;
			SetNextThink( m_flDelay );
			return;
		}

		// set maximum priority for direct choose
		if( value ) 
		{
			m_index = (value - 1);
			if( m_index >= m_cTargets || m_index < -1 )
				m_index = -1;
			return;
		}

		if( pev->button == MODE_INCREMENT )
		{
			m_index++;
			if( m_index >= m_cTargets )
				m_index = 0; 	
		}
                    else if( pev->button == MODE_DECREMENT )
		{
			m_index--;
			if( m_index < 0 )
				m_index = m_cTargets - 1;
		}
		else if( pev->button == MODE_RANDOM_VALUE )
		{
			m_index = RANDOM_LONG( 0, m_cTargets - 1 );
		}
	}
	else if( useType == USE_RESET )
	{
		// reset switcher
		m_iState = STATE_OFF;
		DontThink();
		m_index = 0;
		return;
	}
	else if( m_index != -1 ) // fire any other USE_TYPE and right index
	{
		UTIL_FireTargets( m_iTargetName[m_index], m_hActivator, this, useType, value );
	}
}
