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

#include "func_monsterclip.h"

LINK_ENTITY_TO_CLASS(func_monsterclip, CFuncMonsterClip);

void CFuncMonsterClip::Spawn(void)
{
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	SetBits(pev->flags, FL_MONSTERCLIP);
	SetBits(m_iFlags, MF_TRIGGER);
	pev->effects |= EF_NODRAW;

	// link into world
	SET_MODEL(edict(), GetModel());
}