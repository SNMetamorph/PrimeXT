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

#include "plats.h"

BEGIN_DATADESC( CBasePlatTrain )
	DEFINE_KEYFIELD( m_iMoveSnd, FIELD_STRING, "movesnd" ),
	DEFINE_KEYFIELD( m_iStopSnd, FIELD_STRING, "stopsnd" ),
	DEFINE_KEYFIELD( m_volume, FIELD_FLOAT, "volume" ),
	DEFINE_FIELD( m_flFloor, FIELD_FLOAT ),
END_DATADESC()

void CBasePlatTrain :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "lip" ))
	{
		m_flLip = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "wait" ))
	{
		m_flWait = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "height" ))
	{
		m_flHeight = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "width" ))
	{
		m_flWidth = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "rotation" ))
	{
		m_vecFinalAngle.x = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "movesnd" ))
	{
		m_iMoveSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "stopsnd" ))
	{
		m_iStopSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "volume" ))
	{
		m_volume = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CBasePlatTrain :: Precache( void )
{
	int m_sound = UTIL_LoadSoundPreset( m_iMoveSnd );

	// set the plat's "in-motion" sound
	switch( m_sound )
	{
	case 0:
		pev->noise = UTIL_PrecacheSound( "common/null.wav" );
		break;
	case 1:
		pev->noise = UTIL_PrecacheSound( "plats/bigmove1.wav" );
		break;
	case 2:
		pev->noise = UTIL_PrecacheSound( "plats/bigmove2.wav" );
		break;
	case 3:
		pev->noise = UTIL_PrecacheSound( "plats/elevmove1.wav" );
		break;
	case 4:
		pev->noise = UTIL_PrecacheSound( "plats/elevmove2.wav" );
		break;
	case 5:
		pev->noise = UTIL_PrecacheSound( "plats/elevmove3.wav" );
		break;
	case 6:
		pev->noise = UTIL_PrecacheSound( "plats/freightmove1.wav" );
		break;
	case 7:
		pev->noise = UTIL_PrecacheSound( "plats/freightmove2.wav" );
		break;
	case 8:
		pev->noise = UTIL_PrecacheSound( "plats/heavymove1.wav" );
		break;
	case 9:
		pev->noise = UTIL_PrecacheSound( "plats/rackmove1.wav" );
		break;
	case 10:
		pev->noise = UTIL_PrecacheSound( "plats/railmove1.wav" );
		break;
	case 11:
		pev->noise = UTIL_PrecacheSound( "plats/squeekmove1.wav" );
		break;
	case 12:
		pev->noise = UTIL_PrecacheSound( "plats/talkmove1.wav" );
		break;
	case 13:
		pev->noise = UTIL_PrecacheSound( "plats/talkmove2.wav" );
		break;
	default:
		pev->noise = UTIL_PrecacheSound( m_sound );
		break;
	}

	m_sound = UTIL_LoadSoundPreset( m_iStopSnd );

	// set the plat's 'reached destination' stop sound
	switch( m_sound )
	{
	case 0:
		pev->noise1 = UTIL_PrecacheSound( "common/null.wav" );
		break;
	case 1:
		pev->noise1 = UTIL_PrecacheSound( "plats/bigstop1.wav" );
		break;
	case 2:
		pev->noise1 = UTIL_PrecacheSound( "plats/bigstop2.wav" );
		break;
	case 3:
		pev->noise1 = UTIL_PrecacheSound( "plats/freightstop1.wav" );
		break;
	case 4:
		pev->noise1 = UTIL_PrecacheSound( "plats/heavystop2.wav" );
		break;
	case 5:
		pev->noise1 = UTIL_PrecacheSound( "plats/rackstop1.wav" );
		break;
	case 6:
		pev->noise1 = UTIL_PrecacheSound( "plats/railstop1.wav" );
		break;
	case 7:
		pev->noise1 = UTIL_PrecacheSound( "plats/squeekstop1.wav" );
		break;
	case 8:
		pev->noise1 = UTIL_PrecacheSound( "plats/talkstop1.wav" );
		break;
	default:
		pev->noise1 = UTIL_PrecacheSound( m_sound );
		break;
	}
}
