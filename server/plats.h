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
#include "trains.h"
#include "saverestore.h"
#include "func_traindoor.h"

#define SF_PLAT_TOGGLE		BIT( 0 )

class CBasePlatTrain : public CBaseToggle
{
	DECLARE_CLASS( CBasePlatTrain, CBaseToggle );
public:
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData* pkvd);
	void Precache( void );

	// This is done to fix spawn flag collisions between this class and a derived class
	virtual BOOL IsTogglePlat( void ) { return (pev->spawnflags & SF_PLAT_TOGGLE) ? TRUE : FALSE; }

	DECLARE_DATADESC();

	int	m_iMoveSnd;	// sound a plat makes while moving
	int	m_iStopSnd;	// sound a plat makes when it stops
	float	m_volume;		// Sound volume
	float	m_flFloor;	// Floor number
};
