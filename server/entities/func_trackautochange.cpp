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

#include "func_trackautochange.h"

LINK_ENTITY_TO_CLASS( func_trackautochange, CFuncTrackAuto );

// Auto track change
void CFuncTrackAuto :: UpdateAutoTargets( int toggleState )
{
	// g-cont. update targets same as func_door
	if( toggleState == TS_AT_BOTTOM )
	{
		if( !FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
			UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
		else
			UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}
	else if( toggleState == TS_AT_TOP )
	{
		if( FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
			UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
		else
			UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}

	if( !m_trackTop || !m_trackBottom )
		return;

	CPathTrack *pTarget, *pNextTarget;

	if( m_targetState == TS_AT_TOP )
	{
		pTarget = m_trackTop->GetNext();
		pNextTarget = m_trackBottom->GetNext();
	}
	else
	{
		pTarget = m_trackBottom->GetNext();
		pNextTarget = m_trackTop->GetNext();
	}

	if( pTarget )
	{
		ClearBits( pTarget->pev->spawnflags, SF_PATH_DISABLED );

		if( m_code == TRAIN_FOLLOWING && m_train && !m_train->GetSpeed() )
			m_train->Use( this, this, USE_ON, 0 );
	}

	if( pNextTarget )
		SetBits( pNextTarget->pev->spawnflags, SF_PATH_DISABLED );

}

void CFuncTrackAuto :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CPathTrack *pTarget;

	if( !UseEnabled( ))
		return;

	if( m_toggle_state == TS_AT_TOP )
		pTarget = m_trackTop;
	else if( m_toggle_state == TS_AT_BOTTOM )
		pTarget = m_trackBottom;
	else
		pTarget = NULL;

	m_hActivator = pActivator;

	if( FStringNull( m_trainName ))
		m_train = NULL; // clearing for each call

	if( FClassnameIs( pActivator, "func_tracktrain" ))
	{
		// g-cont. func_trackautochange doesn't search train in specified radius. It will be waiting train as activator
		if( !m_train ) m_train = (CFuncTrackTrain *)pActivator;

		m_code = EvaluateTrain( pTarget );

		// safe to fire?
		if( m_code == TRAIN_FOLLOWING && m_toggle_state != m_targetState )
		{
			DisableUse();
			if( m_toggle_state == TS_AT_TOP )
				GoDown();
			else
				GoUp();
		}
	}
	else if( m_train != NULL )
	{
		if( pTarget )
			pTarget = pTarget->GetNext();

		if( pTarget && m_train->m_ppath != pTarget && ShouldToggle( useType, m_targetState ))
		{
			if( m_targetState == TS_AT_TOP )
				m_targetState = TS_AT_BOTTOM;
			else
				m_targetState = TS_AT_TOP;
		}

		UpdateAutoTargets( m_targetState );
	}
}
