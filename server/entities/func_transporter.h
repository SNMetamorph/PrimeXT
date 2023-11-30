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
#include "client.h"
#include "player.h"
#include "func_break.h"

#define SF_TRANSPORTER_STARTON	BIT( 0 )
#define SF_TRANSPORTER_SOLID		BIT( 1 )

class CFuncTransporter : public CBaseDelay
{
	DECLARE_CLASS(CFuncTransporter, CBaseDelay);
public:
	void Spawn(void);
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	virtual int ObjectCaps(void) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_SET_MOVEDIR; }
	void UpdateSpeed(float speed);
	void Think(void);

	DECLARE_DATADESC();

	float	m_flMaxSpeed;
	float	m_flWidth;
};