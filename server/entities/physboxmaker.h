/*
physboxmaker.h - simple entity for spawning env_physbox entities
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
#include "env_physbox.h"

// pushablemaker spawnflags
#define SF_PHYSBOXMAKER_START_ON	1 // start active ( if has targetname )
#define SF_PHYSBOXMAKER_CYCLIC	4 // drop one monster every time fired.

class CPhysBoxMaker : public CPhysEntity
{
	DECLARE_CLASS( CPhysBoxMaker, CPhysEntity );
public:
	void Spawn( void );
	void KeyValue( KeyValueData* pkvd);
	void ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void CyclicUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MakerThink( void );
	void DeathNotice ( entvars_t *pevChild );// monster maker children use this to tell the monster maker that they have died.
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	void MakePhysBox( void );

	DECLARE_DATADESC();

	int m_cNumBoxes;	// max number of monsters this ent can create

	int m_cLiveBoxes;	// how many monsters made by this monster maker that are currently alive
	int m_iMaxLiveBoxes;// max number of pushables that this maker may have out at one time.

	float m_flGround;	// z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child
};
