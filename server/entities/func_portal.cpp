/*
func_monitor.cpp - realtime monitors and portals
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "func_portal.h"

// =================== FUNC_PORTAL ==============================================

LINK_ENTITY_TO_CLASS( func_portal, CFuncPortal );

void CFuncPortal :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_PORTAL;

	// semisolid portals looks ugly. Disabled
	pev->rendermode = kRenderNormal;
	pev->renderamt = 255;

	SET_MODEL( edict(), GetModel() );

	if( !UTIL_CanRotate( this ))
	{
		ALERT( at_error, "Entity %s [%i] with model %s missed 'origin' brush.\n", GetClassname(), entindex(), GetModel());
		ALERT( at_error, "The portals can't properly working without them. Removed\n" );
		UTIL_Remove( this );
		return;
	}

	SetThink( &CFuncMonitor::VisThink );

	if( FBitSet( pev->spawnflags, SF_PORTAL_START_OFF ))
	{
		m_iState = STATE_OFF;
		pev->effects |= EF_NODRAW;
		DontThink();
	}
	else
	{
		m_iState = STATE_ON;
		pev->effects &= ~EF_NODRAW;
		SetCameraVisibility( TRUE );
		SetNextThink( 0.1f );
	}
}

void CFuncPortal :: Activate( void )
{
	ChangeCamera( pev->target );
}

void CFuncPortal :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "firetarget" ))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CFuncPortal :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	if( ShouldToggle( useType ))
	{
		if( m_iState == STATE_ON )
		{
			SetCameraVisibility( FALSE );
			pev->effects |= EF_NODRAW;
			m_iState = STATE_OFF;
			DontThink();
		}
		else if( m_iState == STATE_OFF )
		{
			SetCameraVisibility( TRUE );
			pev->effects &= ~EF_NODRAW;
			m_iState = STATE_ON;
			SetNextThink( 0.1f );
		}
	}
}

void CFuncPortal :: Touch( CBaseEntity *pOther )
{
	CBaseEntity *pTarget = NULL;

	if( IsLockedByMaster( pOther ))
		return;

	if( m_iState == STATE_OFF || m_flDisableTime > gpGlobals->time )
		return; // disabled

	if( pOther->m_iTeleportFilter )
		return;	// we already teleporting somewhere

	// Only teleport monsters or clients or physents
	if( !pOther->IsPlayer() && !pOther->IsMonster() && !pOther->IsPushable() && !pOther->IsProjectile( ) && !pOther->IsRigidBody())
		return;

	pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ));
	if( !pTarget ) return;

	// disable portal to avoid recursion
	m_flDisableTime = gpGlobals->time + 0.5f;

	if( FBitSet( pev->effects, EF_PORTAL ))
	{
		// Build a this --> remote transformation
		matrix4x4 matMyModelToWorld = EntityToWorldTransform();

		// for prevent two-side portals recursion
		if( DotProduct( matMyModelToWorld.GetForward(), pOther->GetAbsVelocity().Normalize()) <= 0.0f )
			return;	// bad direction

		// Teleport our object
		matrix4x4 matRemotePortalTransform = pTarget->EntityToWorldTransform();
		Vector ptNewOrigin, vLook, vNewLook, vVelocity;

		if ( pOther->IsPlayer() )
		{
			UTIL_MakeVectorsPrivate( pOther->pev->v_angle, vLook, NULL, NULL );
		}
		else
		{
			pOther->GetVectors( &vLook, NULL, NULL );
		}

		// Move origin
		ptNewOrigin = matMyModelToWorld.VectorITransform( pOther->GetAbsOrigin() );
		ptNewOrigin = matRemotePortalTransform.VectorTransform( ptNewOrigin );

		TraceResult tr;

//		UTIL_TraceHull( ptNewOrigin, ptNewOrigin, dont_ignore_monsters, head_hull, pOther->edict(), &tr );
		UTIL_TraceEntity( pOther, ptNewOrigin, ptNewOrigin, &tr );

		// make sure what is enough space here
		if( tr.fAllSolid || tr.fStartSolid )
		{
			ALERT( at_aiconsole, "No free space\n" );
			pOther->SetAbsVelocity( g_vecZero );	// g-cont. test thing
			return;
		}

		// Re-aim camera
		vNewLook = matMyModelToWorld.VectorIRotate( vLook );
		vNewLook = matRemotePortalTransform.VectorRotate( vNewLook );

		// Reorient the physics
		vVelocity = matMyModelToWorld.VectorIRotate( pOther->GetAbsVelocity() );
		vVelocity = matRemotePortalTransform.VectorRotate( vVelocity );

		Vector qNewAngles = UTIL_VecToAngles( vNewLook );
		
		if( pOther->IsPlayer() )
		{
			pOther->pev->punchangle.z = pTarget->GetAbsAngles().z; // apply ROLL too
		}

		pOther->Teleport( &ptNewOrigin, &qNewAngles, &vVelocity );
	}
	else
	{
		Vector tmp = pTarget->GetAbsOrigin();
		Vector pAngles = pTarget->GetAbsAngles();

		if( pOther->IsPlayer( ))
			tmp.z -= pOther->pev->mins.z; // make origin adjustments
		tmp.z++;

		pOther->Teleport( &tmp, &pAngles, &g_vecZero );
	}
          
	ChangeCamera( pev->target ); // update PVS
	pOther->pev->flags &= ~FL_ONGROUND;

	if( FBitSet( pev->spawnflags, SF_PORTAL_CLIENTONLYFIRE ) && !pOther->IsPlayer() )
		return;

	UTIL_FireTargets( pev->netname, pOther, this, USE_TOGGLE ); // fire target
}

// =================== INFO_PORTAL_DESTINATION ==============================================

LINK_ENTITY_TO_CLASS( info_portal_destination, CInfoPortalDest );

void CInfoPortalDest :: Spawn( void )
{
	PRECACHE_MODEL( "sprites/null.spr" );
	SET_MODEL( edict(), "sprites/null.spr" );

	SetBits( m_iFlags, MF_POINTENTITY );
}
