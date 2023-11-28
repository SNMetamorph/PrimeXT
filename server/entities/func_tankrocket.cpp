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
#include "func_tankrocket.h"

LINK_ENTITY_TO_CLASS( func_tankrocket, CFuncTankRocket );

void CFuncTankRocket :: Precache( void )
{
	UTIL_PrecacheOther( "rpg_rocket" );
	BaseClass::Precache();

	m_bulletType = TANK_BULLET_OTHER;
}

void CFuncTankRocket :: Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;

		if( bulletCount > 0 )
		{
			for( int i = 0; i < bulletCount; i++ )
			{
				CBaseEntity *pRocket = CBaseEntity::Create( "rpg_rocket", barrelEnd, GetAbsAngles(), edict() );
			}
			BaseClass::Fire( barrelEnd, forward, pev );
		}
	}
	else
	{
		BaseClass::Fire( barrelEnd, forward, pev );
	}
}