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
//LRC- trigger_onsight
//===========================================================
#define SF_ONSIGHT_NOLOS	0x00001
#define SF_ONSIGHT_NOGLASS	0x00002
#define SF_ONSIGHT_ACTIVE	0x08000
#define SF_ONSIGHT_DEMAND	0x10000

class CTriggerOnSight : public CBaseDelay
{
	DECLARE_CLASS( CTriggerOnSight, CBaseDelay );
public:
	void Spawn( void );
	void Think( void );
	BOOL VisionCheck( void );
	BOOL CanSee( CBaseEntity *pLooker, CBaseEntity *pSeen );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	STATE GetState();
};
