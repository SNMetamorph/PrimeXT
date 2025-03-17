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

#include "weapon_crowbar.h"
#include "weapon_layer.h"
#include "weapons/crowbar.h"
#include "server_weapon_layer_impl.h"
#include <utility>

#define CROWBAR_BODYHIT_VOLUME	128
#define CROWBAR_WALLHIT_VOLUME	512

LINK_ENTITY_TO_CLASS( weapon_crowbar, CCrowbar );

BEGIN_DATADESC( CCrowbar )
	DEFINE_FUNCTION( SwingAgain ),
	DEFINE_FUNCTION( Smack ),
END_DATADESC()

CCrowbar::CCrowbar()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CCrowbarWeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CCrowbar::Spawn()
{
	pev->classname = MAKE_STRING(CLASSNAME_STR(CROWBAR_CLASSNAME)); // hack to allow for old names
	Precache();
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	FallInit();// get ready to fall down.
}

void CCrowbar::Precache()
{
	PRECACHE_MODEL("models/v_crowbar.mdl");
	PRECACHE_MODEL("models/w_crowbar.mdl");
	PRECACHE_MODEL("models/p_crowbar.mdl");
	PRECACHE_SOUND("weapons/cbar_hit1.wav");
	PRECACHE_SOUND("weapons/cbar_hit2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");
}

void CCrowbar::Smack()
{
	CCrowbarWeaponContext *ctx = static_cast<CCrowbarWeaponContext*>(m_pWeaponContext.get());
	TraceResult *tr = &ctx->m_trHit;
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;
	DecalGunshot(tr, BULLET_PLAYER_CROWBAR, vecSrc, vecEnd);
}

void CCrowbar::SwingAgain()
{
	CCrowbarWeaponContext *ctx = static_cast<CCrowbarWeaponContext*>(m_pWeaponContext.get());
	ctx->Swing(false);
}
