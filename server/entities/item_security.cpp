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
#include "item_security.h"

LINK_ENTITY_TO_CLASS(item_security, CItemSecurity);

void CItemSecurity::Spawn( void )
{ 
	Precache( );
	SET_MODEL(ENT(pev), "models/w_security.mdl");
	CItem::Spawn( );
}

void CItemSecurity::Precache( void )
{
	PRECACHE_MODEL ("models/w_security.mdl");
}
	
BOOL CItemSecurity::MyTouch( CBasePlayer *pPlayer )
{
	pPlayer->m_rgItems[ITEM_SECURITY] += 1;
	return TRUE;
}