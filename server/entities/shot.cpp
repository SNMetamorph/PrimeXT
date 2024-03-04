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

#include "shot.h"

LINK_ENTITY_TO_CLASS( shot, CShot ); // enable save\restore

void CShot::Touch( CBaseEntity *pOther )
{
	if( pev->teleport_time > gpGlobals->time )
		return;

	// don't fire too often in collisions!
	// teleport_time is the soonest this can be touched again.
	pev->teleport_time = gpGlobals->time + 0.1;

	if( pev->netname )
		UTIL_FireTargets( pev->netname, this, this, USE_TOGGLE, 0 );
	if( pev->message && pOther != g_pWorld )
		UTIL_FireTargets( pev->message, pOther, this, USE_TOGGLE, 0 );
}
