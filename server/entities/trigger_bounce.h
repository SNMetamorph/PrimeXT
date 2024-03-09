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

//===========================================================
//LRC- trigger_bounce
//===========================================================
#define SF_BOUNCE_CUTOFF 	BIT( 4 )

class CTriggerBounce : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerBounce, CBaseTrigger );
public:
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
};
