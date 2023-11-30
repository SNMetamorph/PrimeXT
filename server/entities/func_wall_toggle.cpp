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

#include "func_wall_toggle.h"

LINK_ENTITY_TO_CLASS(func_wall_toggle, CFuncWallToggle);

void CFuncWallToggle::Spawn(void)
{
	BaseClass::Spawn();

	if (FBitSet(pev->spawnflags, SF_WALL_START_OFF))
		TurnOff();
	else TurnOn();
}

void CFuncWallToggle::TurnOff(void)
{
	WorldPhysic->EnableCollision(this, FALSE);
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	RelinkEntity(FALSE);
	m_iState = STATE_OFF;
}

void CFuncWallToggle::TurnOn(void)
{
	WorldPhysic->EnableCollision(this, TRUE);
	pev->solid = SOLID_BSP;
	pev->effects &= ~EF_NODRAW;
	RelinkEntity(FALSE);
	m_iState = STATE_ON;
}