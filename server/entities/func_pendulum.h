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

#define SF_PENDULUM_START_ON		BIT( 0 )
#define SF_PENDULUM_SWING		BIT( 1 )
#define SF_PENDULUM_PASSABLE		BIT( 3 )
#define SF_PENDULUM_AUTO_RETURN	BIT( 4 )

class CPendulum : public CBaseDelay
{
	DECLARE_CLASS(CPendulum, CBaseDelay);
public:
	void	Spawn(void);
	void	KeyValue(KeyValueData *pkvd);
	void	Swing(void);
	void	Stop(void);
	void	RopeTouch(CBaseEntity *pOther); // this touch func makes the pendulum a rope
	virtual int ObjectCaps(void) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_HOLD_ANGLES; }
	void	Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	void	Touch(CBaseEntity *pOther);
	void	Blocked(CBaseEntity *pOther);

	DECLARE_DATADESC();

	float	m_accel;	// Acceleration
	float	m_distance;
	float	m_time;
	float	m_damp;
	float	m_maxSpeed;
	float	m_dampSpeed;
	Vector	m_center;
	Vector	m_start;
};