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

#include "func_conveyor.h"

LINK_ENTITY_TO_CLASS(func_conveyor, CFuncConveyor);

BEGIN_DATADESC(CFuncConveyor)
	DEFINE_FIELD(m_flMaxSpeed, FIELD_FLOAT),
END_DATADESC()

void CFuncConveyor::Spawn(void)
{
	pev->flags |= FL_WORLDBRUSH;
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL(edict(), GetModel());

	if (!FBitSet(pev->spawnflags, SF_CONVEYOR_VISUAL))
		SetBits(pev->flags, FL_CONVEYOR);

	// is mapper forgot set angles?
	if (pev->movedir == g_vecZero)
		pev->movedir = Vector(1, 0, 0);

	// HACKHACK - This is to allow for some special effects
	if (FBitSet(pev->spawnflags, SF_CONVEYOR_NOTSOLID))
	{
		pev->solid = SOLID_NOT;
		pev->skin = 0; // don't want the engine thinking we've got special contents on this brush
	}
	else
	{
		if (m_hParent)
			m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity(this);
		else m_pUserData = WorldPhysic->CreateStaticBodyFromEntity(this);
	}

	if (pev->speed == 0)
		pev->speed = 100;

	m_flMaxSpeed = pev->speed;	// save initial speed

	if (FBitSet(pev->spawnflags, SF_CONVEYOR_STARTOFF))
		UpdateSpeed(0);
	else UpdateSpeed(m_flMaxSpeed);
}

void CFuncConveyor::UpdateSpeed(float speed)
{
	// make sure the sign is correct - positive for forward rotation,
	// negative for reverse rotation.
	speed = fabs(speed);
	if (pev->impulse) speed *= -1;

	// encode it as an integer with 4 fractional bits
	int speedCode = (int)(fabs(speed) * 16.0f);

	// HACKHACK -- This is ugly, but encode the speed in the rendercolor
	// to avoid adding more data to the network stream
	pev->rendercolor.x = (speed < 0) ? 1 : 0;
	pev->rendercolor.y = (speedCode >> 8);
	pev->rendercolor.z = (speedCode & 0xFF);

	// set conveyor state
	m_iState = (speed != 0) ? STATE_ON : STATE_OFF;
	pev->speed = speed;	// update physical speed too
}

void CFuncConveyor::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (ShouldToggle(useType))
	{
		if (useType == USE_SET)
		{
			if (value != 0)
			{
				pev->impulse = value < 0 ? true : false;
				value = fabs(value);
				pev->dmg = bound(0.1, value, 1) * m_flMaxSpeed;
			}
			else UpdateSpeed(0); // stop
			return;
		}
		else if (useType == USE_RESET)
		{
			// restore last speed
			UpdateSpeed(pev->dmg);
			return;
		}

		pev->impulse = !pev->impulse;
		UpdateSpeed(m_flMaxSpeed);
	}
}