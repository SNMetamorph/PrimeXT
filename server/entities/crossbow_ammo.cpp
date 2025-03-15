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

#include "crossbow_ammo.h"
#include "weapons/crossbow.h"

LINK_ENTITY_TO_CLASS( ammo_crossbow, CCrossbowAmmo );

void CCrossbowAmmo::Spawn( void )
{ 
	Precache( );
	SET_MODEL(ENT(pev), "models/w_crossbow_clip.mdl");
	CBasePlayerAmmo::Spawn( );
}

void CCrossbowAmmo::Precache( void )
{
	PRECACHE_MODEL ("models/w_crossbow_clip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");
}

BOOL CCrossbowAmmo::AddAmmo( CBaseEntity *pOther ) 
{ 
	if (pOther->GiveAmmo( CROSSBOW_MAX_CLIP, "bolts", BOLT_MAX_CARRY ) != -1)
	{
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		return TRUE;
	}
	return FALSE;
}
