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

#include "ammo_egonclip.h"

LINK_ENTITY_TO_CLASS( ammo_egonclip, CEgonAmmo );

void CEgonAmmo::Spawn( void )
{ 
	Precache( );
	SET_MODEL(ENT(pev), "models/w_chainammo.mdl");
	CBasePlayerAmmo::Spawn( );
}

void CEgonAmmo::Precache( void )
{
	PRECACHE_MODEL ("models/w_chainammo.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");
}

BOOL CEgonAmmo::AddAmmo( CBaseEntity *pOther ) 
{ 
	if (pOther->GiveAmmo( AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY ) != -1)
	{
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		return TRUE;
	}
	return FALSE;
}
