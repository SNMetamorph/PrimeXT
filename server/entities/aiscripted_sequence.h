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
***/

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "scripted_sequence.h"

class CCineAI : public CCineMonster
{
	DECLARE_CLASS( CCineAI, CCineMonster );
public:
	BOOL StartSequence( CBaseMonster *pTarget, int iszSeq, BOOL completeOnEmpty );
	void PossessEntity( void );
	BOOL FCanOverrideState ( void );
	virtual void FixScriptMonsterSchedule( CBaseMonster *pMonster );
};
