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

#include "func_break.h"

// func_pushable (it's also func_breakable, so don't collide with those flags)
#define SF_PUSH_BREAKABLE		128
#define SF_PUSH_HOLDABLE		512	// item can be picked up by player
#define SF_PUSH_BSPCOLLISION		1024	// use BSP tree instead of bbox

class CPushable : public CBreakable
{
	DECLARE_CLASS( CPushable, CBreakable );
public:
	void	Spawn ( void );
	void	Precache( void );
	void	Touch ( CBaseEntity *pOther );
	void	Move( CBaseEntity *pMover, int push );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	OnClearParent( void );

	virtual int ObjectCaps( void );

	inline float MaxSpeed( void ) { return m_maxSpeed; }
	
	// breakables use an overridden takedamage
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	virtual BOOL IsPushable( void );
	
	DECLARE_DATADESC();

	static char *m_soundNames[3];
	int	m_lastSound;	// no need to save/restore, just keeps the same sound from playing twice in a row
	float	m_maxSpeed;
	float	m_soundTime;
};