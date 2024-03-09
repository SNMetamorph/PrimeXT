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

#include "trigger_camera.h"
#include "player.h"
#include "trains.h"

LINK_ENTITY_TO_CLASS( trigger_camera, CTriggerCamera );

// Global Savedata for changelevel friction modifier
BEGIN_DATADESC( CTriggerCamera )
	DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pPath, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_sPath, FIELD_STRING, "moveto" ),
	DEFINE_FIELD( m_flReturnTime, FIELD_TIME ),
	DEFINE_FIELD( m_flStopTime, FIELD_TIME ),
	DEFINE_FIELD( m_moveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_targetSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_initialSpeed, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_acceleration, FIELD_FLOAT, "acceleration" ),
	DEFINE_KEYFIELD( m_deceleration, FIELD_FLOAT, "deceleration" ),
	DEFINE_KEYFIELD( m_iszViewEntity, FIELD_STRING, "m_iszViewEntity" ),
	DEFINE_FIELD( m_state, FIELD_INTEGER ),
	DEFINE_FUNCTION( FollowTarget ),
END_DATADESC()

void CTriggerCamera::Spawn( void )
{
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;		// Remove model & collisions
	pev->renderamt = 0;			// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;

	m_initialSpeed = pev->speed;

	if( m_acceleration == 0 )
		m_acceleration = 500;
	if( m_deceleration == 0 )
		m_deceleration = 500;
}

void CTriggerCamera :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "moveto" ))
	{
		m_sPath = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "acceleration" ))
	{
		m_acceleration = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "deceleration" ))
	{
		m_deceleration = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszViewEntity"))
	{
		m_iszViewEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue( pkvd );
	}
}

void CTriggerCamera::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_state ) )
		return;

	// Toggle state
	m_state = !m_state;

	if( m_state == 0 )
	{
		Stop();
		return;
	}

	if( !pActivator || !pActivator->IsPlayer() )
	{
		pActivator = UTIL_PlayerByIndex( 1 );
	}
		
	m_hPlayer = pActivator;

	m_flReturnTime = gpGlobals->time + m_flWait;
	pev->speed = m_initialSpeed;
	m_targetSpeed = m_initialSpeed;

	if (FBitSet (pev->spawnflags, SF_CAMERA_PLAYER_TARGET ) )
	{
		m_hTarget = m_hPlayer;
	}
	else
	{
		m_hTarget = GetNextTarget();
	}

	// Nothing to look at!
	if( m_hTarget == NULL )
	{
		ALERT( at_error, "%s couldn't find target %s\n", GetClassname(), GetTarget( ));
		return;
	}

	if( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL ))
	{
		((CBasePlayer *)pActivator)->EnableControl( FALSE );
	}

	if( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_HIDEHUD ))
	{
		((CBasePlayer *)pActivator)->m_iHideHUD |= HIDEHUD_ALL;
	}

	if ( m_sPath )
	{
		m_pPath = UTIL_FindEntityByTargetname( NULL, STRING( m_sPath ));
	}
	else
	{
		m_pPath = NULL;
	}

	m_flStopTime = gpGlobals->time;
	if ( m_pPath )
	{
		if ( m_pPath->pev->speed != 0 )
			m_targetSpeed = m_pPath->pev->speed;
		
		m_flStopTime += m_pPath->GetDelay();
	}

	// copy over player information
	if (FBitSet (pev->spawnflags, SF_CAMERA_PLAYER_POSITION ) )
	{
		UTIL_SetOrigin( this, pActivator->EyePosition() );
		Vector vecAngles;
		vecAngles.x = -pActivator->GetAbsAngles().x;
		vecAngles.y = pActivator->GetAbsAngles().y;
		vecAngles.z = 0;
		SetLocalVelocity( pActivator->GetAbsVelocity( ));
		SetLocalAngles( vecAngles );
	}
	else
	{
		SetLocalVelocity( g_vecZero );
	}

	if (m_iszViewEntity)
	{
		CBaseEntity *pEntity = UTIL_FindEntityByTargetname( NULL, STRING( m_iszViewEntity ));

		if( pEntity )
		{
			SET_VIEW( pActivator->edict(), pEntity->edict() );
		}
		else
		{
			ALERT( at_error, "%s couldn't find view entity %s\n", GetClassname(), STRING( m_iszViewEntity ));
			SET_VIEW( pActivator->edict(), edict() );
			SET_MODEL( edict(), pActivator->GetModel( ));
		}
	}
	else
	{
		SET_VIEW( pActivator->edict(), edict() );
		SET_MODEL( edict(), pActivator->GetModel( ));
	}

	// follow the player down
	SetThink( &CTriggerCamera::FollowTarget );
	SetNextThink( 0 );

	m_moveDistance = 0;
	Move();
}

