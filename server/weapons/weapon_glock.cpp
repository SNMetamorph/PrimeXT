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

#include "weapon_glock.h"
#include "weapon_layer.h"
#include "weapons/glock.h"
#include "server_weapon_layer_impl.h"

LINK_ENTITY_TO_CLASS( weapon_9mmhandgun, CGlock );
LINK_ENTITY_TO_CLASS( weapon_glock, CGlock );

CGlock::CGlock()
{
	auto layerImpl = std::make_unique<CServerWeaponLayerImpl>(this);
	auto contextImpl = std::make_unique<CGlockWeaponContext>(std::move(layerImpl));
	m_pWeaponContext = std::move(contextImpl);
}

void CGlock::Spawn( )
{
	pev->classname = MAKE_STRING(CLASSNAME_STR(GLOCK_CLASSNAME)); // hack to allow for old names
	Precache( );
	SET_MODEL( edict(), "models/w_9mmhandgun.mdl" );
	FallInit();// get ready to fall down.
}

void CGlock::Precache( void )
{
	PRECACHE_MODEL("models/v_9mmhandgun.mdl");
	PRECACHE_MODEL("models/w_9mmhandgun.mdl");
	PRECACHE_MODEL("models/p_9mmhandgun.mdl");
	PRECACHE_MODEL("models/shell.mdl"); // brass shell

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("items/9mmclip2.wav");

	PRECACHE_SOUND("weapons/pl_gun1.wav"); //silenced handgun
	PRECACHE_SOUND("weapons/pl_gun2.wav"); //silenced handgun
	PRECACHE_SOUND("weapons/pl_gun3.wav"); //handgun
}
