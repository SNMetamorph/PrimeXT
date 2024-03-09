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
***/

#include "trigger_changetarget.h"

LINK_ENTITY_TO_CLASS( trigger_changetarget, CTriggerChangeTarget );

BEGIN_DATADESC( CTriggerChangeTarget )
	DEFINE_KEYFIELD( m_iszNewTarget, FIELD_STRING, "m_iszNewTarget" ),
END_DATADESC()

void CTriggerChangeTarget::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iszNewTarget"))
	{
		m_iszNewTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ), pActivator );

	if( pTarget )
	{
		if( FStrEq( STRING( m_iszNewTarget ), "*locus" ))
		{
			if( pActivator )
				pTarget->pev->target = pActivator->pev->targetname;
			else ALERT( at_error, "trigger_changetarget \"%s\" requires a locus!\n", STRING( pev->targetname ));
		}
		else
		{
			pTarget->pev->target = m_iszNewTarget;
		}

		CBaseMonster *pMonster = pTarget->MyMonsterPointer( );
		if( pMonster )
		{
			pMonster->m_pGoalEnt = NULL;
		}

		if( pTarget->IsFuncScreen( ) )
			pTarget->Activate(); // HACKHACK update portal camera
	}
}
