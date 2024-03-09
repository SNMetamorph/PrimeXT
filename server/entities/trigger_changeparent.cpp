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

#include "trigger_changeparent.h"

LINK_ENTITY_TO_CLASS( trigger_changeparent, CTriggerChangeParent );

BEGIN_DATADESC( CTriggerChangeParent )
	DEFINE_KEYFIELD( m_iszNewParent, FIELD_STRING, "m_iszNewParent" ),
END_DATADESC()

void CTriggerChangeParent :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iszNewParent" ))
	{
		m_iszNewParent = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeParent :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ), pActivator );

	if( pTarget )
	{
		if( FStrEq( STRING( m_iszNewParent ), "*locus" ))
		{
			if( pActivator )
				pTarget->SetParent( pActivator );
			else ALERT( at_error, "trigger_changeparent \"%s\" requires a locus!\n", STRING( pev->targetname ));
		}
		else
		{
			if( m_iszNewParent != NULL_STRING )
				pTarget->SetParent( m_iszNewParent );
			else pTarget->SetParent( 0 );
		}
	}
}
