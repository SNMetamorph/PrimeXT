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

// RenderFxManager itself
class CRenderFxManager : public CPointEntity
{
	DECLARE_CLASS( CRenderFxManager, CPointEntity );
public:
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Affect( CBaseEntity *pEntity, BOOL bIsLocus, CBaseEntity *pActivator );

	void KeyValue( KeyValueData *pkvd );
};
