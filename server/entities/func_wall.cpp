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

#include "func_wall.h"

LINK_ENTITY_TO_CLASS(func_wall, CFuncWall);

void CFuncWall::Spawn(void)
{
	pev->flags |= FL_WORLDBRUSH;
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	m_iState = STATE_OFF;

	SetLocalAngles(g_vecZero);
	SET_MODEL(edict(), GetModel());

	// LRC (support generic switchable texlight)
	if (m_iStyle >= 32)
		LIGHT_STYLE(m_iStyle, "a");
	else if (m_iStyle <= -32)
		LIGHT_STYLE(-m_iStyle, "z");

	if (m_hParent != NULL || FClassnameIs(pev, "func_wall_toggle"))
		m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity(this);
	else m_pUserData = WorldPhysic->CreateStaticBodyFromEntity(this);
}

void CFuncWall::TurnOff(void)
{
	pev->frame = 0;
	m_iState = STATE_OFF;
}

void CFuncWall::TurnOn(void)
{
	pev->frame = 1;
	m_iState = STATE_ON;
}

void CFuncWall::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (ShouldToggle(useType))
	{
		if (GetState() == STATE_ON)
			TurnOff();
		else TurnOn();

		// LRC (support generic switchable texlight)
		if (m_iStyle >= 32)
		{
			if (pev->frame)
				LIGHT_STYLE(m_iStyle, "z");
			else
				LIGHT_STYLE(m_iStyle, "a");
		}
		else if (m_iStyle <= -32)
		{
			if (pev->frame)
				LIGHT_STYLE(-m_iStyle, "a");
			else
				LIGHT_STYLE(-m_iStyle, "z");
		}
	}
}