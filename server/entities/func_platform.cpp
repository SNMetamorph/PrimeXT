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

#include "func_platform.h"

// UNDONE: Need to save this!!! It needs class & linkage
class CPlatTrigger : public CBaseEntity
{
	DECLARE_CLASS( CPlatTrigger, CBaseEntity );
public:
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }
	void SpawnInsideTrigger( CFuncPlat *pPlatform );
	void Touch( CBaseEntity *pOther );
	CFuncPlat *m_pPlatform;
};

LINK_ENTITY_TO_CLASS( func_plat, CFuncPlat );
LINK_ENTITY_TO_CLASS( func_platform, CFuncPlat );	// a elevator

BEGIN_DATADESC( CFuncPlat )
	DEFINE_FUNCTION( PlatUse ),
	DEFINE_FUNCTION( FloorCalc ),
	DEFINE_FUNCTION( CallGoUp ),
	DEFINE_FUNCTION( CallGoDown ),
	DEFINE_FUNCTION( CallHitTop ),
	DEFINE_FUNCTION( CallHitBottom ),
	DEFINE_FUNCTION( CallHitFloor ),
END_DATADESC()

static void PlatSpawnInsideTrigger( entvars_t* pevPlatform )
{
	GetClassPtr(( CPlatTrigger *)NULL)->SpawnInsideTrigger( GetClassPtr(( CFuncPlat *)pevPlatform ));
}

/*QUAKED func_plat (0 .5 .8) ? SF_PLAT_TOGGLE
speed	default 150

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out disabled in
the extended position until it is trigger, when it will lower and become a normal plat.

If the "height" key is set, that will determine the amount the plat moves, instead of
being implicitly determined by the model's height.

Set "sounds" to one of the following:
1) base fast
2) chain slow
*/

void CFuncPlat :: Setup( void )
{
	if( m_flTLength == 0 )
		m_flTLength = 80;
	if( m_flTWidth == 0 )
		m_flTWidth = 10;

	if( pev->movedir == g_vecZero )
		pev->movedir = Vector( 1.0f, 0.0f, 0.0f );

	SetLocalAngles( g_vecZero );	

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	RelinkEntity( TRUE ); // set size and link into world
	UTIL_SetSize( pev, pev->mins, pev->maxs);
	SET_MODEL( edict(), STRING( pev->model ));

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );

	// vecPosition1 is the top position, vecPosition2 is the bottom
	m_vecPosition1 = GetLocalOrigin();
	m_vecPosition2 = GetLocalOrigin();

	if( m_flWidth != 0 )
	{
		m_vecPosition2 = m_vecPosition1 + (pev->movedir * m_flWidth);

		if( m_flHeight != 0 )
			m_vecPosition2.z = m_vecPosition2.z - m_flHeight;
		else
			m_vecPosition2.z = m_vecPosition1.z; // just kill Z-component
	}
	else
	{
		if( m_flHeight != 0 )
			m_vecPosition2.z = m_vecPosition2.z - m_flHeight;
		else
			m_vecPosition2.z = m_vecPosition2.z - pev->size.z + 8;
	}

	if( !pev->speed )
		pev->speed = 150;

	if( !m_volume )
		m_volume = 0.85f;

	if( !pev->dmg )
		pev->dmg = 1.0f;
}

void CFuncPlat :: Precache( )
{
	CBasePlatTrain::Precache();

	if( !IsTogglePlat( ) && !FClassnameIs( pev, "func_platform" ))
		PlatSpawnInsideTrigger( pev ); // the "start moving" trigger
}

void CFuncPlat :: Spawn( void )
{
	Setup();

	Precache();

	// If this platform is the target of some button, it starts at the TOP position,
	// and is brought down by that button.  Otherwise, it starts at BOTTOM.
	if( !FStringNull( pev->targetname ))
	{
		UTIL_SetOrigin( this, m_vecPosition1 );
		m_toggle_state = TS_AT_TOP;
		SetUse( &CFuncPlat::PlatUse );
	}
	else
	{
		UTIL_SetOrigin( this, m_vecPosition2 );
		m_toggle_state = TS_AT_BOTTOM;
	}
}

