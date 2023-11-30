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

class CFuncClock : public CBaseDelay
{
	DECLARE_CLASS(CFuncClock, CBaseDelay);
public:
	void Spawn(void);
	void Think(void);
	void Activate(void);
	void KeyValue(KeyValueData *pkvd);
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();
private:
	Vector	m_vecCurtime;
	Vector	m_vecFinaltime;
	float	m_flCurTime;	// current unconverted time in seconds
	int	m_iClockType;	// also contain seconds count for different modes
	int	m_iHoursCount;
	BOOL	m_fInit;		// this clock already init
};