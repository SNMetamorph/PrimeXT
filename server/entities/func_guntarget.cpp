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

#include "func_guntarget.h"

LINK_ENTITY_TO_CLASS( func_guntarget, CGunTarget );

BEGIN_DATADESC( CGunTarget )
	DEFINE_FIELD( m_on, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( Next ),
	DEFINE_FUNCTION( Wait ),
	DEFINE_FUNCTION( Start ),
END_DATADESC()

void CGunTarget::Spawn( void )
{
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
	SET_MODEL( edict(), GetModel() );
	RelinkEntity( TRUE );

	if ( pev->speed == 0 )
		pev->speed = 100;

	// Don't take damage until "on"
	pev->takedamage = DAMAGE_NO;
	pev->flags |= FL_MONSTER;

	m_on = FALSE;
	pev->max_health = pev->health;

	if( pev->spawnflags & FGUNTARGET_START_ON )
	{
		SetMoveDone( &CGunTarget::Start );
		SetMoveDoneTime( 0.3 );
	}
}

void CGunTarget :: Activate( void )
{
	CBaseEntity *pTarg;

	// now find our next target
	pTarg = GetNextTarget();

	if( pTarg )
	{
		m_hTargetEnt = pTarg;
		UTIL_SetOrigin( this, pTarg->GetLocalOrigin() - (pev->mins + pev->maxs) * 0.5 );
	}
}

void CGunTarget::Start( void )
{
	Use( this, this, USE_ON, 0 );
}

void CGunTarget::Next( void )
{
	SetThink( NULL );

	m_hTargetEnt = GetNextTarget();
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if( !pTarget )
	{
		Stop();
		return;
	}

	SetMoveDone( &CGunTarget::Wait );
	LinearMove( pTarget->GetLocalOrigin() - (pev->mins + pev->maxs) * 0.5, pev->speed );
}

void CGunTarget::Wait( void )
{
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if ( !pTarget )
	{
		Stop();
		return;
	}

	// Fire the pass target if there is one
	if ( pTarget->pev->message )
	{
		UTIL_FireTargets( STRING(pTarget->pev->message), this, this, USE_TOGGLE, 0 );
		if ( FBitSet( pTarget->pev->spawnflags, SF_CORNER_FIREONCE ) )
			pTarget->pev->message = 0;
	}
		
	m_flWait = pTarget->GetDelay();

	pev->target = pTarget->pev->target;
	SetMoveDone( &CGunTarget::Next );

	if( m_flWait != 0 )
	{
		// -1 wait will wait forever!		
		SetMoveDoneTime( m_flWait );
	}
	else
	{
		Next();// do it RIGHT now!
	}
}

void CGunTarget::Stop( void )
{
	SetLocalVelocity( g_vecZero );
	SetMoveDoneTime( -1 );
	pev->takedamage = DAMAGE_NO;
}

int CGunTarget::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pev->health > 0 )
	{
		pev->health -= flDamage;
		if ( pev->health <= 0 )
		{
			pev->health = 0;
			Stop();
			if ( pev->message )
				UTIL_FireTargets( STRING(pev->message), this, this, USE_TOGGLE, 0 );
		}
	}
	return 0;
}

void CGunTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_on ) )
		return;

	if ( m_on )
	{
		Stop();
	}
	else
	{
		pev->takedamage = DAMAGE_AIM;
		m_hTargetEnt = GetNextTarget();
		if ( m_hTargetEnt == NULL )
			return;
		pev->health = pev->max_health;
		Next();
	}
}
