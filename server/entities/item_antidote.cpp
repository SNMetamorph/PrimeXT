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
#include "item_antidote.h"

LINK_ENTITY_TO_CLASS(item_antidote, CItemAntidote);

void CItemAntidote::Spawn( void )
{ 
	Precache( );
	SET_MODEL(ENT(pev), "models/w_antidote.mdl");
	CItem::Spawn( );
}

void CItemAntidote::Precache( void )
{
	PRECACHE_MODEL ("models/w_antidote.mdl");
}

BOOL CItemAntidote::MyTouch( CBasePlayer *pPlayer )
{
	pPlayer->SetSuitUpdate("!HEV_DET4", FALSE, SUIT_NEXT_IN_1MIN);
		
	pPlayer->m_rgItems[ITEM_ANTIDOTE] += 1;
	return TRUE;
}