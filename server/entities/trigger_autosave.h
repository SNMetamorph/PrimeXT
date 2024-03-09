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
#include "triggers.h"

class CTriggerSave : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerSave, CBaseTrigger );
public:
	void Spawn( void );
	void SaveTouch( CBaseEntity *pOther );

	DECLARE_DATADESC();
};
