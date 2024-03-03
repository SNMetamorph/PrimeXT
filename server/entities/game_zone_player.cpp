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

#include "game_zone_player.h"

LINK_ENTITY_TO_CLASS( game_zone_player, CGamePlayerZone );

BEGIN_DATADESC( CGamePlayerZone )
	DEFINE_KEYFIELD( m_iszInTarget, FIELD_STRING, "intarget" ),
	DEFINE_KEYFIELD( m_iszOutTarget, FIELD_STRING, "outtarget" ),
	DEFINE_KEYFIELD( m_iszInCount, FIELD_STRING, "incount" ),
	DEFINE_KEYFIELD( m_iszOutCount, FIELD_STRING, "outcount" ),
END_DATADESC()

void CGamePlayerZone::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "intarget"))
	{
		m_iszInTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "outtarget"))
	{
		m_iszOutTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "incount"))
	{
		m_iszInCount = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "outcount"))
	{
		m_iszOutCount = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CRuleBrushEntity::KeyValue( pkvd );
}

void CGamePlayerZone::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int playersInCount = 0;
	int playersOutCount = 0;

	if ( !CanFireForActivator( pActivator ) )
		return;

	CBaseEntity *pPlayer = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			TraceResult trace;
			int			hullNumber;

			hullNumber = human_hull;
			if ( pPlayer->pev->flags & FL_DUCKING )
			{
				hullNumber = head_hull;
			}

			UTIL_TraceModel( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(), hullNumber, edict(), &trace );

			if ( trace.fStartSolid )
			{
				playersInCount++;
				if ( m_iszInTarget )
				{
					UTIL_FireTargets( STRING(m_iszInTarget), pPlayer, pActivator, useType, value );
				}
			}
			else
			{
				playersOutCount++;
				if ( m_iszOutTarget )
				{
					UTIL_FireTargets( STRING(m_iszOutTarget), pPlayer, pActivator, useType, value );
				}
			}
		}
	}

	if ( m_iszInCount )
	{
		UTIL_FireTargets( STRING(m_iszInCount), pActivator, this, USE_SET, playersInCount );
	}

	if ( m_iszOutCount )
	{
		UTIL_FireTargets( STRING(m_iszOutCount), pActivator, this, USE_SET, playersOutCount );
	}
}
