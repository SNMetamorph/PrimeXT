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

#include "weapon_rpg.h"
#include "weapon_layer.h"
#include "weapons/rpg.h"
#include "server_weapon_layer_impl.h"
#include "rpg_rocket.h"

LINK_ENTITY_TO_CLASS( weapon_rpg, CRpg );

#define DEFINE_RPGWEAPON_FIELD( x, ft ) \
	DEFINE_CUSTOM_FIELD( x, ft, [](CBaseEntity *pEntity, void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CRpgWeaponContext *ctx = p->m_pWeaponContext->As<CRpgWeaponContext>(); \
		std::memcpy(pData, &ctx->x, dataSize); \
	}, \
	[](CBaseEntity *pEntity, const void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CRpgWeaponContext *ctx = p->m_pWeaponContext->As<CRpgWeaponContext>(); \
		std::memcpy(&ctx->x, pData, dataSize); \
	})

BEGIN_DATADESC( CRpg )
	DEFINE_RPGWEAPON_FIELD( m_fSpotActive, FIELD_INTEGER ),
	DEFINE_RPGWEAPON_FIELD( m_cActiveRockets, FIELD_INTEGER ),
END_DATADESC()

CRpg::CRpg()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CRpgWeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CRpg::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_rpg.mdl");
	FallInit(); // get ready to fall down.
}

void CRpg::Precache()
{
	PRECACHE_MODEL("models/w_rpg.mdl");
	PRECACHE_MODEL("models/v_rpg.mdl");
	PRECACHE_MODEL("models/p_rpg.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound

	UTIL_PrecacheOther("laser_spot");
	UTIL_PrecacheOther("rpg_rocket");
}

int CRpg::AddToPlayer(CBasePlayer *pPlayer)
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

void CRpg::AddActiveRocket()
{
	CRpgWeaponContext *ctx = m_pWeaponContext->As<CRpgWeaponContext>();
	ctx->m_cActiveRockets++;
}

void CRpg::RemoveActiveRocket()
{
	CRpgWeaponContext *ctx = m_pWeaponContext->As<CRpgWeaponContext>();
	ctx->m_cActiveRockets--;
}
