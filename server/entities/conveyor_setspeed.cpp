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

#include "conveyor_setspeed.h"

LINK_ENTITY_TO_CLASS( conveyor_setspeed, CConveyorSetSpeed );

// Global Savedata for train speed modifier
BEGIN_DATADESC( CConveyorSetSpeed )
	DEFINE_KEYFIELD( m_flTime, FIELD_FLOAT, "time" ),
	DEFINE_FUNCTION( UpdateSpeed ),
END_DATADESC()

// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CConveyorSetSpeed :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "time" ))
	{
		m_flTime = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CConveyorSetSpeed :: Spawn( void )
{
	m_iState = STATE_OFF;
	m_hActivator = NULL;
	pev->speed = bound( -10000.0f, pev->speed, 10000.0f );
}

BOOL CConveyorSetSpeed :: EvaluateSpeed( BOOL bStartup )
{
	CBaseEntity *pEntity = NULL;
	int affected_conveyors = 0;
	int finished_conveyors = 0;

	while(( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->target ), m_hActivator )) != NULL )
	{
		if( !FClassnameIs( pEntity->pev, "func_transporter" ) )
			continue;

		if( bStartup )
		{
			pEntity->pev->frags = pev->speed - pEntity->pev->speed;
			pEntity->pev->dmg_inflictor = edict();
			continue;
		}

		if( m_flTime > 0 && pEntity->pev->dmg_inflictor != edict( ))
			continue;	// owned by another set speed

		if( fabs( pev->speed - pEntity->pev->speed ) <= 1.0f || m_flTime <= 0 )
		{
			if( FBitSet( pev->spawnflags, SF_CONVSPEED_DEBUG ) && pev->speed != pEntity->pev->speed )
				ALERT( at_console, "conveyor_setspeed: %s target speed %g, curspeed %g\n", GetTargetname(), pev->speed, pEntity->pev->speed );
			pEntity->Use( this, this, USE_SET, pev->speed );
			pEntity->pev->dmg_inflictor = NULL; // done
			finished_conveyors++;
		}
		else
		{
			float flInterval = pEntity->pev->frags / ( m_flTime / 0.1 ); // nextthink 0.1
			pEntity->Use( this, this, USE_SET, pEntity->pev->speed + flInterval );
			if( FBitSet( pev->spawnflags, SF_CONVSPEED_DEBUG ))
				ALERT( at_console, "conveyor_setspeed: %s target speed %g, curspeed %g\n", GetTargetname(), pev->speed, pEntity->pev->speed );
		}
		affected_conveyors++;
	}

	return (affected_conveyors != finished_conveyors);
}

void CConveyorSetSpeed :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( m_iState == STATE_ON )
		return;	// entity in work

	m_hActivator = pActivator;

	if( m_flTime > 0 )
	{
		EvaluateSpeed( TRUE );
		SetThink( &CConveyorSetSpeed :: UpdateSpeed );
		m_iState = STATE_ON;
		SetNextThink( 0.1 );
	}
	else
	{
		EvaluateSpeed( FALSE );
	}
}

void CConveyorSetSpeed :: UpdateSpeed( void )
{
	if( !EvaluateSpeed( FALSE ))
	{
		if( FBitSet( pev->spawnflags, SF_CONVSPEED_DEBUG ))
			ALERT( at_console, "conveyor_setspeed: %s is done\n", GetTargetname( ));
		m_iState = STATE_OFF;
		return; // finished
          }

	SetNextThink( 0.1 );	
}
