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

#include "inout_register.h"
#include "trigger_inout.h"

LINK_ENTITY_TO_CLASS( inout_register, CInOutRegister );

BEGIN_DATADESC( CInOutRegister )
	DEFINE_FIELD( m_pField, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pNext, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_hValue, FIELD_EHANDLE ),
END_DATADESC()

BOOL CInOutRegister :: IsRegistered ( CBaseEntity *pValue )
{
	if (m_hValue == pValue)
		return TRUE;
	else if (m_pNext)
		return m_pNext->IsRegistered( pValue );
	else
		return FALSE;
}

CInOutRegister *CInOutRegister::Add( CBaseEntity *pValue )
{
	if( m_hValue == pValue )
	{
		// it's already in the list, don't need to do anything
		return this;
	}
	else if( m_pNext )
	{
		// keep looking
		m_pNext = m_pNext->Add( pValue );
		return this;
	}
	else
	{
		// reached the end of the list; add the new entry, and trigger
		CInOutRegister *pResult = GetClassPtr( (CInOutRegister*)NULL );
		pResult->m_hValue = pValue;
		pResult->m_pNext = this;
		pResult->m_pField = m_pField;
		pResult->pev->classname = MAKE_STRING("inout_register");

		m_pField->FireOnEntry( pValue );
		return pResult;
	}
}

CInOutRegister *CInOutRegister::Prune( void )
{
	if ( m_hValue )
	{
		ASSERTSZ(m_pNext != NULL, "invalid InOut registry terminator\n");
		if ( m_pField->TriggerIntersects( m_hValue ) && !FBitSet( m_pField->pev->flags, FL_KILLME ))
		{
			// this entity is still inside the field, do nothing
			m_pNext = m_pNext->Prune();
			return this;
		}
		else
		{
			// this entity has just left the field, trigger
			m_pField->FireOnLeaving( m_hValue );
			SetThink( &CInOutRegister:: SUB_Remove );
			SetNextThink( 0.1 );
			return m_pNext->Prune();
		}
	}
	else
	{	// this register has a missing or null value
		if( m_pNext )
		{
			// this is an invalid list entry, remove it
			SetThink( &CInOutRegister:: SUB_Remove );
			SetNextThink( 0.1 );
			return m_pNext->Prune();
		}
		else
		{
			// this is the list terminator, leave it.
			return this;
		}
	}
}
