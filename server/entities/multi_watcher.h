/*
multi_watcher.h - an entity that implements the “if-else” logical condition
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
// 		   multi_watcher
//=======================================================================
#define SF_WATCHER_START_OFF	1

#define LOGIC_AND  		0 // fire if all objects active
#define LOGIC_OR   		1 // fire if any object active
#define LOGIC_NAND 		2 // fire if not all objects active
#define LOGIC_NOR  		3 // fire if all objects disable
#define LOGIC_XOR		4 // fire if only one (any) object active
#define LOGIC_XNOR		5 // fire if active any number objects, but < then all

class CMultiMaster : public CBaseDelay
{
	DECLARE_CLASS( CMultiMaster, CBaseDelay );
public:
	void Spawn( void );
	void Think( void );
	void KeyValue( KeyValueData *pkvd );
	int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE GetState( CBaseEntity *pActivator ) { return EvalLogic( pActivator ) ? STATE_ON : STATE_OFF; };
	virtual STATE GetState( void ) { return m_iState; };
	int GetLogicModeForString( const char *string );
	bool CheckState( STATE state, int targetnum );

	DECLARE_DATADESC();

	int	m_cTargets; // the total number of targets in this manager's fire list.
	int	m_iTargetName[MAX_MULTI_TARGETS]; // list of indexes into global string array
          STATE	m_iTargetState[MAX_MULTI_TARGETS]; // list of wishstate targets
	STATE	m_iSharedState;
	int	m_iLogicMode;
	
	BOOL	EvalLogic( CBaseEntity *pEntity );
	bool	globalstate;
};
