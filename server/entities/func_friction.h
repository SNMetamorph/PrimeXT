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

class CFrictionModifier : public CBaseEntity
{
	DECLARE_CLASS( CFrictionModifier, CBaseEntity );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void ChangeFriction( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	float		m_frictionFraction;		// Sorry, couldn't resist this name :)
};
