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
***/

#include "monster_furniture.h"

LINK_ENTITY_TO_CLASS( monster_furniture, CFurniture );

//=========================================================
// Furniture is killed
//=========================================================
void CFurniture :: Die ( void )
{
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// This used to have something to do with bees flying, but 
// now it only initializes moving furniture in scripted sequences
//=========================================================
void CFurniture :: Spawn( )
{
	PRECACHE_MODEL((char *)STRING(pev->model));
	SET_MODEL(ENT(pev),	STRING(pev->model));

	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_BBOX;
	pev->health = 80000;
	pev->takedamage = DAMAGE_AIM;
	pev->yaw_speed = 0;
	pev->sequence = 0;
	pev->frame = 0;

	ResetSequenceInfo( );
	pev->frame = 0;
	MonsterInit();
}

//=========================================================
// ID's Furniture as neutral (noone will attack it)
//=========================================================
int CFurniture::Classify ( void )
{
	return m_iClass ? m_iClass : CLASS_NONE;
}
