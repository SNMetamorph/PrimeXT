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
/*

===== h_cycler.cpp ========================================================

  The Halflife Cycler Monsters

*/

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "weapons.h"
#include "player.h"

class CCycler : public CBaseMonster
{
	DECLARE_CLASS( CCycler, CBaseMonster );
public:
	void GenericCyclerSpawn(char *szModel, Vector vecMin, Vector vecMax);
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() | FCAP_IMPULSE_USE); }
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void Spawn( void );
	void Think( void );
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	// Don't treat as a live target
	virtual BOOL IsAlive( void ) { return FALSE; }

	DECLARE_DATADESC();

	int	m_animate;
};

class CGenericCycler : public CCycler
{
	DECLARE_CLASS( CGenericCycler, CBaseMonster );
public:
	void Spawn( void ) { GenericCyclerSpawn( (char *)STRING(pev->model), Vector(-16, -16, 0), Vector(16, 16, 72) ); }
};