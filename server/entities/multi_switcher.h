/*
multi_switcher.h - target switcher that can shuffle targets randomly
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

//=======================================================================
// 		   multi_switcher
//=======================================================================
#define MODE_INCREMENT		0
#define MODE_DECREMENT		1
#define MODE_RANDOM_VALUE		2

#define SF_SWITCHER_START_ON		BIT( 0 )

class CSwitcher : public CBaseDelay
{
	DECLARE_CLASS( CSwitcher, CBaseDelay );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Spawn( void );
	void Think( void );

	DECLARE_DATADESC();

	int m_iTargetName[MAX_MULTI_TARGETS]; // list if indexes into global string array
	int m_iTargetOrder[MAX_MULTI_TARGETS];	// don't save\restore
	int m_cTargets; // the total number of targets in this manager's fire list.
	int m_index; // Current target
};
