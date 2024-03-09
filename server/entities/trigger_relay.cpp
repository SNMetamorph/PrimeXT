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

#include "trigger_relay.h"

LINK_ENTITY_TO_CLASS( trigger_relay, CTriggerRelay );

BEGIN_DATADESC( CTriggerRelay )
	DEFINE_FIELD( triggerType, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iszAltTarget, FIELD_STRING, "m_iszAltTarget" ),
END_DATADESC()

void CTriggerRelay::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "triggerstate" ))
	{
		int type = Q_atoi( pkvd->szValue );
		switch( type )
		{
		case 0:
			triggerType = (USE_TYPE)-1;	// will be changed in spawn
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		case 3:
			triggerType = USE_SET;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "value" ))
	{
		pev->scale = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszAltTarget" ))
	{
		m_iszAltTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CTriggerRelay::Spawn( void )
{
	if( triggerType == -1 )
		triggerType = USE_OFF;	// "triggerstate" is present and set to 'OFF'
	else if( !triggerType )
		triggerType = USE_ON;	// "triggerstate" is missing - defaulting to 'ON'

	// HACKHACK: allow Hihilanth working on a c4a3
	if( FStrEq( STRING( gpGlobals->mapname ), "c4a3" ) && FStrEq( GetTargetname(), "n_end_relay" ))
		triggerType = USE_OFF;		
}

void CTriggerRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
	{
		if( m_iszAltTarget )
			SUB_UseTargets( this, triggerType, (pev->scale != 0.0f) ? pev->scale : value, m_iszAltTarget );

		if( FBitSet( pev->spawnflags, SF_RELAY_FIREONCE ))
			UTIL_Remove( this );
		return;
	}

	SUB_UseTargets( this, triggerType, (pev->scale != 0.0f) ? pev->scale : value );

	if( FBitSet( pev->spawnflags, SF_RELAY_FIREONCE ))
		UTIL_Remove( this );
}
