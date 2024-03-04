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
#include "plats.h"

// ----------------------------------------------------------
//
//
// pev->speed is the travel speed
// pev->health is current health
// pev->max_health is the amount to reset to each time it starts

#define FGUNTARGET_START_ON			0x0001

class CGunTarget : public CBaseMonster
{
	DECLARE_CLASS( CGunTarget, CBaseMonster );
public:
	void Spawn( void );
	void Activate( void );
	void Next( void );
	void Start( void );
	void Wait( void );
	void Stop( void );

	int BloodColor( void ) { return DONT_BLEED; }
	int Classify( void ) { return CLASS_MACHINE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	Vector BodyTarget( const Vector &posSrc ) { return GetAbsOrigin(); }
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

private:
	BOOL	m_on;
};
