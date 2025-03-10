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

#include "weapon_handgrenade.h"
#include "weapon_layer.h"
#include "weapons/handgrenade.h"
#include "server_weapon_layer_impl.h"

#define HANDGRENADE_PRIMARY_VOLUME	450

LINK_ENTITY_TO_CLASS( weapon_handgrenade, CHandGrenade );

CHandGrenade::CHandGrenade()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CHandGrenadeWeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CHandGrenade::Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_grenade.mdl");
	pev->dmg = gSkillData.plrDmgHandGrenade;
	FallInit(); // get ready to fall down.
}

void CHandGrenade::Precache( void )
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_MODEL("models/v_grenade.mdl");
	PRECACHE_MODEL("models/p_grenade.mdl");
}
