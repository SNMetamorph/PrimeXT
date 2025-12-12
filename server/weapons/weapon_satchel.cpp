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

#include "weapon_satchel.h"
#include "monster_satchel.h"
#include "weapon_layer.h"
#include "weapons/satchel.h"
#include "server_weapon_layer_impl.h"

LINK_ENTITY_TO_CLASS( weapon_satchel, CSatchel );

#define DEFINE_SATCHELWEAPON_FIELD( x, ft ) \
	DEFINE_CUSTOM_FIELD( x, ft, [](CBaseEntity *pEntity, void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CSatchelWeaponContext *ctx = p->m_pWeaponContext->As<CSatchelWeaponContext>(); \
		std::memcpy(pData, &ctx->x, dataSize); \
	}, \
	[](CBaseEntity *pEntity, const void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = static_cast<CBasePlayerWeapon*>(pEntity); \
		CSatchelWeaponContext *ctx = p->m_pWeaponContext->As<CSatchelWeaponContext>(); \
		std::memcpy(&ctx->x, pData, dataSize); \
	})
	
BEGIN_DATADESC( CSatchel )
	DEFINE_SATCHELWEAPON_FIELD( m_chargeReady, FIELD_INTEGER ),
END_DATADESC()

CSatchel::CSatchel()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CSatchelWeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CSatchel::Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_satchel.mdl");	
	FallInit();// get ready to fall down.
}

void CSatchel::Precache( void )
{
	PRECACHE_MODEL("models/v_satchel.mdl");
	PRECACHE_MODEL("models/v_satchel_radio.mdl");
	PRECACHE_MODEL("models/w_satchel.mdl");
	PRECACHE_MODEL("models/p_satchel.mdl");
	PRECACHE_MODEL("models/p_satchel_radio.mdl");

	UTIL_PrecacheOther( "monster_satchel" );
}

int CSatchel::AddToPlayer( CBasePlayer *pPlayer )
{
	int bResult = CBasePlayerItem::AddToPlayer( pPlayer );
	CSatchelWeaponContext *ctx = m_pWeaponContext->As<CSatchelWeaponContext>();

	pPlayer->AddWeapon( m_pWeaponContext->m_iId );
	ctx->m_chargeReady = 0;// this satchel charge weapon now forgets that any satchels are deployed by it.

	if ( bResult )
	{
		return AddWeapon( );
	}
	return FALSE;
}

//=========================================================
// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
//=========================================================
int CSatchel::AddDuplicate( CBasePlayerItem *pOriginal )
{
	if ( g_pGameRules->IsMultiplayer() )
	{
		CSatchel *pSatchel = (CSatchel *)pOriginal;
		CSatchelWeaponContext *ctx = m_pWeaponContext->As<CSatchelWeaponContext>();

		if ( ctx->m_chargeReady != 0 )
		{
			// player has some satchels deployed. Refuse to add more.
			return FALSE;
		}
	}

	return CBasePlayerWeapon::AddDuplicate ( pOriginal );
}
