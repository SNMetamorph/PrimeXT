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

#include "trigger_inout.h"

LINK_ENTITY_TO_CLASS( trigger_inout, CTriggerInOut );

BEGIN_DATADESC( CTriggerInOut )
	DEFINE_KEYFIELD( m_iszAltTarget, FIELD_STRING, "m_iszAltTarget" ),
	DEFINE_KEYFIELD( m_iszBothTarget, FIELD_STRING, "m_iszBothTarget" ),
	DEFINE_FIELD( m_pRegister, FIELD_CLASSPTR ),
END_DATADESC()

void CTriggerInOut::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iszAltTarget" ))
	{
		m_iszAltTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszBothTarget" ))
	{
		m_iszBothTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

int CTriggerInOut :: Restore( CRestore &restore )
{
	CInOutRegister *pRegister;

	if( restore.IsGlobalMode() )
	{
		// we already have the valid chain.
		// Don't break it with bad pointers from previous level
		pRegister = m_pRegister;
	}

	int status = BaseClass::Restore(restore);
	if ( !status )
		return 0;

	if( restore.IsGlobalMode() )
	{
		// restore our chian here
		m_pRegister = pRegister;
	}

	return status;
}

void CTriggerInOut :: Spawn( void )
{
	InitTrigger();
	// create a null-terminator for the registry
	m_pRegister = GetClassPtr( (CInOutRegister*)NULL );
	m_pRegister->m_hValue = NULL;
	m_pRegister->m_pNext = NULL;
	m_pRegister->m_pField = this;
	m_pRegister->pev->classname = MAKE_STRING("inout_register");
}

void CTriggerInOut :: Touch( CBaseEntity *pOther )
{
	if( !CanTouch( pOther ))
		return;

	m_pRegister = m_pRegister->Add( pOther );

	if( pev->nextthink <= 0.0f && !m_pRegister->IsEmpty( ))
		SetNextThink( 0.05 );
}

void CTriggerInOut :: Think( void )
{
	// Prune handles all Intersects tests and fires targets as appropriate
	m_pRegister = m_pRegister->Prune();

	if (m_pRegister->IsEmpty())
		DontThink();
	else
		SetNextThink( 0.05 );
}

void CTriggerInOut :: FireOnEntry( CBaseEntity *pOther )
{
	if( !IsLockedByMaster( pOther ))
	{
//Msg( "FireOnEntry( %s )\n", STRING( pev->target ));
		UTIL_FireTargets( m_iszBothTarget, pOther, this, USE_ON, 0 );
		UTIL_FireTargets( pev->target, pOther, this, USE_TOGGLE, 0 );
	}
}

void CTriggerInOut :: FireOnLeaving( CBaseEntity *pOther )
{
	if( !IsLockedByMaster( pOther ))
	{
//Msg( "FireOnLeaving( %s )\n", STRING( m_iszAltTarget ));
		UTIL_FireTargets( m_iszBothTarget, pOther, this, USE_OFF, 0 );
		UTIL_FireTargets( m_iszAltTarget, pOther, this, USE_TOGGLE, 0 );
	}
}

void CTriggerInOut :: OnRemove( void )
{
	if( !m_pRegister ) return; // e.g. moved from another level

	// Prune handles all Intersects tests and fires targets as appropriate
	m_pRegister = m_pRegister->Prune();
}
