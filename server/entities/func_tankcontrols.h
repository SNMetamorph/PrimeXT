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
#include "weapons.h"
#include "player.h"

#define MAX_CONTROLLED_TANKS		48

class CFuncTankControls : public CBaseDelay
{
	DECLARE_CLASS( CFuncTankControls, CBaseDelay );
public:
	virtual int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void HandleTank( CBaseEntity *pActivator, CBaseEntity *m_pTank, BOOL activate );
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );

	DECLARE_DATADESC();

	BOOL OnControls( CBaseEntity *pTest );

	CBasePlayer	*m_pController;
	Vector		m_vecControllerUsePos; // where was the player standing when he used me?
	int		m_iTankName[MAX_CONTROLLED_TANKS]; // list if indexes into global string array
	int		m_cTanks; // the total number of targets in this manager's fire list.
	BOOL		m_fVerifyTanks;
};