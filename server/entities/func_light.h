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
#include "func_break.h"

#define SF_LIGHT_START_ON		BIT( 0 )

class CFuncLight : public CBreakable
{
	DECLARE_CLASS(CFuncLight, CBreakable);
public:
	void Spawn(void);
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	virtual int TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	void TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	void Flicker(void);
	void Die(void);

	DECLARE_DATADESC();
private:
	int	m_iFlickerMode;
	Vector	m_vecLastDmgPoint;
	float	m_flNextFlickerTime;
};