void CTriggerCamera::FollowTarget( void )
{
	if( m_hPlayer == NULL )
		return;

	if( m_hTarget == NULL || (m_hTarget->IsMonster() && !m_hTarget->IsAlive())
	|| !m_hPlayer->IsAlive() || ( m_flWait != -1 && m_flReturnTime < gpGlobals->time ))
	{
		Stop();
		return;
	}

	// update target, maybe it was changed by trigger_changetarget
	if (!(pev->spawnflags & SF_CAMERA_PLAYER_TARGET)) {
		m_hTarget = GetNextTarget();
	}

	Vector vecGoal = UTIL_VecToAngles( m_hTarget->GetAbsOrigin() - GetLocalOrigin() );
	Vector angles = GetLocalAngles();

	if (angles.y > 360)
		angles.y -= 360;

	if (angles.y < 0)
		angles.y += 360;

	SetLocalAngles( angles );

	float dx = vecGoal.x - angles.x;
	float dy = vecGoal.y - angles.y;

	if (dx < -180) 
		dx += 360;
	if (dx > 180) 
		dx = dx - 360;
	
	if (dy < -180) 
		dy += 360;
	if (dy > 180) 
		dy = dy - 360;

	Vector vecAvel;

	vecAvel.x = dx * 40 * gpGlobals->frametime;
	vecAvel.y = dy * 40 * gpGlobals->frametime;
	vecAvel.z = 0;

	SetLocalAvelocity( vecAvel );

	if( !( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL )))	
	{
		SetLocalVelocity( GetLocalVelocity() * 0.8f );
		if( GetLocalVelocity().Length( ) < 10.0f )
			SetLocalVelocity( g_vecZero );
	}

	SetNextThink( 0 );

	Move();
}

void CTriggerCamera :: Stop( void )
{
	if( m_hPlayer != NULL )
	{
		SET_VIEW( m_hPlayer->edict(), m_hPlayer->edict() );
		((CBasePlayer *)((CBaseEntity *)m_hPlayer))->EnableControl( TRUE );
	}

	if( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_HIDEHUD ))
	{
		((CBasePlayer *)((CBaseEntity *)m_hPlayer))->m_iHideHUD &= ~HIDEHUD_ALL;
	}

	SUB_UseTargets( this, USE_TOGGLE, 0 );
	SetLocalVelocity( g_vecZero );
	SetLocalAvelocity( g_vecZero );
	DontThink();
	m_state = 0;
}

void CTriggerCamera::Move( void )
{
	// Not moving on a path, return
	if (!m_pPath)
		return;

	// Subtract movement from the previous frame
	m_moveDistance -= pev->speed * gpGlobals->frametime;

	// Have we moved enough to reach the target?
	if ( m_moveDistance <= 0 )
	{
		// Fire the passtarget if there is one
		if ( m_pPath->pev->message )
		{
			UTIL_FireTargets( STRING(m_pPath->pev->message), this, this, USE_TOGGLE, 0 );
			if ( FBitSet( m_pPath->pev->spawnflags, SF_CORNER_FIREONCE ) )
				m_pPath->pev->message = 0;
		}
		// Time to go to the next target
		m_pPath = m_pPath->GetNextTarget();

		// Set up next corner
		if ( !m_pPath )
		{
			SetLocalVelocity( g_vecZero );
		}
		else 
		{
			if ( m_pPath->pev->speed != 0 )
				m_targetSpeed = m_pPath->pev->speed;

			Vector delta = m_pPath->GetLocalOrigin() - GetLocalOrigin();
			m_moveDistance = delta.Length();
			pev->movedir = delta.Normalize();
			m_flStopTime = gpGlobals->time + m_pPath->GetDelay();
		}
	}

	if( m_flStopTime > gpGlobals->time )
		pev->speed = UTIL_Approach( 0, pev->speed, m_deceleration * gpGlobals->frametime );
	else
		pev->speed = UTIL_Approach( m_targetSpeed, pev->speed, m_acceleration * gpGlobals->frametime );

	float fraction = 2 * gpGlobals->frametime;
	SetLocalVelocity(((pev->movedir * pev->speed) * fraction) + (GetLocalVelocity() * ( 1.0f - fraction )));

	RelinkEntity();
}
