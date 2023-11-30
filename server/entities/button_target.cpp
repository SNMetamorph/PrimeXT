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

#include "button_target.h"

LINK_ENTITY_TO_CLASS( button_target, CButtonTarget );

void CButtonTarget :: Spawn( void )
{
	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;

	SET_MODEL( edict(), GetModel() );
	pev->takedamage = DAMAGE_YES;

	if( FBitSet( pev->spawnflags, SF_BUTTON_TARGET_ON ))
	{
		m_iState = STATE_ON;
		pev->frame = 1;
	}
	else
	{
		m_iState = STATE_OFF;
		pev->frame = 0;
	}

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
}

void CButtonTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !ShouldToggle( useType ))
		return;

	pev->frame = !pev->frame;

	if( pev->frame )
	{
		m_iState = STATE_ON;
		SUB_UseTargets( pActivator, USE_ON, 0 );
	}
	else
	{
		m_iState = STATE_OFF;
		SUB_UseTargets( pActivator, USE_OFF, 0 );
	}
}

int CButtonTarget :: ObjectCaps( void )
{
	int caps = (BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION));

	if( FBitSet( pev->spawnflags, SF_BUTTON_TARGET_USE ))
		return caps | FCAP_IMPULSE_USE;
	return caps;
}


int CButtonTarget :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	Use( Instance( pevAttacker ), this, USE_TOGGLE, 0 );

	return 1;
}