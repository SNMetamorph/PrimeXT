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

//	-------------------------------------------
//
//	maprules.cpp
//
//	This module contains entities for implementing/changing game
//	rules dynamically within each map (.BSP)
//
//	-------------------------------------------

#include "maprules.h"

BEGIN_DATADESC( CRuleEntity )
	DEFINE_KEYFIELD( m_iszMaster, FIELD_STRING, "master" ),
END_DATADESC()

void CRuleEntity::Spawn( void )
{
	pev->solid	= SOLID_NOT;
	pev->movetype	= MOVETYPE_NONE;
	pev->effects	= EF_NODRAW;
}

void CRuleEntity::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "master"))
	{
		SetMaster( ALLOC_STRING(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

BOOL CRuleEntity::CanFireForActivator( CBaseEntity *pActivator )
{
	if ( m_iszMaster )
	{
		if ( UTIL_IsMasterTriggered( m_iszMaster, pActivator ) )
			return TRUE;
		else
			return FALSE;
	}
	
	return TRUE;
}

void CRulePointEntity::Spawn( void )
{
	CRuleEntity::Spawn();
	pev->frame = 0;
	pev->model = 0;
}

void CRuleBrushEntity::Spawn( void )
{
	SET_MODEL( edict(), STRING(pev->model) );
	BaseClass :: Spawn();
}
