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
	DEFINE_FIELD( m_hRegister, FIELD_EHANDLE ),
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
	EHANDLE pRegister;

	if( restore.IsGlobalMode() )
	{
		// we already have the valid chain.
		// Don't break it with bad pointers from previous level
		pRegister = m_hRegister;
	}

	int status = BaseClass::Restore(restore);
	if ( !status )
		return 0;

	if( restore.IsGlobalMode() )
	{
		// restore our chian here
		m_hRegister = pRegister;
	}

	return status;
}

void CTriggerInOut :: Spawn( void )
{
	InitTrigger();
	// create a null-terminator for the registry
	m_hRegister = GetClassPtr( (CInOutRegister*)NULL );
	CInOutRegister *pRegister = dynamic_cast<CInOutRegister*>(m_hRegister.GetPointer());
	pRegister->m_hValue = NULL;
	pRegister->m_pNext = NULL;
	pRegister->m_pField = this;
	pRegister->pev->classname = MAKE_STRING("inout_register");
}

void CTriggerInOut :: Touch( CBaseEntity *pOther )
{
	if( !CanTouch( pOther ))
		return;

	CInOutRegister *pRegister = dynamic_cast<CInOutRegister*>(m_hRegister.GetPointer());
	pRegister = pRegister->Add( pOther );

	if( pev->nextthink <= 0.0f && !pRegister->IsEmpty( ))
		SetNextThink( 0.05 );
}

void CTriggerInOut :: Think( void )
{
	// Prune handles all Intersects tests and fires targets as appropriate
	CInOutRegister *pRegister = dynamic_cast<CInOutRegister*>(m_hRegister.GetPointer());
	m_hRegister = pRegister->Prune();

	if (pRegister->IsEmpty())
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
	if( !m_hRegister ) 
		return; // e.g. moved from another level

	// Prune handles all Intersects tests and fires targets as appropriate
	CInOutRegister *pRegister = dynamic_cast<CInOutRegister*>(m_hRegister.GetPointer());
	m_hRegister = pRegister->Prune();
}

STATE CTriggerInOut :: GetState() 
{
	CInOutRegister *pRegister = dynamic_cast<CInOutRegister*>(m_hRegister.GetPointer());
	return pRegister->IsEmpty() ? STATE_OFF : STATE_ON; 
}
