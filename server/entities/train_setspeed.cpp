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

#include "train_setspeed.h"

LINK_ENTITY_TO_CLASS( train_setspeed, CTrainSetSpeed );

// Global Savedata for train speed modifier
BEGIN_DATADESC( CTrainSetSpeed )
	DEFINE_KEYFIELD( m_iMode, FIELD_CHARACTER, "mode" ),
	DEFINE_KEYFIELD( m_flTime, FIELD_FLOAT, "time" ),
	DEFINE_FIELD( m_flInterval, FIELD_FLOAT ),
	DEFINE_FIELD( m_pTrain, FIELD_CLASSPTR ),
	DEFINE_FUNCTION( UpdateSpeed ),
	DEFINE_FUNCTION( Find ),
END_DATADESC()

// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CTrainSetSpeed :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "time" ))
	{
		m_flTime = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "mode" ))
	{
		m_iMode = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "train" ))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CTrainSetSpeed :: Spawn( void )
{
	if( m_flTime <= 0 ) m_flTime = 10.0f;

	m_iState = STATE_OFF;
	m_hActivator = NULL;
//pev->spawnflags |= SF_TRAINSPEED_DEBUG;
	if( !FStrEq( STRING( pev->netname ), "*locus" ))
	{
		SetThink( &CTrainSetSpeed :: Find );
		SetNextThink( 0.1 );
	}
}

void CTrainSetSpeed :: Find( void )
{
	CBaseEntity *pEntity = NULL;

	while(( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->netname ), m_hActivator )) != NULL )
	{
		// found the tracktrain
		if( FClassnameIs( pEntity->pev, "func_tracktrain" ) )
		{
//			ALERT( at_console, "Found tracktrain: %s\n", STRING( pEntity->pev->targetname ));
			m_pTrain = (CFuncTrackTrain *)pEntity;
			break;
		}
	}
}

void CTrainSetSpeed :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( m_iState == STATE_ON )
		return;	// entity in work

	m_hActivator = pActivator;

	if( FStrEq( STRING( pev->netname ), "*locus" ) || !m_pTrain )
		Find();

	if( !m_pTrain ) return; // couldn't find train

	if( m_pTrain->m_pSpeedControl )
	{
		// cancelling previous controller...
		CTrainSetSpeed *pSpeedControl = (CTrainSetSpeed *)m_pTrain->m_pSpeedControl;
		pSpeedControl->m_iState = STATE_OFF;
		m_pTrain->m_pSpeedControl = NULL;
		pSpeedControl->DontThink();
	}

	if( !m_pTrain->GetSpeed( ))
	{
		if( !FBitSet( pev->spawnflags, SF_ACTIVATE_TRAIN ))
			return;

		// activate train before set speed
		if( FBitSet( pev->spawnflags, SF_TRAINSPEED_DEBUG ))
			ALERT( at_console, "train is activated\n" );
	}

	float m_flDelta;

	switch( m_iMode )
	{
	case 1:
		m_flDelta = ( pev->speed - m_pTrain->GetSpeed( ));
		m_flInterval = m_flDelta / (m_flTime / 0.1 ); // nextthink 0.1
		SetThink( &CTrainSetSpeed :: UpdateSpeed );
		m_pTrain->m_pSpeedControl = this; // now i'm control the train
		m_iState = STATE_ON; // i'm changing the train speed
		SetNextThink( 0.1 );
		break;
	case 0:
		m_pTrain->SetSpeedExternal( pev->speed );
		m_pTrain->m_pSpeedControl = NULL;
		break;
	}
}

void CTrainSetSpeed :: UpdateSpeed( void )
{
	if( !m_pTrain )
	{
		m_iState = STATE_OFF;
		return;	// no train or train not moving
          }

	if( FBitSet( pev->spawnflags, SF_TRAINSPEED_DEBUG ))
		ALERT( at_console, "train_setspeed: %s target speed %g, curspeed %g, step %g\n", GetTargetname(), pev->speed, m_pTrain->GetSpeed( ), m_flInterval );

	if( fabs( m_pTrain->GetSpeed() - pev->speed ) <= fabs( m_flInterval ))
	{
		if( pev->speed == 0.0f && FBitSet( pev->spawnflags, SF_ACTIVATE_TRAIN ))
		{
			if( FBitSet( pev->spawnflags, SF_TRAINSPEED_DEBUG ))
				ALERT( at_console, "train is stopped\n" );
			m_pTrain->SetSpeedExternal( 0 );
		}
		else if( pev->speed != 0.0f )
		{
			if( FBitSet( pev->spawnflags, SF_TRAINSPEED_DEBUG ))
				ALERT( at_console, "train is reached target speed: %g\n", pev->speed );
			m_pTrain->SetSpeedExternal( pev->speed );
		}

		m_iState = STATE_OFF;
		m_pTrain->m_pSpeedControl = NULL;
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
		return; // reached
	}

	m_pTrain->SetSpeedExternal( m_pTrain->GetSpeed() + m_flInterval );

	if( m_pTrain->GetSpeed() >= 2000 )
	{
		m_iState = STATE_OFF;
		m_pTrain->m_pSpeedControl = NULL;
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
		m_pTrain->SetSpeedExternal( 2000 );
		return;
	}

	if( m_pTrain->GetSpeed() <= -2000 )
	{
		m_iState = STATE_OFF;
		m_pTrain->m_pSpeedControl = NULL;
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
		m_pTrain->SetSpeedExternal( -2000 );
		return;
	}

	SetNextThink( 0.1 );	
}
