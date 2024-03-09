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

#include "func_ladder.h"

LINK_ENTITY_TO_CLASS( func_ladder, CLadder );

void CLadder :: Precache( void )
{
	// Do all of this in here because we need to 'convert' old saved games
	pev->solid = SOLID_NOT;
	pev->skin = CONTENTS_LADDER;

	if( !CVAR_GET_FLOAT( "showtriggers" ))
	{
		pev->rendermode = kRenderTransTexture;
		pev->renderamt = 0;
	}

	pev->effects &= ~EF_NODRAW;
}

void CLadder :: Spawn( void )
{
	Precache();

	SET_MODEL( edict(), GetModel( )); // set size and link into world
	pev->movetype = MOVETYPE_PUSH;
	SetBits( m_iFlags, MF_LADDER );
}
