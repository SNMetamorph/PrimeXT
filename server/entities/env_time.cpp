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

#include "env_time.h"

LINK_ENTITY_TO_CLASS( env_time, CEnvTime );

void CEnvTime :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "time" ))
	{
		UTIL_StringToVector( (float*)(m_vecTime), pkvd->szValue );
		pkvd->fHandled = TRUE;
		pev->button = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CEnvTime :: Spawn( void )
{
	if( !pev->scale )
		pev->scale = 1.0f;	// default time scale

	if( !pev->button ) return;

	// first initialize?
	m_flLevelTime = bound( 0.0f, m_vecTime.x, 24.0f );
	m_flLevelTime += bound( 0.0f, m_vecTime.y, 60.0f ) * (1.0f/60.0f);
	m_flLevelTime += bound( 0.0f, m_vecTime.z, 60.0f ) * (1.0f/3600.0f);
	int name = MAKE_STRING( "GLOBAL_TIME" );

	if( !gGlobalState.EntityInTable( name )) // first initialize ?
		gGlobalState.EntityAdd( name, gpGlobals->mapname, (GLOBALESTATE)GLOBAL_ON, m_flLevelTime );
}

void CEnvTime :: Activate( void )
{
	// initialize level-time here
	m_flLevelTime = gGlobalState.EntityGetTime( MAKE_STRING( "GLOBAL_TIME" ));

	if( m_flLevelTime != -1.0f )
		SetNextThink( 0.0f );
}

void CEnvTime :: Think( void )
{
	if( m_flLevelTime == -1.0f )
		return; // no time specified

	m_flLevelTime += pev->scale * (1.0f/3600.0f); // evaluate level time
	if( m_flLevelTime > 24.0f ) m_flLevelTime = 0.001f; // A little hack to prevent disable the env_counter 

	// update clocks
	UTIL_FireTargets( pev->target, this, this, USE_SET, m_flLevelTime * 60.0f );	// encoded hours & minutes out
	UTIL_FireTargets( pev->netname, this, this, USE_SET, m_flLevelTime * 3600.0f );	// encoded hours & minutes & seconds out

	gGlobalState.EntitySetTime( MAKE_STRING( "GLOBAL_TIME" ), m_flLevelTime );
	SetNextThink( 1.0f ); // tick every second
}