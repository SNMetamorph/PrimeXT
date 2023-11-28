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

#include "func_monitor.h"

LINK_ENTITY_TO_CLASS( func_monitor, CFuncMonitor );

BEGIN_DATADESC( CFuncMonitor )
	DEFINE_FIELD( m_pController, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_vecControllerUsePos, FIELD_VECTOR ),
	DEFINE_FIELD( m_flDisableTime, FIELD_TIME ),
	DEFINE_FUNCTION( VisThink ),
END_DATADESC()

void CFuncMonitor::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "camera" ))
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "fov" ))
	{
		pev->fuser2 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CFuncMonitor :: Spawn( void )
{
	pev->movetype = MOVETYPE_PUSH;

	if( FBitSet( pev->spawnflags, SF_MONITOR_PASSABLE ))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;

	SET_MODEL( edict(), GetModel() );
	pev->effects |= EF_SCREEN;
	m_iState = STATE_OFF;

	SetLocalAngles( m_vecTempAngles );
	UTIL_SetOrigin( this, GetLocalOrigin( ));

	if( FBitSet( pev->spawnflags, SF_MONITOR_MONOCRHOME ))
		pev->iuser1 |= CF_MONOCHROME;

	// enable monitor
	if( FBitSet( pev->spawnflags, SF_MONITOR_START_ON ))
	{
		SetThink( &CBaseEntity::SUB_CallUseToggle );
		SetNextThink( 0.1 );
	}
}

void CFuncMonitor::Activate( void )
{
	ChangeCamera( pev->target );
}

void CFuncMonitor :: StartMessage( CBasePlayer *pPlayer )
{
	// dr.Tressi request: camera who attached with player
	if( FStrEq( STRING( pev->target ), "*player" ) && !g_pGameRules->IsMultiplayer())
		ChangeCamera( pev->target );
}

int CFuncMonitor :: ObjectCaps( void ) 
{
	if( FBitSet( pev->spawnflags, SF_MONITOR_USEABLE ))
		return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE | FCAP_HOLD_ANGLES;
	return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION)| FCAP_HOLD_ANGLES;
}

void CFuncMonitor::ChangeCamera( string_t newcamera )
{
	CBaseEntity *pCamera = UTIL_FindEntityByTargetname( NULL, STRING( newcamera ));
	CBaseEntity *pOldCamera = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ));

	if( pOldCamera )
          {
		// now old camera can't merge PVS anymore
		ClearBits( pOldCamera->pev->effects, EF_MERGE_VISIBILITY );
		pev->aiment = NULL;
	}
         
	if( pCamera )
          {
		// update the visible state
		if( m_iState == STATE_ON )
			SetBits( pCamera->pev->effects, EF_MERGE_VISIBILITY );
		pev->sequence = pCamera->entindex();
		pev->aiment = pCamera->edict(); // tell engine about portal entity with camera
	}

	pev->target = newcamera;
}

void CFuncMonitor :: SetCameraVisibility( bool fEnable )
{
	// set camera PVS
	if( pev->sequence > 0 )
	{
		CBaseEntity *pTarget = CBaseEntity::Instance( INDEXENT( pev->sequence ));
		if( !pTarget ) return;

		// allow camera merge visibility for enabled or disabled portals
		if( fEnable ) SetBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
		else ClearBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
	}
}

bool CFuncMonitor :: AllowToFindClientInPVS( CBaseEntity *pTarget )
{
	if( !pTarget ) return false;	// no target

	if( pTarget->IsPlayer() )	// target is player
		return false;

	CBaseEntity *pParent = pTarget->GetRootParent();

	if( pParent->IsPlayer() )	// target is attached to player
		return false;	

	return true; // non-client camera
}

void CFuncMonitor :: VisThink( void )
{
	if( pev->sequence != 0 )
	{
		CBaseEntity *pTarget = CBaseEntity::Instance( INDEXENT( pev->sequence ));

		// no reason to find in PVS himself
		if( AllowToFindClientInPVS( pTarget ))
		{
			if( FNullEnt( FIND_CLIENT_IN_PVS( edict() )))
				ClearBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
			else SetBits( pTarget->pev->effects, EF_MERGE_VISIBILITY );
		}
	}

	SetNextThink( 0.1 );
}

void CFuncMonitor :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	if( ShouldToggle( useType ))
	{
		if(( useType == USE_SET || useType == USE_RESET ) && pActivator->IsPlayer() && FBitSet( pev->spawnflags, SF_MONITOR_USEABLE ))
		{
			if( useType == USE_SET )
			{
				if( m_iState == STATE_ON )
				{
					UTIL_SetView( pActivator, pev->target );
					m_pController = (CBasePlayer *)pActivator;
					if( FBitSet( pev->spawnflags, SF_MONITOR_HIDEHUD ))
						m_pController->m_iHideHUD |= HIDEHUD_ALL;
					m_pController->m_pMonitor = this;

					// remember where the player's standing, so we can tell when he walks away
					if( m_hParent != NULL && FClassnameIs( m_hParent->pev, "func_tracktrain" ))
					{
						// transform controller pos into local space because parent can be moving
						m_vecControllerUsePos = m_pController->EntityToWorldTransform().VectorITransform( m_pController->GetAbsOrigin() );
					}
					else m_vecControllerUsePos = m_pController->GetAbsOrigin();
				}
			}
			else if( useType == USE_RESET )
			{
				if( FBitSet( pev->spawnflags, SF_MONITOR_HIDEHUD ))
					((CBasePlayer *)pActivator)->m_iHideHUD &= ~HIDEHUD_ALL;
				UTIL_SetView( pActivator );
			}
			return;
		}

		pev->body = !pev->body;
		m_iState = (pev->body) ? STATE_ON : STATE_OFF;

		SetThink( &CFuncMonitor::VisThink );

		if( pev->body )
		{
			SetCameraVisibility( TRUE );
			SetNextThink( 0.1f );
		}
		else
		{
			SetCameraVisibility( FALSE );
			DontThink();
		}
	}
}

BOOL CFuncMonitor :: OnControls( CBaseEntity *pTest )
{
	if( m_hParent != NULL && FClassnameIs( m_hParent->pev, "func_tracktrain" ))
	{
		// transform local controller pos through tankcontrols pos
		Vector vecTransformedControlerUsePos = EntityToWorldTransform().VectorTransform( m_vecControllerUsePos );
		if(( vecTransformedControlerUsePos - pTest->GetAbsOrigin() ).Length() < 30 )
		{
			return TRUE;
		}
	}
	else if(( m_vecControllerUsePos - pTest->GetAbsOrigin() ).Length() < 30 )
	{
		return TRUE;
	}

	return FALSE;
}
