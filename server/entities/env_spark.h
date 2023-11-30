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

#define SF_SPARK_TOGGLE		BIT( 5 )
#define SF_SPARK_START_ON		BIT( 6 )

class CEnvSpark : public CBaseDelay
{
	DECLARE_CLASS( CEnvSpark, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void SparkThink( void );
	void SparkUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();
};