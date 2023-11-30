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
#include "func_tankcontrols.h"
#include "base_tank.h"

LINK_ENTITY_TO_CLASS( func_tankcontrols, CFuncTankControls );

BEGIN_DATADESC( CFuncTankControls )
	DEFINE_FIELD( m_pController, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_vecControllerUsePos, FIELD_VECTOR ),
	DEFINE_AUTO_ARRAY( m_iTankName, FIELD_STRING ),
	DEFINE_FIELD( m_cTanks, FIELD_INTEGER ),
	DEFINE_FIELD( m_fVerifyTanks, FIELD_BOOLEAN ),
END_DATADESC()

void CFuncTankControls :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;

	SET_MODEL( edict(), GetModel() );
	UTIL_SetSize( pev, pev->mins, pev->maxs );

	// store pev->target for backward compatibility
	if( m_cTanks < MAX_CONTROLLED_TANKS && !FStringNull( pev->target ))
	{
		m_iTankName[m_cTanks] = pev->target;
		m_cTanks++;
	}

	SetLocalAngles( g_vecZero );
	SetBits( m_iFlags, MF_TRIGGER );
}

void CFuncTankControls :: KeyValue( KeyValueData *pkvd )
{
	// get support for spirit field too
	if( FStrEq( pkvd->szKeyName, "parent" ) || FStrEq( pkvd->szKeyName, "movewith" ))
	{
		m_iParent = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( m_cTanks < MAX_CONTROLLED_TANKS ) 
	{
		// add this field to the target list
		// this assumes that additional fields are targetnames and their values are delay values.
		char tmp[128];
		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iTankName[m_cTanks] = ALLOC_STRING( tmp );
		m_cTanks++;
		pkvd->fHandled = TRUE;
	}
}

BOOL CFuncTankControls :: OnControls( CBaseEntity *pTest )
{
	if( m_hParent != NULL && FClassnameIs( m_hParent->pev, "func_tracktrain" ))
	{
		// transform local controller pos through tankcontrols pos
		Vector vecTransformedControlerUsePos = EntityToWorldTransform().VectorTransform( m_vecControllerUsePos );
		if(( vecTransformedControlerUsePos - pTest->GetAbsOrigin() ).Length() < 30 )
		{
			return TRUE;
		}
	}
	else if(( m_vecControllerUsePos - pTest->GetAbsOrigin() ).Length() < 30 )
	{
		return TRUE;
	}

	return FALSE;
}

void CFuncTankControls :: HandleTank ( CBaseEntity *pActivator, CBaseEntity *m_pTank, BOOL activate )
{
	// it's tank entity
	if( m_pTank && m_pTank->IsTank( ))
	{
		if( activate )
		{
			if(((CBaseTank *)m_pTank)->StartControl((CBasePlayer *)pActivator, this ))
			{
				m_iState = STATE_ON; // we have active tank!
			}
		}
		else
		{
			((CBaseTank *)m_pTank)->StopControl( this );
		}
	}
}

void CFuncTankControls :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	if( !m_pController && useType != USE_OFF )
	{
		if( !pActivator || !( pActivator->IsPlayer( )))
			return;

		if( m_iState != STATE_OFF || ((CBasePlayer *)pActivator)->m_pTank != NULL )
			return;

		// find all specified tanks
		for( int i = 0; i < m_cTanks; i++ )
		{
			CBaseEntity *tryTank = NULL;
	
			// find all tanks with current name
			while( (tryTank = UTIL_FindEntityByTargetname( tryTank, STRING( m_iTankName[i] ))))
			{
				HandleTank( pActivator, tryTank, TRUE );
			}			
		}

		if( m_iState == STATE_ON )
		{
			// we found at least one tank to use, so holster player's weapon
			m_pController = (CBasePlayer *)pActivator;
			m_pController->m_pTank = this;

			if( m_pController->m_pActiveItem )
			{
				m_pController->m_pActiveItem->Holster();
				m_pController->pev->weaponmodel = 0;
				// viewmodel reset in tank
			}

			m_pController->m_iHideHUD |= HIDEHUD_WEAPONS;

			// remember where the player's standing, so we can tell when he walks away
			if( m_hParent != NULL && FClassnameIs( m_hParent->pev, "func_tracktrain" ))
			{
				// transform controller pos into local space because parent can be moving
				m_vecControllerUsePos = m_pController->EntityToWorldTransform().VectorITransform( m_pController->GetAbsOrigin() );
			}
			else m_vecControllerUsePos = m_pController->GetAbsOrigin();
		}
	}
	else if( m_pController && useType != USE_ON )
	{
		// find all specified tanks
		for( int i = 0; i < m_cTanks; i++ )
		{
			CBaseEntity *tryTank = NULL;

			// find all tanks with current name
			while(( tryTank = UTIL_FindEntityByTargetname( tryTank, STRING( m_iTankName[i] ))))
			{
				HandleTank( pActivator, tryTank, FALSE );
			}			
		}

		// bring back player's weapons
		if( m_pController->m_pActiveItem )
			m_pController->m_pActiveItem->Deploy();

		m_pController->m_iHideHUD &= ~HIDEHUD_WEAPONS;
		m_pController->m_pTank = NULL;				

		m_pController = NULL;
		m_iState = STATE_OFF;
	}
}
