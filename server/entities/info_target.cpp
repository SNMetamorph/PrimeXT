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

#include "info_target.h"

#define SF_TARGET_HACK_VISIBLE	BIT( 0 )

LINK_ENTITY_TO_CLASS( info_target, CInfoTarget );

void CInfoTarget :: Precache( void )
{
	if( FBitSet( pev->spawnflags, SF_TARGET_HACK_VISIBLE ))
		PRECACHE_MODEL( "sprites/null.spr" );
}

void CInfoTarget :: Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	SetBits( m_iFlags, MF_POINTENTITY );

	if( FBitSet( pev->spawnflags, SF_TARGET_HACK_VISIBLE ))
	{
		SET_MODEL( edict(), "sprites/null.spr" );
		UTIL_SetSize( pev, g_vecZero, g_vecZero );
	}
}