//
// Used by SUB_UseTargets, when a platform is the target of a button.
// Start bringing platform down.
//
void CFuncPlat :: PlatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( IsLockedByMaster( ))
		return;

	if( useType == USE_SET && FClassnameIs( pev, "func_platform" ))
	{
		GoToFloor( value );
		return;
	}

	if( IsTogglePlat( ))
	{
		// Top is off, bottom is on
		BOOL on = (m_toggle_state == TS_AT_BOTTOM) ? TRUE : FALSE;

		if( !ShouldToggle( useType, on ))
			return;

		if( m_toggle_state == TS_AT_TOP )
			GoDown();
		else if( m_toggle_state == TS_AT_BOTTOM )
			GoUp();
	}
	else
	{
		SetUse( NULL );

		if( m_toggle_state == TS_AT_TOP )
			GoDown();
	}
}

//
// Platform is at top, now starts moving down.
//
void CFuncPlat :: GoDown( void )
{
	if( pev->noise )
		EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ), m_volume, ATTN_NORM );

	ASSERT( m_toggle_state == TS_AT_TOP || m_toggle_state == TS_GOING_UP );

	m_toggle_state = TS_GOING_DOWN;
	SetMoveDone( &CFuncPlat::CallHitBottom );
	LinearMove( m_vecPosition2, pev->speed );
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncPlat :: HitBottom( void )
{
	if( pev->noise )
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));

	if( pev->noise1 )
		EMIT_SOUND( edict(), CHAN_WEAPON, STRING( pev->noise1 ), m_volume, ATTN_NORM );

	ASSERT( m_toggle_state == TS_GOING_DOWN );
	m_toggle_state = TS_AT_BOTTOM;
}

//
// Platform is at bottom, now starts moving up
//
void CFuncPlat :: GoUp( void )
{
	if( pev->noise )
		EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ), m_volume, ATTN_NORM );
	
	ASSERT( m_toggle_state == TS_AT_BOTTOM || m_toggle_state == TS_GOING_DOWN );
	m_toggle_state = TS_GOING_UP;
	SetMoveDone( &CFuncPlat::CallHitTop );
	LinearMove( m_vecPosition1, pev->speed );
}


//
// Platform has hit top.  Pauses, then starts back down again.
//
void CFuncPlat :: HitTop( void )
{
	if( pev->noise )
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));

	if( pev->noise1 )
		EMIT_SOUND( edict(), CHAN_WEAPON, STRING( pev->noise1 ), m_volume, ATTN_NORM );
	
	ASSERT( m_toggle_state == TS_GOING_UP );
	m_toggle_state = TS_AT_TOP;

	if( !IsTogglePlat( ))
	{
		// After a delay, the platform will automatically start going down again.
		SetMoveDone( &CFuncPlat::CallGoDown );
		SetMoveDoneTime( 3 );
	}
}

//
// Platform has hit specified floor.
//
void CFuncPlat :: HitFloor( void )
{
	if( pev->noise )
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));

	if( pev->noise1 )
		EMIT_SOUND( edict(), CHAN_WEAPON, STRING( pev->noise1 ), m_volume, ATTN_NORM );
	
	ASSERT( m_toggle_state == TS_GOING_UP || m_toggle_state == TS_GOING_DOWN );

	if( m_toggle_state == TS_GOING_UP )
		m_toggle_state = TS_AT_TOP;
	else
		m_toggle_state = TS_AT_BOTTOM;

	UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE, m_flFloor );
	UTIL_FireTargets( pev->netname, m_hActivator, this, USE_SET, m_flFloor );

	SetUse( &CFuncPlat::PlatUse );
	SetThink( NULL );
	DontThink(); // stop the floor counter
}

