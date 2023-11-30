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

#include "env_local.h"

LINK_ENTITY_TO_CLASS( env_local, CEnvLocal );

void CEnvLocal :: Spawn( void )
{
	if( FBitSet( pev->spawnflags, SF_LOCAL_START_ON ))
		m_iState = STATE_ON;
	else m_iState = STATE_OFF;
}

void CEnvLocal :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	m_hActivator = pActivator;
	pev->scale = value;

	if( useType == USE_TOGGLE )
	{
		// set use type
		if( m_iState == STATE_TURN_OFF || m_iState == STATE_OFF )
			useType = USE_ON;
		else if( m_iState == STATE_TURN_ON || m_iState == STATE_ON )
			useType = USE_OFF;
	}

	if( useType == USE_ON )
	{
		// enable entity
		if( m_iState == STATE_TURN_OFF || m_iState == STATE_OFF )
		{
 			if( m_flDelay )
 			{
 				// we have time to turning on
				m_iState = STATE_TURN_ON;
				SetNextThink( m_flDelay );
			}
			else
			{
				// just enable entity
				m_iState = STATE_ON;
				UTIL_FireTargets( pev->target, pActivator, this, USE_ON, pev->scale );
				if( FBitSet( pev->spawnflags, SF_LOCAL_REMOVE_ON_FIRE ))
					UTIL_Remove( this );
				else DontThink(); // break thinking
			}
		}
	}
	else if( useType == USE_OFF )
	{
		// disable entity
		if( m_iState == STATE_TURN_ON || m_iState == STATE_ON ) // activate turning off entity
		{
 			if( m_flWait )
 			{
 				// we have time to turning off
				m_iState = STATE_TURN_OFF;
				SetNextThink( m_flWait );
			}
			else
			{
				// just enable entity
				m_iState = STATE_OFF;
				UTIL_FireTargets( pev->target, pActivator, this, USE_OFF, pev->scale );
				if( FBitSet( pev->spawnflags, SF_LOCAL_REMOVE_ON_FIRE ))
					UTIL_Remove( this );
				else DontThink(); // break thinking
			}
		}
	}
 	else if( useType == USE_SET )
 	{
 		// just set state
 		if( value != 0.0f )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		DontThink(); // break thinking
	}
}

void CEnvLocal :: Think( void )
{
	if( m_iState == STATE_TURN_ON )
	{
		m_iState = STATE_ON;
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_ON, pev->scale );
	}
	else if( m_iState == STATE_TURN_OFF )
	{
		m_iState = STATE_OFF;
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_OFF, pev->scale );
	}

	if( FBitSet( pev->spawnflags, SF_LOCAL_REMOVE_ON_FIRE ))
		UTIL_Remove( this );
	else DontThink(); // break thinking
}