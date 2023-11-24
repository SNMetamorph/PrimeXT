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
#include "item_battery.h"

LINK_ENTITY_TO_CLASS(item_battery, CItemBattery);

void CItemBattery::Spawn( void )
{ 
	Precache( );
	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/w_battery.mdl");
	CItem::Spawn( );
}

void CItemBattery::Precache( void )
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL ("models/w_battery.mdl");

	if (pev->noise)
		PRECACHE_SOUND( (char*)STRING(pev->noise) ); //LRC
	else
		PRECACHE_SOUND( "items/gunpickup2.wav" );
}

BOOL CItemBattery::MyTouch( CBasePlayer *pPlayer )
{
	if ( pPlayer->pev->deadflag != DEAD_NO )
	{
		return FALSE;
	}

	if ((pPlayer->pev->armorvalue < MAX_NORMAL_BATTERY) && pPlayer->HasWeapon( WEAPON_SUIT ))
	{
		int pct;
		char szcharge[64];

		pPlayer->pev->armorvalue += gSkillData.batteryCapacity;
		pPlayer->pev->armorvalue = Q_min(pPlayer->pev->armorvalue, MAX_NORMAL_BATTERY);

		if (pev->noise) EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, STRING(pev->noise), 1, ATTN_NORM ); //LRC
		else EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );

		MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev );
			WRITE_STRING( STRING(pev->classname) );
		MESSAGE_END();

			
		// Suit reports new power level
		// For some reason this wasn't working in release build -- round it.
		pct = (int)( (float)(pPlayer->pev->armorvalue * 100.0) * (1.0/MAX_NORMAL_BATTERY) + 0.5);
		pct = (pct / 5);
		if (pct > 0)
			pct--;
		
		sprintf( szcharge,"!HEV_%1dP", pct );
			
		//EMIT_SOUND_SUIT(ENT(pev), szcharge);
		pPlayer->SetSuitUpdate(szcharge, FALSE, SUIT_NEXT_IN_30SEC);
		return TRUE;		
	}
	return FALSE;
}