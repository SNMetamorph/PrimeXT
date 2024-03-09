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

#include "trigger_impulse.h"

LINK_ENTITY_TO_CLASS( trigger_impulse, CTriggerImpulse );

void CTriggerImpulse :: Spawn( void )
{
	// apply default name
	if( FStringNull( pev->targetname ))
		pev->targetname = MAKE_STRING( "game_firetarget" );
}

void CTriggerImpulse::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int iImpulse = (int)value;

	if( IsLockedByMaster( pActivator ))
		return;

	// run custom filter for entity
	if( !pev->impulse || ( pev->impulse == iImpulse ))
	{
		UTIL_FireTargets( STRING(pev->target ), pActivator, this, USE_TOGGLE, value );
	}
}
