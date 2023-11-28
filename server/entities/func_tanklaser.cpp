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
#include "func_tanklaser.h"

LINK_ENTITY_TO_CLASS( func_tanklaser, CFuncTankLaser );

BEGIN_DATADESC( CFuncTankLaser )
	DEFINE_FIELD( m_pLaser, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_laserTime, FIELD_TIME ),
END_DATADESC()

void CFuncTankLaser :: Activate( void )
{
	if( !GetLaser( ) )
	{
		ALERT( at_error, "Laser tank with no env_laser!\n" );
		UTIL_Remove( this );
		return;
	}
	else
	{
		m_pLaser->TurnOff();
	}

	m_bulletType = TANK_BULLET_OTHER;

	BaseClass::Activate();
}

void CFuncTankLaser :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "laserentity" ))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue( pkvd );
	}
}

CLaser *CFuncTankLaser :: GetLaser( void )
{
	if( m_pLaser )
		return m_pLaser;

	CBaseEntity *pEntity;

	pEntity = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ));

	while( pEntity )
	{
		// Found the laser
		if( FClassnameIs( pEntity->pev, "env_laser" ))
		{
			m_pLaser = (CLaser *)pEntity;
			break;
		}
		else
		{
			pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->message ));
		}
	}
	return m_pLaser;
}

void CFuncTankLaser :: Think( void )
{
	if( m_pLaser && ( gpGlobals->time > m_laserTime ))
		m_pLaser->TurnOff();

	CBaseTank::Think();
}

void CFuncTankLaser :: Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	TraceResult tr;

	if( m_fireLast != 0 && GetLaser() )
	{
		// TankTrace needs gpGlobals->v_up, etc.
		UTIL_MakeAimVectors( GetAbsAngles( ));

		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		if( bulletCount )
		{
			for( int i = 0; i < bulletCount; i++ )
			{
				m_pLaser->SetAbsOrigin( barrelEnd );
				TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );
				
				m_laserTime = gpGlobals->time;
				m_pLaser->TurnOn();
				m_pLaser->pev->dmgtime = gpGlobals->time - 1.0f;
				m_pLaser->FireAtPoint( barrelEnd, tr );
				m_pLaser->DontThink();
			}

			BaseClass::Fire( barrelEnd, forward, pev );
		}
	}
	else
	{
		BaseClass::Fire( barrelEnd, forward, pev );
	}
}
