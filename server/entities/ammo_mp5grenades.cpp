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

#include "ammo_mp5grenades.h"

LINK_ENTITY_TO_CLASS( ammo_mp5grenades, CMP5AmmoGrenade );
LINK_ENTITY_TO_CLASS( ammo_ARgrenades, CMP5AmmoGrenade );

void CMP5AmmoGrenade::Spawn( void )
{ 
	Precache( );
	SET_MODEL(ENT(pev), "models/w_ARgrenade.mdl");
	CBasePlayerAmmo::Spawn( );
}

void CMP5AmmoGrenade::Precache( void )
{
	PRECACHE_MODEL ("models/w_ARgrenade.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");
}

BOOL CMP5AmmoGrenade::AddAmmo( CBaseEntity *pOther ) 
{ 
	int bResult = (pOther->GiveAmmo( AMMO_M203BOX_GIVE, "ARgrenades", M203_GRENADE_MAX_CARRY ) != -1);

	if (bResult)
	{
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
	}
	return bResult;
}
