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

#include "func_pushable.h"

// pushablemaker spawnflags
#define SF_PUSHABLEMAKER_START_ON	1 // start active ( if has targetname )
#define SF_PUSHABLEMAKER_CYCLIC	4 // drop one monster every time fired.

class CPushableMaker : public CPushable
{
	DECLARE_CLASS( CPushableMaker, CPushable );
public:
	void Spawn( void );
	void KeyValue( KeyValueData* pkvd);
	void ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void CyclicUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MakerThink( void );
	void DeathNotice ( entvars_t *pevChild );// monster maker children use this to tell the monster maker that they have died.
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	void MakePushable( void );

	DECLARE_DATADESC();

	int m_cNumBoxes;	// max number of monsters this ent can create

	int m_cLiveBoxes;	// how many monsters made by this monster maker that are currently alive
	int m_iMaxLiveBoxes;// max number of pushables that this maker may have out at one time.

	float m_flGround;	// z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child
};