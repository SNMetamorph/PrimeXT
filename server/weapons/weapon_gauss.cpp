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

#include "weapon_gauss.h"
#include "weapon_layer.h"
#include "weapons/gauss.h"
#include "server_weapon_layer_impl.h"
#include "user_messages.h"

#define DEFINE_GAUSSWEAPON_FIELD( x, ft ) \
	DEFINE_CUSTOM_FIELD( x, ft, [](CBaseEntity *pEntity, void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CGaussWeaponContext *ctx = static_cast<CGaussWeaponContext*>(p->m_pWeaponContext.get()); \
		std::memcpy(pData, &ctx->x, dataSize); \
	}, \
	[](CBaseEntity *pEntity, const void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CGaussWeaponContext *ctx = static_cast<CGaussWeaponContext*>(p->m_pWeaponContext.get()); \
		std::memcpy(&ctx->x, pData, dataSize); \
	})

BEGIN_DATADESC( CGauss )
	DEFINE_GAUSSWEAPON_FIELD( m_fInAttack, FIELD_INTEGER ),
	DEFINE_GAUSSWEAPON_FIELD( m_fPrimaryFire, FIELD_BOOLEAN ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( weapon_gauss, CGauss );

CGauss::CGauss()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CGaussWeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CGauss::Spawn()
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_gauss.mdl");
	FallInit();// get ready to fall down.
}

void CGauss::Precache()
{
	PRECACHE_MODEL("models/w_gauss.mdl");
	PRECACHE_MODEL("models/v_gauss.mdl");
	PRECACHE_MODEL("models/p_gauss.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("weapons/gauss2.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/electro5.wav");
	PRECACHE_SOUND("weapons/electro6.wav");
	PRECACHE_SOUND("ambience/pulsemachine.wav");

	PRECACHE_MODEL("sprites/hotglow.spr");
	PRECACHE_MODEL("sprites/hotglow.spr");
	PRECACHE_MODEL("sprites/smoke.spr");
}

int CGauss::AddToPlayer(CBasePlayer *pPlayer)
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
