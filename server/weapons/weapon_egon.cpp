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

#include "weapon_egon.h"
#include "weapon_layer.h"
#include "weapons/egon.h"
#include "server_weapon_layer_impl.h"

#define DEFINE_EGONWEAPON_FIELD( x, ft ) \
	DEFINE_CUSTOM_FIELD( x, ft, [](CBaseEntity *pEntity, void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CEgonWeaponContext *ctx = static_cast<CEgonWeaponContext*>(p->m_pWeaponContext.get()); \
		std::memcpy(pData, &ctx->x, dataSize); \
	}, \
	[](CBaseEntity *pEntity, const void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CEgonWeaponContext *ctx = static_cast<CEgonWeaponContext*>(p->m_pWeaponContext.get()); \
		std::memcpy(&ctx->x, pData, dataSize); \
	})

BEGIN_DATADESC( CEgon )
	DEFINE_EGONWEAPON_FIELD( m_pBeam, FIELD_CLASSPTR ),
	DEFINE_EGONWEAPON_FIELD( m_pNoise, FIELD_CLASSPTR ),
	DEFINE_EGONWEAPON_FIELD( m_pSprite, FIELD_CLASSPTR ),
	DEFINE_EGONWEAPON_FIELD( m_fireState, FIELD_INTEGER ),
	DEFINE_EGONWEAPON_FIELD( m_fireMode, FIELD_INTEGER ),
	DEFINE_EGONWEAPON_FIELD( m_shakeTime, FIELD_TIME ),
	DEFINE_EGONWEAPON_FIELD( m_flAmmoUseTime, FIELD_TIME ),
	DEFINE_EGONWEAPON_FIELD( m_flAttackCooldown, FIELD_FLOAT ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( weapon_egon, CEgon );

CEgon::CEgon()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CEgonWeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CEgon::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_egon.mdl");
	FallInit();// get ready to fall down.
}

void CEgon::Precache()
{
	PRECACHE_MODEL("models/w_egon.mdl");
	PRECACHE_MODEL("models/v_egon.mdl");
	PRECACHE_MODEL("models/p_egon.mdl");

	PRECACHE_MODEL("models/w_9mmclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND(EGON_SOUND_OFF);
	PRECACHE_SOUND(EGON_SOUND_RUN);
	PRECACHE_SOUND(EGON_SOUND_STARTUP);

	PRECACHE_MODEL(EGON_BEAM_SPRITE);
	PRECACHE_MODEL(EGON_FLARE_SPRITE);

	PRECACHE_SOUND("weapons/357_cock1.wav");
}

int CEgon::AddToPlayer(CBasePlayer *pPlayer)
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
