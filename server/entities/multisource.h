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

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "client.h"
#include "player.h"
#include "func_door.h"

#define SF_MULTI_INIT		BIT( 0 )

class CMultiSource : public CBaseDelay
{
	DECLARE_CLASS( CMultiSource, CBaseDelay );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Register( void );
	STATE GetState( void );

	DECLARE_DATADESC();

	string_t	m_globalstate;
	EHANDLE	m_rgEntities[MAX_MASTER_TARGETS];
	int	m_rgTriggered[MAX_MASTER_TARGETS];
	int	m_iTotal;
};