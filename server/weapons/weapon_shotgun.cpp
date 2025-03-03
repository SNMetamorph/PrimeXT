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

#include "weapon_shotgun.h"
#include "weapon_layer.h"
#include "weapons/shotgun.h"
#include "server_weapon_layer_impl.h"
#include "user_messages.h"

LINK_ENTITY_TO_CLASS( weapon_shotgun, CShotgun );

CShotgun::CShotgun()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CShotgunWeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CShotgun::Spawn()
{
	pev->classname = MAKE_STRING(CLASSNAME_STR(SHOTGUN_CLASSNAME)); // hack to allow for old names
	Precache();
	SET_MODEL(ENT(pev), "models/w_shotgun.mdl");
	FallInit(); // get ready to fall
}

void CShotgun::Precache()
{
	PRECACHE_MODEL("models/v_shotgun.mdl");
	PRECACHE_MODEL("models/w_shotgun.mdl");
	PRECACHE_MODEL("models/p_shotgun.mdl");
	PRECACHE_MODEL("models/shotgunshell.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");              
	PRECACHE_SOUND("weapons/dbarrel1.wav");	// shotgun
	PRECACHE_SOUND("weapons/sbarrel1.wav");	// shotgun
	PRECACHE_SOUND("weapons/reload1.wav");	// shotgun reload
	PRECACHE_SOUND("weapons/reload3.wav");	// shotgun reload

	PRECACHE_SOUND("weapons/357_cock1.wav"); // gun empty sound
	PRECACHE_SOUND("weapons/scock1.wav");	// cock gun
}

int CShotgun::AddToPlayer(CBasePlayer *pPlayer)
{
	if (CBasePlayerWeapon::AddToPlayer(pPlayer))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
			WRITE_BYTE(m_pWeaponContext->m_iId);
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}
