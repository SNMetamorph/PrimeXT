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
#include "item_generic.h"

LINK_ENTITY_TO_CLASS(item_generic, CItemGeneric);

void CItemGeneric::Spawn( void )
{ 
	Precache( );
	SET_MODEL( edict(), GetModel( ));
	CItem::Spawn( );
}

void CItemGeneric::Precache( void )
{
	PRECACHE_MODEL( GetModel( ));
	if( pev->noise )
		PRECACHE_SOUND( (char*)STRING(pev->noise) );
}

BOOL CItemGeneric::MyTouch( CBasePlayer *pPlayer )
{
	if ( pPlayer->HasWeapon( WEAPON_SUIT ))
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev );
			if( pev->netname != NULL_STRING )
				WRITE_STRING( STRING( pev->netname ));
			else WRITE_STRING( STRING( pev->classname )); 			
		MESSAGE_END();
	}
	else if( FBitSet( pev->spawnflags, SF_ONLY_IF_IN_SUIT ))
		return FALSE;

	if( pev->noise )
		EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, STRING( pev->noise ), 1, ATTN_NORM );

	return TRUE;
}