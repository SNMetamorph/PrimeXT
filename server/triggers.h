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

// common trigger spawnflags
#define SF_TRIGGER_ALLOWMONSTERS	1	// monsters allowed to fire this trigger
#define SF_TRIGGER_NOCLIENTS		2	// players not allowed to fire this trigger
#define SF_TRIGGER_PUSHABLES		4	// only pushables can fire this trigger
#define SF_TRIGGER_CHECKANGLES		8	// Quake style triggers - fire when activator angles matched with trigger angles
#define SF_TRIGGER_ALLOWPHYSICS		16	// react on a rigid-bodies

class CBaseTrigger : public CBaseToggle
{
	DECLARE_CLASS( CBaseTrigger, CBaseToggle );
public:
	void TeleportTouch ( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void MultiTouch( CBaseEntity *pOther );
	void HurtTouch ( CBaseEntity *pOther );
	void ActivateMultiTrigger( CBaseEntity *pActivator );
	void MultiWaitOver( void );
	void CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL CanTouch( CBaseEntity *pOther );
	void InitTrigger( void );

	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_SET_MOVEDIR; }

	DECLARE_DATADESC();
};
