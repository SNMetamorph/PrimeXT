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

#include "generator.h"

LINK_ENTITY_TO_CLASS( generator, CGenerator );

BEGIN_DATADESC( CGenerator )
	DEFINE_KEYFIELD( m_iMaxFireCount, FIELD_INTEGER, "maxcount" ),
	DEFINE_FIELD( m_iFireCount, FIELD_INTEGER ),
END_DATADESC()

void CGenerator :: Spawn( void )
{
	if( pev->button == RANDOM_MODE || pev->button == RANDOM_MODE_WITH_DEC )
	{ 	
		// set delay with decceleration
		if( !m_flDelay || pev->button == RANDOM_MODE_WITH_DEC )
			m_flDelay = 0.05;

		// generate max count automaticallly, if not set on map
		if( !m_iMaxFireCount ) m_iMaxFireCount = RANDOM_LONG( 100, 200 );
	}
	else
	{
		// Smart Field System ©
		if( !m_iMaxFireCount ) m_iMaxFireCount = -1; // disable counting for normal mode
	}

	if( FBitSet( pev->spawnflags, SF_GENERATOR_START_ON ))
	{
		m_iState = STATE_ON; // initialy off in random mode
		SetNextThink( m_flDelay );
	}
}

void CGenerator :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "maxcount" ))
	{
		m_iMaxFireCount = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if( FStrEq( pkvd->szKeyName, "mode" ))
	{
		pev->button = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CGenerator :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_TOGGLE )
	{
		if( m_iState )
			useType = USE_OFF;
		else useType = USE_ON;
	}

	if( useType == USE_ON )
	{
		if( pev->button == RANDOM_MODE || pev->button == RANDOM_MODE_WITH_DEC )
		{
			if( pev->button == RANDOM_MODE_WITH_DEC )
				m_flDelay = 0.05f;
			m_iFireCount = RANDOM_LONG( 0, m_iMaxFireCount / 2 );
		}

		m_iState = STATE_ON;
		SetNextThink( 0 ); // immediately start firing targets
	}
	else if( useType == USE_OFF )
	{
		m_iState = STATE_OFF;
		DontThink();
	}
	else if( useType == USE_SET )
	{
		// set max count of impulses
		m_iMaxFireCount = value;
	}		
	else if( useType == USE_RESET )
	{
		// immediately reset
		m_iFireCount = 0;
	}
}

void CGenerator :: Think( void )
{
	if( m_iFireCount != -1 )
	{
		// if counter enabled
		if( m_iFireCount == m_iMaxFireCount )
		{
			m_iFireCount = NULL;
			m_iState = STATE_OFF;
			DontThink();
			return;
		}
 		else m_iFireCount++;
	}

	if( pev->button == RANDOM_MODE_WITH_DEC )
	{	
		// deceleration for random mode
		if( m_iMaxFireCount - m_iFireCount < 40 )
			m_flDelay += 0.05;
	}

	UTIL_FireTargets( pev->target, this, this, USE_TOGGLE );
	SetNextThink( m_flDelay );
}
