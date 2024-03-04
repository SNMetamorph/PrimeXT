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

#include "item_sodacan.h"

LINK_ENTITY_TO_CLASS( item_sodacan, CItemSoda );

BEGIN_DATADESC( CItemSoda )
	DEFINE_FUNCTION( CanThink ),
	DEFINE_FUNCTION( CanTouch ),
END_DATADESC()

void CItemSoda :: Precache ( void )
{
	PRECACHE_MODEL( "models/can.mdl" );
}

void CItemSoda::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	if( WorldPhysic->Initialized( ))
		pev->movetype = MOVETYPE_PHYSIC;
	else pev->movetype = MOVETYPE_TOSS;

	SET_MODEL ( ENT(pev), "models/can.mdl" );
	UTIL_SetSize ( pev, Vector ( 0, 0, 0 ), Vector ( 0, 0, 0 ) );
	
	SetThink( &CItemSoda::CanThink);
	pev->nextthink = gpGlobals->time + 0.5;

	m_pUserData = WorldPhysic->CreateBodyFromEntity( this );
}

void CItemSoda::CanThink ( void )
{
	EMIT_SOUND (ENT(pev), CHAN_WEAPON, "weapons/g_bounce3.wav", 1, ATTN_NORM );

	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize ( pev, Vector ( -8, -8, 0 ), Vector ( 8, 8, 8 ) );
	SetThink( NULL );
	SetTouch( &CItemSoda::CanTouch );
}

void CItemSoda::CanTouch ( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	// spoit sound here

	pOther->TakeHealth( 1, DMG_GENERIC );// a bit of health.

	if ( !FNullEnt( pev->owner ) )
	{
		// tell the machine the can was taken
		pev->owner->v.frags = 0;
	}

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}
