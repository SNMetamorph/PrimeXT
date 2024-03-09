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

#include "trigger_dsp_zone.h"
#include "player.h"

LINK_ENTITY_TO_CLASS( trigger_dsp_zone, CTriggerDSPZone );

void CTriggerDSPZone :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "roomtype"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

void CTriggerDSPZone :: FireOnEntry( CBaseEntity *pOther )
{
	// Only save on clients
	if( !pOther->IsPlayer( )) return;
	pev->button = ((CBasePlayer *)pOther)->m_iSndRoomtype;
	((CBasePlayer *)pOther)->m_iSndRoomtype = pev->impulse;
}

void CTriggerDSPZone :: FireOnLeaving( CBaseEntity *pOther )
{
	if( !pOther->IsPlayer( )) return;
	((CBasePlayer *)pOther)->m_iSndRoomtype = pev->button;
}
