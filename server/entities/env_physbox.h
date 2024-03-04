/*
env_physbox.h - simple entity with rigid body physics
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "func_break.h"

#define SF_PHYS_BREAKABLE	128
#define SF_PHYS_CROWBAR		256		// instant break if hit with crowbar
#define SF_PHYS_HOLDABLE	512		// item can be picked up by player

class CPhysEntity : public CBaseDelay
{
	DECLARE_CLASS( CPhysEntity, CBaseDelay );
public:
	void	TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	int		TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void	Killed( entvars_t *pevAttacker, int iGib );
	void	KeyValue( KeyValueData* pkvd );
	void	Touch( CBaseEntity *pOther );
	void	AutoSetSize( void );
	void	Precache( void );
	void	Spawn( void );
	void	DamageSound( void );

	DECLARE_DATADESC();

	virtual int ObjectCaps(void);
	virtual BOOL IsBreakable(void);
	void SetObjectCollisionBox(void);
	const char *DamageDecal( int bitsDamageType );

	int	m_idShard;
	int	m_iszGibModel;
	Materials m_Material;
};
