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

//=======================================================================
// 		   Logic_generator
//=======================================================================
#define NORMAL_MODE			0
#define RANDOM_MODE			1
#define RANDOM_MODE_WITH_DEC		2

#define SF_GENERATOR_START_ON		BIT( 0 )

class CGenerator : public CBaseDelay
{
	DECLARE_CLASS( CGenerator, CBaseDelay );
public:
	void Spawn( void );
	void Think( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();
private:
	int m_iFireCount;
	int m_iMaxFireCount;
};
