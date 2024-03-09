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

#define SF_TRIG_PUSH_ONCE	BIT( 0 )
#define SF_TRIGGER_PUSH_START_OFF	2	// spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIG_PUSH_SETUP	BIT( 31 )

class CTriggerPush : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerPush, CBaseTrigger );
public:
	void Spawn( void );
	void Activate( void );
	void Touch( CBaseEntity *pOther );
	Vector m_vecPushDir;
};
