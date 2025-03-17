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

#include "weapon_crossbow.h"
#include "weapon_layer.h"
#include "weapons/crossbow.h"
#include "server_weapon_layer_impl.h"

LINK_ENTITY_TO_CLASS( weapon_crossbow, CCrossbow );

BEGIN_DATADESC( CCrossbow )
	//DEFINE_FIELD( m_fInZoom, FIELD_BOOLEAN ),
	//DEFINE_FIELD( m_fZoomInUse, FIELD_BOOLEAN ),
END_DATADESC()

CCrossbow::CCrossbow()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	m_pWeaponContext = std::make_unique<CCrossbowWeaponContext>(std::move(layerImpl));
}

void CCrossbow::Spawn( void )
{
	pev->classname = MAKE_STRING(CLASSNAME_STR(CROSSBOW_CLASSNAME));
	Precache( );
	SET_MODEL(ENT(pev), "models/w_crossbow.mdl");
	FallInit();// get ready to fall down.
}

int CCrossbow::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_pWeaponContext->m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

void CCrossbow::Precache( void )
{
	PRECACHE_MODEL("models/w_crossbow.mdl");
	PRECACHE_MODEL("models/v_crossbow.mdl");
	PRECACHE_MODEL("models/p_crossbow.mdl");

	PRECACHE_SOUND("weapons/xbow_fire1.wav");
	PRECACHE_SOUND("weapons/xbow_reload1.wav");

	UTIL_PrecacheOther( "crossbow_bolt" );
}
