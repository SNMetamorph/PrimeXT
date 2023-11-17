/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "effects.h"

class CApacheHVR : public CGrenade
{
public:
	DECLARE_CLASS( CApacheHVR, CGrenade );
	void Spawn( void );
	void Precache( void );
	void IgniteThink( void );
	void AccelerateThink( void );

	DECLARE_DATADESC();

	int m_iTrail;
	Vector m_vecForward;
};