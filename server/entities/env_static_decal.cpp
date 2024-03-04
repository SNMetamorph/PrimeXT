/*
env_static_decal.h - analog for infodecal that works with new decals system
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "env_static_decal.h"

LINK_ENTITY_TO_CLASS(env_static_decal, CStaticDecal);
//LINK_ENTITY_TO_CLASS(infodecal, CStaticDecal);	// now an alias

void CStaticDecal::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue(pkvd);
}

int CStaticDecal::PasteDecal(int dir)
{
	TraceResult tr;
	Vector vecdir = g_vecZero;
	vecdir[dir % 3] = (dir & 1) ? 32.0f : -32.0f;
	UTIL_TraceLine(pev->origin, pev->origin + vecdir, ignore_monsters, edict(), &tr);
	return UTIL_TraceCustomDecal(&tr, STRING(pev->netname), pev->angles.y, TRUE);
}

void CStaticDecal::Spawn(void)
{
	if (pev->skin <= 0 || pev->skin > 6)
	{
		// try all directions
		int i;
		for (i = 0; i < 6; i++)
		{
			if (PasteDecal(i)) 
				break;
		}
		if (i == 6) ALERT(at_warning, "failed to place decal %s\n", STRING(pev->netname));
	}
	else
	{
		// try specified direction
		PasteDecal(pev->skin - 1);
	}

	// NOTE: don't need to keep this entity
	// with new custom decal save\restore system
	UTIL_Remove(this);
}
