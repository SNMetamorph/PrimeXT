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

#define SF_ROTATING_INSTANT		BIT( 0 )
#define SF_ROTATING_BACKWARDS		BIT( 1 )
#define SF_ROTATING_Z_AXIS		BIT( 2 )
#define SF_ROTATING_X_AXIS		BIT( 3 )
#define SF_ROTATING_ACCDCC		BIT( 4 )	// brush should accelerate and decelerate when toggled
#define SF_ROTATING_HURT		BIT( 5 )	// rotating brush that inflicts pain based on rotation speed
#define SF_ROTATING_NOT_SOLID		BIT( 6 )	// some special rotating objects are not solid.
#define SF_ROTATING_SND_SMALL_RADIUS	BIT( 7 )
#define SF_ROTATING_SND_MEDIUM_RADIUS	BIT( 8 )
#define SF_ROTATING_SND_LARGE_RADIUS	BIT( 9 )
#define SF_ROTATING_STOP_AT_START_POS	BIT( 10 )

//
// RampPitchVol - ramp pitch and volume up to final values, based on difference
// between how fast we're going vs how fast we plan to go
//
#define FANPITCHMIN		30
#define FANPITCHMAX		100

class CFuncRotating : public CBaseDelay
{
	DECLARE_CLASS(CFuncRotating, CBaseDelay);
public:
	void Spawn(void);
	void Precache(void);
	void SpinUp(void);
	void SpinDown(void);
	void ReverseMove(void);
	void RotateFriction(void);
	void KeyValue(KeyValueData* pkvd);
	virtual int ObjectCaps(void) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_HOLD_ANGLES; }
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	void UpdateSpeed(float flNewSpeed);
	bool SpinDown(float flTargetSpeed);
	void SetTargetSpeed(float flSpeed);
	void HurtTouch(CBaseEntity *pOther);
	void Rotate(void);
	void RampPitchVol(void);
	void Blocked(CBaseEntity *pOther);
	float GetNextMoveInterval(void) const;

	DECLARE_DATADESC();

private:
	byte	m_bStopAtStartPos;
	float	m_flFanFriction;
	float	m_flAttenuation;
	float	m_flTargetSpeed;
	float	m_flMaxSpeed;
	float	m_flVolume;
	Vector	m_angStart;
	string_t	m_sounds;
};