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

#include "trigger_push.h"

LINK_ENTITY_TO_CLASS( trigger_push, CTriggerPush );

void CTriggerPush :: Activate( void )
{
	if( !FBitSet( pev->spawnflags, SF_TRIG_PUSH_SETUP ))
		return;	// already initialized

	ClearBits( pev->spawnflags, SF_TRIG_PUSH_SETUP );
	m_pGoalEnt = GetNextTarget();
}

/*QUAKED trigger_push (.5 .5 .5) ? TRIG_PUSH_ONCE
Pushes the player
*/

void CTriggerPush :: Spawn( )
{
	InitTrigger();

	if (pev->speed == 0)
		pev->speed = 100;

	if( pev->movedir == g_vecZero )
		pev->movedir = Vector( 1.0f, 0.0f, 0.0f );

	// this flag was changed and flying barrels on c2a5 stay broken
	if ( FStrEq( STRING( gpGlobals->mapname ), "c2a5" ) && pev->spawnflags & 4)
		pev->spawnflags |= SF_TRIG_PUSH_ONCE;

	if ( FBitSet (pev->spawnflags, SF_TRIGGER_PUSH_START_OFF) )// if flagged to Start Turned Off, make trigger nonsolid.
		pev->solid = SOLID_NOT;

	SetUse( &CBaseTrigger::ToggleUse );
	RelinkEntity(); // Link into the list

	// trying to find push target like in Quake III Arena
	SetBits( pev->spawnflags, SF_TRIG_PUSH_SETUP );
}


void CTriggerPush :: Touch( CBaseEntity *pOther )
{
	entvars_t* pevToucher = pOther->pev;

	if( pevToucher->solid == SOLID_NOT )
		return;

	// UNDONE: Is there a better way than health to detect things that have physics? (clients/monsters)
	switch( pevToucher->movetype )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:	// filter the movetype push here but pass the pushstep
	case MOVETYPE_NOCLIP:
	case MOVETYPE_FOLLOW:
		return;
	}

	// FIXME: If something is hierarchically attached, should we try to push the parent?
	if( pOther->m_hParent != NULL )
		return;

	if( !FNullEnt( m_pGoalEnt ))
	{
		if( pev->dmgtime >= gpGlobals->time )
			return;

		if( !pOther->IsPlayer() || pOther->pev->movetype != MOVETYPE_WALK )
			return;

		float	time, dist, f;
		Vector	origin, velocity;

		origin = Center();

		// assume m_pGoalEnt is valid
		time = sqrt (( m_pGoalEnt->pev->origin.z - origin.z ) / (0.5f * CVAR_GET_FLOAT( "sv_gravity" )));

		if( !time )
		{
			UTIL_Remove( this );
			return;
		}

		velocity = m_pGoalEnt->GetAbsOrigin() - origin;
		velocity.z = 0.0f;
		dist = velocity.Length();
		velocity = velocity.Normalize();

		f = dist / time;
		velocity *= f;
		velocity.z = time * CVAR_GET_FLOAT( "sv_gravity" );

		pOther->ApplyAbsVelocityImpulse( velocity );

		if( pOther->GetAbsVelocity().z > 0 )
			pOther->pev->flags &= ~FL_ONGROUND;

//		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "world/jumppad.wav", VOL_NORM, ATTN_IDLE );

		pev->dmgtime = gpGlobals->time + ( 2.0f * gpGlobals->frametime );

		if (FBitSet(pev->spawnflags, SF_TRIG_PUSH_ONCE))
			UTIL_Remove( this );
		return;
	}

	m_vecPushDir = EntityToWorldTransform().VectorRotate( pev->movedir );

	// Instant trigger, just transfer velocity and remove
	if (FBitSet(pev->spawnflags, SF_TRIG_PUSH_ONCE))
	{
		pOther->ApplyAbsVelocityImpulse( pev->speed * m_vecPushDir );
		if( pOther->m_iActorType == ACTOR_DYNAMIC )
			WorldPhysic->AddForce( pOther, pev->speed * m_vecPushDir * (1.0f / gpGlobals->frametime) * 0.5f );

		if( pOther->GetAbsVelocity().z > 0 )
			pOther->pev->flags &= ~FL_ONGROUND;
		UTIL_Remove( this );
	}
	else
	{	
		// Push field, transfer to base velocity
		Vector vecPush = (pev->speed * m_vecPushDir);

		if ( pevToucher->flags & FL_BASEVELOCITY )
			vecPush = vecPush + pOther->GetBaseVelocity();

		if( vecPush.z > 0 && FBitSet( pOther->pev->flags, FL_ONGROUND ))
		{
			pOther->pev->flags &= ~FL_ONGROUND;
			Vector origin = pOther->GetAbsOrigin();
			origin.z += 1.0f;
			pOther->SetAbsOrigin( origin );
		}

		if( pOther->m_iActorType == ACTOR_DYNAMIC )
			WorldPhysic->AddForce( pOther, vecPush * (1.0f / gpGlobals->frametime) * 0.5f );
		pOther->SetBaseVelocity( vecPush );
		pevToucher->flags |= FL_BASEVELOCITY;

//		ALERT( at_console, "Vel %f, base %f\n", pevToucher->velocity.z, pevToucher->basevelocity.z );
 	}
}
