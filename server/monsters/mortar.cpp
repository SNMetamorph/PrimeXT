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
/*

===== mortar.cpp ========================================================

  the "LaBuznik" mortar device              

*/

#include "mortar.h"

LINK_ENTITY_TO_CLASS( monster_mortar, CMortar );

BEGIN_DATADESC( CMortar )
	DEFINE_FUNCTION( MortarExplode ),
END_DATADESC()

void CMortar::Spawn( )
{
	pev->movetype	= MOVETYPE_NONE;
	pev->solid		= SOLID_NOT;

	pev->dmg		= 200;

	SetThink( &CMortar::MortarExplode );
	pev->nextthink = 0;

	Precache( );


}

void CMortar::Precache( )
{
	m_spriteTexture = PRECACHE_MODEL( "sprites/lgtning.spr" );
}

void CMortar::MortarExplode( void )
{
	Vector absOrigin = GetAbsOrigin();

	// mortar beam
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( absOrigin.x );
		WRITE_COORD( absOrigin.y );
		WRITE_COORD( absOrigin.z );
		WRITE_COORD( absOrigin.x );
		WRITE_COORD( absOrigin.y );
		WRITE_COORD( absOrigin.z + 1024 );
		WRITE_SHORT(m_spriteTexture );
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 1 ); // life
		WRITE_BYTE( 40 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 160 );   // r, g, b
		WRITE_BYTE( 100 );   // r, g, b
		WRITE_BYTE( 128 );	// brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();

	TraceResult tr;
	UTIL_TraceLine( absOrigin + Vector( 0, 0, 1024 ), absOrigin - Vector( 0, 0, 1024 ), dont_ignore_monsters, edict(), &tr );

	Explode( &tr, DMG_BLAST | DMG_MORTAR );
	UTIL_ScreenShake( tr.vecEndPos, 25.0, 150.0, 1.0, 750 );
}
