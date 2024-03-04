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

// Blood effects
class CBlood : public CPointEntity
{
	DECLARE_CLASS( CBlood, CPointEntity );
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_SET_MOVEDIR; }
	void	KeyValue( KeyValueData *pkvd );

	inline	int Color( void ) { return pev->impulse; }
	inline	float BloodAmount( void ) { return pev->dmg; }

	inline	void SetColor( int color ) { pev->impulse = color; }
	inline	void SetBloodAmount( float amount ) { pev->dmg = amount; }
	
	Vector	Direction( void );
	Vector	BloodPosition( CBaseEntity *pActivator );
};