void CFuncPlat :: GoToFloor( float floor )
{
	float curfloor = CalcFloor();
          m_flFloor = floor;	// store target floor
 
	if( curfloor <= 0.0f ) return;
	if( curfloor == floor )
	{
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_TOGGLE, m_flFloor );
		return; // already there?
	}

          m_vecFloor = m_vecPosition1;
          m_vecFloor.z = GetAbsOrigin().z + (floor * step( )) - (curfloor * step( ));
	
	if( pev->noise )
		EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ), m_volume, ATTN_NORM );
        
	if( floor > curfloor )
		m_toggle_state = TS_GOING_UP;
	else m_toggle_state = TS_GOING_DOWN;

	if( fabs( floor - curfloor ) > 1.0f )
	{
		// run a floor informator for env_counter
		SetThink( &CFuncPlat::FloorCalc );
		SetNextThink( 0.1 );
	}

	SetUse( NULL );
	SetMoveDone( &CFuncPlat::CallHitFloor );
	LinearMove( m_vecFloor, pev->speed );
}

void CFuncPlat :: FloorCalc( void )
{
	UTIL_FireTargets( pev->netname, m_hActivator, this, USE_SET, CalcFloor( ));
	SetNextThink( 0.1 );
}

void CFuncPlat :: Blocked( CBaseEntity *pOther )
{
//	ALERT( at_aiconsole, "%s Blocked by %s\n", GetClassname(), pOther->GetClassname() );

	// Hurt the blocker a little
	if( m_hActivator )
		pOther->TakeDamage( pev, m_hActivator->pev, pev->dmg, DMG_CRUSH );
	else
		pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );

	if( FClassnameIs( pev, "func_platform" ))
		return;

	if( pev->noise )
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));
	
	// Send the platform back where it came from
	ASSERT( m_toggle_state == TS_GOING_UP || m_toggle_state == TS_GOING_DOWN );

	if( m_toggle_state == TS_GOING_UP )
		GoDown();
	else if( m_toggle_state == TS_GOING_DOWN )
		GoUp ();
}

//
// Create a trigger entity for a platform.
//
void CPlatTrigger :: SpawnInsideTrigger( CFuncPlat *pPlatform )
{
	m_pPlatform = pPlatform;

	// Create trigger entity, "point" it at the owning platform, give it a touch method
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	SetLocalOrigin( pPlatform->GetLocalOrigin() );

	// Establish the trigger field's size
	Vector vecTMin = m_pPlatform->pev->mins + Vector ( 25 , 25 , 0 );
	Vector vecTMax = m_pPlatform->pev->maxs + Vector ( 25 , 25 , 8 );
	vecTMin.z = vecTMax.z - ( m_pPlatform->m_vecPosition1.z - m_pPlatform->m_vecPosition2.z + 8 );

	if( m_pPlatform->pev->size.x <= 50 )
	{
		vecTMin.x = ( m_pPlatform->pev->mins.x + m_pPlatform->pev->maxs.x ) / 2;
		vecTMax.x = vecTMin.x + 1;
	}

	if( m_pPlatform->pev->size.y <= 50 )
	{
		vecTMin.y = ( m_pPlatform->pev->mins.y + m_pPlatform->pev->maxs.y ) / 2;
		vecTMax.y = vecTMin.y + 1;
	}

	UTIL_SetSize ( pev, vecTMin, vecTMax );
}

//
// When the platform's trigger field is touched, the platform ???
//
void CPlatTrigger :: Touch( CBaseEntity *pOther )
{
	// Ignore touches by non-players
	if( !pOther->IsPlayer( ))
		return;

	// Ignore touches by corpses
	if( !pOther->IsAlive( ))
		return;
	
	// Make linked platform go up/down.
	if( m_pPlatform->m_toggle_state == TS_AT_BOTTOM )
		m_pPlatform->GoUp();
	else if( m_pPlatform->m_toggle_state == TS_AT_TOP )
		m_pPlatform->SetMoveDoneTime( 1 ); // delay going down
}
