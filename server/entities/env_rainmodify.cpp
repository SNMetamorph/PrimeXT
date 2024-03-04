/*
env_rainmodify.h - rain VFX implementation from Paranoia 1
Copyright (C) 2004 BUzer

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "env_rainmodify.h"
#include "player.h"
#include "gamerules.h"

#define SF_RAIN_CONSTANT	BIT( 0 )

LINK_ENTITY_TO_CLASS( env_rainmodify, CEnvRainModify );

void CEnvRainModify :: Spawn( void )
{
	if( g_pGameRules->IsMultiplayer() || FStringNull( pev->targetname ))
		SetBits( pev->spawnflags, SF_RAIN_CONSTANT );
}

void CEnvRainModify :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iDripsPerSecond" ))
	{
		pev->impulse = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flWindX" ))
	{
		pev->fuser1 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flWindY" ))
	{
		pev->fuser2 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flRandX" ))
	{
		pev->fuser3 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flRandY" ))
	{
		pev->fuser4 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flTime" ))
	{
		pev->dmg = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	
	{
		BaseClass::KeyValue( pkvd );
	}
}

void CEnvRainModify :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( FBitSet( pev->spawnflags, SF_RAIN_CONSTANT ))
		return; // constant

	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(1);

	if( pev->dmg )
	{
		// write to 'ideal' settings
		pPlayer->m_iRainIdealDripsPerSecond = pev->impulse;
		pPlayer->m_flRainIdealRandX = pev->fuser3;
		pPlayer->m_flRainIdealRandY = pev->fuser4;
		pPlayer->m_flRainIdealWindX = pev->fuser1;
		pPlayer->m_flRainIdealWindY = pev->fuser2;

		pPlayer->m_flRainEndFade = gpGlobals->time + pev->dmg;
		pPlayer->m_flRainNextFadeUpdate = gpGlobals->time + 1.0f;
	}
	else
	{
		pPlayer->m_iRainDripsPerSecond = pev->impulse;
		pPlayer->m_flRainRandX = pev->fuser3;
		pPlayer->m_flRainRandY = pev->fuser4;
		pPlayer->m_flRainWindX = pev->fuser1;
		pPlayer->m_flRainWindY = pev->fuser2;

		pPlayer->m_bRainNeedsUpdate = true;
	}
}
