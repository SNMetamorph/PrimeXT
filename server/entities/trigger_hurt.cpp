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

#include "trigger_hurt.h"
#include "player.h"

LINK_ENTITY_TO_CLASS( trigger_hurt, CTriggerHurt );

BEGIN_DATADESC( CTriggerHurt )
	DEFINE_FUNCTION( RadiationThink ),
END_DATADESC()

//
// trigger_hurt - hurts anything that touches it. if the trigger has a targetname, firing it will toggle state
//
//int gfToggleState = 0; // used to determine when all radiation trigger hurts have called 'RadiationThink'

void CTriggerHurt :: Spawn( void )
{
	InitTrigger();
	SetTouch( &CBaseTrigger::HurtTouch );

	if ( !FStringNull ( pev->targetname ) )
	{
		SetUse( &CBaseTrigger::ToggleUse );
	}
	else
	{
		SetUse( NULL );
	}

	if (m_bitsDamageInflict & DMG_RADIATION)
	{
		SetThink( &CTriggerHurt::RadiationThink );
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.0, 0.5); 
	}

	if ( FBitSet (pev->spawnflags, SF_TRIGGER_HURT_START_OFF) )// if flagged to Start Turned Off, make trigger nonsolid.
		pev->solid = SOLID_NOT;

	RelinkEntity(); // Link into the list
}

// trigger hurt that causes radiation will do a radius
// check and set the player's geiger counter level
// according to distance from center of trigger

void CTriggerHurt :: RadiationThink( void )
{

	edict_t *pentPlayer;
	CBasePlayer *pPlayer = NULL;
	float flRange;
	entvars_t *pevTarget;
	Vector vecSpot1;
	Vector vecSpot2;
	Vector vecRange;
	Vector origin;
	Vector view_ofs;

	// check to see if a player is in pvs
	// if not, continue	

	// set origin to center of trigger so that this check works
	origin = GetAbsOrigin();
	view_ofs = pev->view_ofs;

	SetAbsOrigin(( pev->absmin + pev->absmax ) * 0.5f );
	pev->view_ofs = g_vecZero;

	pentPlayer = FIND_CLIENT_IN_PVS( edict( ));

	SetAbsOrigin( origin );
	pev->view_ofs = view_ofs;

	// reset origin

	if( !FNullEnt( pentPlayer ))
	{
		pPlayer = GetClassPtr( (CBasePlayer *)VARS(pentPlayer));
		pevTarget = VARS( pentPlayer );

		// get range to player;

		vecSpot1 = (pev->absmin + pev->absmax) * 0.5f;
		vecSpot2 = (pevTarget->absmin + pevTarget->absmax) * 0.5f;
		
		vecRange = vecSpot1 - vecSpot2;
		flRange = vecRange.Length();

		// if player's current geiger counter range is larger
		// than range to this trigger hurt, reset player's
		// geiger counter range 

		if (pPlayer->m_flgeigerRange >= flRange)
			pPlayer->m_flgeigerRange = flRange;
	}

	SetNextThink( 0.25 );
}
