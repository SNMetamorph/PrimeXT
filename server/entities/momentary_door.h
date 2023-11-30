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

#include "func_door.h"

#define SF_DOOR_CONSTANT_SPEED		BIT( 1 )

class CMomentaryDoor : public CBaseToggle
{
	DECLARE_CLASS( CMomentaryDoor, CBaseToggle );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)) | FCAP_SET_MOVEDIR; }
	void Blocked( CBaseEntity *pOther );
	void StopMoveSound( void );
	virtual void MoveDone( void );

	DECLARE_DATADESC();

	void OnChangeParent( void );
	void OnClearParent( void );

	int m_iMoveSnd; // sound a door makes while moving	
};