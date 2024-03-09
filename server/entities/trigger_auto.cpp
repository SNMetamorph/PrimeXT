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

#include "trigger_auto.h"

LINK_ENTITY_TO_CLASS( trigger_auto, CAutoTrigger );

BEGIN_DATADESC( CAutoTrigger )
	DEFINE_KEYFIELD( m_globalstate, FIELD_STRING, "globalstate" ),
	DEFINE_FIELD( triggerType, FIELD_INTEGER ),
END_DATADESC()

void CAutoTrigger::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "globalstate"))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = atoi( pkvd->szValue );
		switch( type )
		{
		case 0:
			triggerType = (USE_TYPE)-1;	// will be changed in spawn
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}


void CAutoTrigger::Spawn( void )
{
	if( triggerType == -1 )
		triggerType = USE_OFF;	// "triggerstate" is present and set to 'OFF'
	else if( !triggerType )
		triggerType = USE_ON;	// "triggerstate" is missing - defaulting to 'ON'

	// HACKHACK: run circles on a final map
	if( FStrEq( STRING( gpGlobals->mapname ), "c5a1" ) && FStrEq( GetTarget(), "hoop_1" ))
		triggerType = USE_ON;
#if 0
	// don't confuse level-designers with firing after loading savedgame
	if( m_globalstate == NULL_STRING )
		SetBits( pev->spawnflags, SF_AUTO_FIREONCE );
#endif
	Precache();
}

void CAutoTrigger::Precache( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
}

void CAutoTrigger::Think( void )
{
	if ( !m_globalstate || gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
	{
		SUB_UseTargets( this, triggerType, 0 );
		if ( pev->spawnflags & SF_AUTO_FIREONCE )
			UTIL_Remove( this );
	}
}
