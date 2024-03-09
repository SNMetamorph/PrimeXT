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

//**********************************************************
// The Multimanager Entity - when fired, will fire up to 16 targets 
// at specified times.
// FLAG:		THREAD (create clones when triggered)
// FLAG:		CLONE (this is a clone for a threaded execution)

#define SF_MULTIMAN_CLONE		0x80000000
#define SF_MULTIMAN_THREAD		BIT( 0 )
// used on a Valve maps
#define SF_MULTIMAN_LOOP		BIT( 2 )
#define SF_MULTIMAN_ONLYONCE		BIT( 3 )
#define SF_MULTIMAN_START_ON		BIT( 4 )	// same as START_ON

class CMultiManager : public CBaseDelay
{
	DECLARE_CLASS( CMultiManager, CBaseDelay );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void ManagerThink ( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
#if _DEBUG
	void ManagerReport( void );
#endif
	BOOL HasTarget( string_t targetname );
	int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_NOT_MASTER; }

	DECLARE_DATADESC();

	int	m_cTargets;			// the total number of targets in this manager's fire list.
	int	m_index;				// Current target
	float	m_startTime;			// Time we started firing
	int	m_iTargetName[MAX_MULTI_TARGETS];	// list if indexes into global string array
	float	m_flTargetDelay[MAX_MULTI_TARGETS];	// delay (in seconds) from time of manager fire to target fire
private:
	inline BOOL IsClone( void )
	{
		return (pev->spawnflags & SF_MULTIMAN_CLONE) ? TRUE : FALSE;
	}

	inline BOOL ShouldClone( void ) 
	{ 
		if( IsClone() )
			return FALSE;
		return (FBitSet( pev->spawnflags, SF_MULTIMAN_THREAD )) ? TRUE : FALSE; 
	}

	CMultiManager *Clone( void );
};
