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
/*

===== plats.cpp ========================================================

  spawn, think, and touch functions for trains, etc

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trains.h"
#include "saverestore.h"

#define SF_PLAT_TOGGLE		BIT( 0 )

class CBasePlatTrain : public CBaseToggle
{
	DECLARE_CLASS( CBasePlatTrain, CBaseToggle );
public:
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData* pkvd);
	void Precache( void );

	// This is done to fix spawn flag collisions between this class and a derived class
	virtual BOOL IsTogglePlat( void ) { return (pev->spawnflags & SF_PLAT_TOGGLE) ? TRUE : FALSE; }

	DECLARE_DATADESC();

	int	m_iMoveSnd;	// sound a plat makes while moving
	int	m_iStopSnd;	// sound a plat makes when it stops
	float	m_volume;		// Sound volume
	float	m_flFloor;	// Floor number
};

BEGIN_DATADESC( CBasePlatTrain )
	DEFINE_KEYFIELD( m_iMoveSnd, FIELD_STRING, "movesnd" ),
	DEFINE_KEYFIELD( m_iStopSnd, FIELD_STRING, "stopsnd" ),
	DEFINE_KEYFIELD( m_volume, FIELD_FLOAT, "volume" ),
	DEFINE_FIELD( m_flFloor, FIELD_FLOAT ),
END_DATADESC()

void CBasePlatTrain :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "lip" ))
	{
		m_flLip = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "wait" ))
	{
		m_flWait = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "height" ))
	{
		m_flHeight = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "width" ))
	{
		m_flWidth = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "rotation" ))
	{
		m_vecFinalAngle.x = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "movesnd" ))
	{
		m_iMoveSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "stopsnd" ))
	{
		m_iStopSnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "volume" ))
	{
		m_volume = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CBasePlatTrain :: Precache( void )
{
	int m_sound = UTIL_LoadSoundPreset( m_iMoveSnd );

	// set the plat's "in-motion" sound
	switch( m_sound )
	{
	case 0:
		pev->noise = UTIL_PrecacheSound( "common/null.wav" );
		break;
	case 1:
		pev->noise = UTIL_PrecacheSound( "plats/bigmove1.wav" );
		break;
	case 2:
		pev->noise = UTIL_PrecacheSound( "plats/bigmove2.wav" );
		break;
	case 3:
		pev->noise = UTIL_PrecacheSound( "plats/elevmove1.wav" );
		break;
	case 4:
		pev->noise = UTIL_PrecacheSound( "plats/elevmove2.wav" );
		break;
	case 5:
		pev->noise = UTIL_PrecacheSound( "plats/elevmove3.wav" );
		break;
	case 6:
		pev->noise = UTIL_PrecacheSound( "plats/freightmove1.wav" );
		break;
	case 7:
		pev->noise = UTIL_PrecacheSound( "plats/freightmove2.wav" );
		break;
	case 8:
		pev->noise = UTIL_PrecacheSound( "plats/heavymove1.wav" );
		break;
	case 9:
		pev->noise = UTIL_PrecacheSound( "plats/rackmove1.wav" );
		break;
	case 10:
		pev->noise = UTIL_PrecacheSound( "plats/railmove1.wav" );
		break;
	case 11:
		pev->noise = UTIL_PrecacheSound( "plats/squeekmove1.wav" );
		break;
	case 12:
		pev->noise = UTIL_PrecacheSound( "plats/talkmove1.wav" );
		break;
	case 13:
		pev->noise = UTIL_PrecacheSound( "plats/talkmove2.wav" );
		break;
	default:
		pev->noise = UTIL_PrecacheSound( m_sound );
		break;
	}

	m_sound = UTIL_LoadSoundPreset( m_iStopSnd );

	// set the plat's 'reached destination' stop sound
	switch( m_sound )
	{
	case 0:
		pev->noise1 = UTIL_PrecacheSound( "common/null.wav" );
		break;
	case 1:
		pev->noise1 = UTIL_PrecacheSound( "plats/bigstop1.wav" );
		break;
	case 2:
		pev->noise1 = UTIL_PrecacheSound( "plats/bigstop2.wav" );
		break;
	case 3:
		pev->noise1 = UTIL_PrecacheSound( "plats/freightstop1.wav" );
		break;
	case 4:
		pev->noise1 = UTIL_PrecacheSound( "plats/heavystop2.wav" );
		break;
	case 5:
		pev->noise1 = UTIL_PrecacheSound( "plats/rackstop1.wav" );
		break;
	case 6:
		pev->noise1 = UTIL_PrecacheSound( "plats/railstop1.wav" );
		break;
	case 7:
		pev->noise1 = UTIL_PrecacheSound( "plats/squeekstop1.wav" );
		break;
	case 8:
		pev->noise1 = UTIL_PrecacheSound( "plats/talkstop1.wav" );
		break;
	default:
		pev->noise1 = UTIL_PrecacheSound( m_sound );
		break;
	}
}

//
//====================== PLAT code ====================================================
//

class CFuncPlat : public CBasePlatTrain
{
	DECLARE_CLASS( CFuncPlat, CBasePlatTrain );
public:
	void Spawn( void );
	void Precache( void );
	void Setup( void );

	virtual void Blocked( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return (CBasePlatTrain :: ObjectCaps()) | FCAP_SET_MOVEDIR; }
	void PlatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void GoToFloor( float floor );
	void FloorCalc( void );	// floor calc

	void CallGoUp( void ) { GoUp(); }
	void CallGoDown( void ) { GoDown(); }
	void CallHitTop( void  ) { HitTop(); }
	void CallHitBottom( void ) { HitBottom(); }
	void CallHitFloor( void ) { HitFloor(); }

	virtual void GoUp( void );
	virtual void GoDown( void );
	virtual void HitTop( void );
	virtual void HitBottom( void );
	virtual void HitFloor( void );

	DECLARE_DATADESC();

	// func_platform routines
	float CalcFloor( void ) { return Q_rint( (( GetLocalOrigin().z - m_vecPosition1.z ) / step( )) + 1 ); }
	float step( void ) { return ( pev->size.z + m_flHeight - 2 ); }
	float ftime( void ) { return ( step() / pev->speed ); } // time to moving between floors
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

class CFuncPlatRot : public CFuncPlat
{
	DECLARE_CLASS( CFuncPlatRot, CFuncPlat );
public:
	void Spawn( void );
	void SetupRotation( void );

	virtual void GoUp( void );
	virtual void GoDown( void );
	virtual void HitTop( void );
	virtual void HitBottom( void );
	
	void RotMove( Vector &destAngle, float time );

	DECLARE_DATADESC();

	Vector m_end, m_start;
};

LINK_ENTITY_TO_CLASS( func_platrot, CFuncPlatRot );

BEGIN_DATADESC( CFuncPlatRot )
	DEFINE_FIELD( m_end, FIELD_VECTOR ),
	DEFINE_FIELD( m_start, FIELD_VECTOR ),
END_DATADESC()

void CFuncPlatRot :: SetupRotation( void )
{
	if( m_vecFinalAngle.x != 0 ) // this plat rotates too!
	{
		CBaseToggle :: AxisDir( pev );
		m_start = GetLocalAngles();
		m_end = GetLocalAngles() + pev->movedir * m_vecFinalAngle.x;
	}
	else
	{
		m_start = g_vecZero;
		m_end = g_vecZero;
	}

	if( !FStringNull( pev->targetname )) // Start at top
	{
		SetLocalAngles( m_end );
	}
}

void CFuncPlatRot :: Spawn( void )
{
	BaseClass :: Spawn();
	SetupRotation();
}

void CFuncPlatRot :: GoDown( void )
{
	BaseClass :: GoDown();
	RotMove( m_start, GetMoveDoneTime() );
}

//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncPlatRot :: HitBottom( void )
{
	BaseClass :: HitBottom();
	SetLocalAvelocity( g_vecZero );
	SetLocalAngles( m_start );
}

//
// Platform is at bottom, now starts moving up
//
void CFuncPlatRot :: GoUp( void )
{
	BaseClass :: GoUp();
	RotMove( m_end, GetMoveDoneTime() );
}

//
// Platform has hit top.  Pauses, then starts back down again.
//
void CFuncPlatRot :: HitTop( void )
{
	BaseClass :: HitTop();
	SetLocalAvelocity( g_vecZero );
	SetLocalAngles( m_end );
}

void CFuncPlatRot :: RotMove( Vector &destAngle, float time )
{
	// set destdelta to the vector needed to move
	Vector vecDestDelta = destAngle - GetLocalAngles();

	// Travel time is so short, we're practically there already;  so make it so.
	if( time >= 0.1f )
	{
		SetLocalAvelocity( vecDestDelta * ( 1.0 / time ));
	}
	else
	{
		SetLocalAvelocity( vecDestDelta );
		SetMoveDoneTime( 1 );
	}
}

//
//====================== TRAINSEQ code ==================================================
//
#define DIRECTION_NONE		0
#define DIRECTION_FORWARDS		1
#define DIRECTION_BACKWARDS		2
#define DIRECTION_STOP		3
#define DIRECTION_DESTINATION		4

#define SF_TRAINSEQ_REMOVE		BIT( 1 )
#define SF_TRAINSEQ_DIRECT		BIT( 2 )
#define SF_TRAINSEQ_DEBUG		BIT( 3 )

class CTrainSequence : public CBaseEntity
{
	DECLARE_CLASS( CTrainSequence, CBaseEntity );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	STATE GetState( void ) { return (m_pTrain) ? STATE_ON : STATE_OFF; }

	void EndThink( void );
	void StopSequence( void );
	void ArrivalNotify( void );

	DECLARE_DATADESC();

	string_t		m_iszEntity;
	string_t		m_iszDestination;
	string_t		m_iszTerminate;
	int		m_iDirection;

	CFuncTrain	*m_pTrain;
	CBaseEntity	*m_pDestination;
};

LINK_ENTITY_TO_CLASS( scripted_trainsequence, CTrainSequence );

BEGIN_DATADESC( CTrainSequence )
	DEFINE_KEYFIELD( m_iszEntity, FIELD_STRING, "m_iszEntity" ),
	DEFINE_KEYFIELD( m_iszDestination, FIELD_STRING, "m_iszDestination" ),
	DEFINE_KEYFIELD( m_iszTerminate, FIELD_STRING, "m_iszTerminate" ),
	DEFINE_KEYFIELD( m_iDirection, FIELD_INTEGER, "m_iDirection" ),
	DEFINE_FIELD( m_pDestination, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pTrain, FIELD_CLASSPTR ),
	DEFINE_FUNCTION( EndThink ),
END_DATADESC()

void CTrainSequence :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iDirection" ))
	{
		m_iDirection = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszEntity" ))
	{
		m_iszEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszDestination" ))
	{
		m_iszDestination = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszTerminate" ))
	{
		m_iszTerminate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CTrainSequence :: EndThink( void )
{
	// the sequence has expired. Release control.
	StopSequence();
	UTIL_FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
}

//
//====================== TRAIN code ==================================================
//

#define SF_TRAIN_WAIT_RETRIGGER	BIT( 0 )
#define SF_TRAIN_SETORIGIN		BIT( 1 )
#define SF_TRAIN_START_ON		BIT( 2 )	// Train is initially moving
#define SF_TRAIN_PASSABLE		BIT( 3 )	// Train is not solid -- used to make water trains

#define SF_TRAIN_REVERSE		BIT( 31 )	// hidden flag

class CFuncTrain : public CBasePlatTrain
{
	DECLARE_CLASS( CFuncTrain, CBasePlatTrain );
public:
	void Spawn( void );
	void Precache( void );
	void Activate( void );
	void OverrideReset( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	STATE GetState( void ) { return m_iState; }

	Vector CalcPosition( CBaseEntity *pTarg )
	{
		Vector nextPos = pTarg->GetLocalOrigin();
		if( !FBitSet( pev->spawnflags, SF_TRAIN_SETORIGIN ))
			nextPos -= (pev->mins + pev->maxs) * 0.5f;
		return nextPos;
	}

	void Wait( void );
	void Next( void );
	void SoundSetup( void );
	void Stop( void );

	void StartSequence( CTrainSequence *pSequence );
	void StopSequence( void );

	DECLARE_DATADESC();

	CTrainSequence	*m_pSequence;
	EHANDLE		m_hCurrentTarget;
	BOOL		m_activated;
};

LINK_ENTITY_TO_CLASS( func_train, CFuncTrain );

BEGIN_DATADESC( CFuncTrain )
	DEFINE_FIELD( m_hCurrentTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pSequence, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_activated, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( SoundSetup ),
	DEFINE_FUNCTION( Wait ),
	DEFINE_FUNCTION( Next ),
END_DATADESC()

void CFuncTrain :: Blocked( CBaseEntity *pOther )
{
	if( gpGlobals->time < m_flActivateFinished )
		return;

	m_flActivateFinished = gpGlobals->time + 0.5f;

	if( pev->dmg )
	{
		if( m_hActivator )
			pOther->TakeDamage( pev, m_hActivator->pev, pev->dmg, DMG_CRUSH );
		else
			pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );
          }
}

void CFuncTrain :: Stop( void )
{
	// clear the sound channel.
	if( pev->noise )
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));

	SetLocalVelocity( g_vecZero );
	SetMoveDoneTime( -1 );
	m_iState = STATE_OFF;
	SetMoveDone( NULL );
	DontThink();
}

void CFuncTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( IsLockedByMaster( ))
		return;

	if( !ShouldToggle( useType ))
		return;

	if( pev->spawnflags & SF_TRAIN_WAIT_RETRIGGER )
	{
		// Move toward my target
		pev->spawnflags &= ~SF_TRAIN_WAIT_RETRIGGER;
		Next();
	}
	else
	{
		pev->spawnflags |= SF_TRAIN_WAIT_RETRIGGER;

		// pop back to last target if it's available
		if( pev->enemy )
			pev->target = pev->enemy->v.targetname;

		Stop();

		if( pev->noise1 )
			EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise1 ), m_volume, ATTN_NORM );
	}
}

void CFuncTrain :: Wait( void )
{
	if ( m_pSequence )
		m_pSequence->ArrivalNotify();

	// Fire the pass target if there is one
	if ( m_hCurrentTarget->pev->message )
	{
		UTIL_FireTargets( STRING( m_hCurrentTarget->pev->message ), this, this, USE_TOGGLE, 0 );
		UTIL_FireTargets( STRING( pev->netname ), this, this, USE_TOGGLE );

		if( FBitSet( m_hCurrentTarget->pev->spawnflags, SF_CORNER_FIREONCE ))
			m_hCurrentTarget->pev->message = NULL_STRING;
	}
		
	// need pointer to LAST target.
	if( FBitSet( m_hCurrentTarget->pev->spawnflags, SF_TRAIN_WAIT_RETRIGGER ) || ( pev->spawnflags & SF_TRAIN_WAIT_RETRIGGER ) )
	{
		pev->spawnflags |= SF_TRAIN_WAIT_RETRIGGER;

		Stop();

		if( pev->noise1 )
			EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise1 ), m_volume, ATTN_NORM );
		return;
	}
    
	// ALERT ( at_console, "%f\n", m_flWait );

	if( m_flWait != 0 )
	{
		// -1 wait will wait forever!		
		SetMoveDoneTime( m_flWait );

		if( pev->noise )
			STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));

		if( pev->noise1 )
			EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise1 ), m_volume, ATTN_NORM );

		SetMoveDone( &CFuncTrain::Next );
		m_iState = STATE_OFF;
	}
	else
	{
		// do it RIGHT now!
		Next();
	}
}

//
// Train next - path corner needs to change to next target 
//
void CFuncTrain :: Next( void )
{
	// now find our next target
	CBaseEntity *pTarg = GetNextTarget();

	if( !pTarg )
	{
		Stop();

		// play stop sound
		if( pev->noise1 )
			EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise1 ), m_volume, ATTN_NORM );

		return;
	}

	// Save last target in case we need to find it again
	pev->message = pev->target;

	if( FBitSet( pev->spawnflags, SF_TRAIN_REVERSE ) && m_pSequence != NULL )
	{
		CBaseEntity *pSearch = m_pSequence->m_pDestination;

		while( pSearch )
		{
			if( FStrEq( pSearch->GetTarget(), GetTarget()))
			{
				// pSearch leads to the current corner, so it's the next thing we're moving to.
				pev->target = pSearch->pev->targetname;
				break;
			}

			pSearch = pSearch->GetNextTarget();
		}
	}
	else
	{
		pev->target = pTarg->pev->target;
	}

	m_flWait = pTarg->GetDelay();

	if( m_hCurrentTarget != NULL && m_hCurrentTarget->pev->speed != 0 )
	{
		// don't copy speed from target if it is 0 (uninitialized)
		pev->speed = m_hCurrentTarget->pev->speed;
		ALERT( at_aiconsole, "Train %s speed to %4.2f\n", GetTargetname(), pev->speed );
	}

	m_hCurrentTarget = pTarg;	// keep track of this since path corners change our target for us.
	pev->enemy = pTarg->edict();	// hack

	if( FBitSet( m_hCurrentTarget->pev->spawnflags, SF_CORNER_TELEPORT ))
	{
		// Path corner has indicated a teleport to the next corner.
		SetBits( pev->effects, EF_NOINTERP );
		UTIL_SetOrigin( this, CalcPosition( pTarg ));
		Wait(); // Get on with doing the next path corner.
	}
	else
	{
		// Normal linear move.
		
		// CHANGED this from CHAN_VOICE to CHAN_STATIC around OEM beta time because trains should
		// use CHAN_STATIC for their movement sounds to prevent sound field problems.
		// this is not a hack or temporary fix, this is how things should be. (sjb).
		if( pev->noise )
			STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));

		if( pev->noise )
			EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ), m_volume, ATTN_NORM );

		ClearBits( pev->effects, EF_NOINTERP );
		m_iState = STATE_ON;

		SetMoveDone( &CFuncTrain::Wait );
		LinearMove( CalcPosition( pTarg ), pev->speed );
	}
}

void CFuncTrain :: Activate( void )
{
	// Not yet active, so teleport to first target
	if( !m_activated )
	{
		m_activated = TRUE;
		CBaseEntity *pTarg = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ));

		if( pTarg )
		{
			pev->target = pTarg->pev->target;
			pev->message = pTarg->pev->targetname;
			m_hCurrentTarget = pTarg; // keep track of this since path corners change our target for us.
			Vector nextPos = CalcPosition( pTarg );
    
			Teleport( &nextPos, NULL, NULL );
		}		

		if( FStringNull( pev->targetname ))
		{
			// not triggered, so start immediately
			SetMoveDoneTime( 0.1 );
			SetMoveDone( &CFuncTrain::Next );
		}
		else
		{
			pev->spawnflags |= SF_TRAIN_WAIT_RETRIGGER;
		}
	}
}

void CFuncTrain :: StartSequence(CTrainSequence *pSequence)
{
	m_pSequence = pSequence;
	ClearBits( pev->spawnflags, SF_TRAIN_WAIT_RETRIGGER );
}

void CFuncTrain :: StopSequence( )
{
	m_pSequence = NULL;
	pev->spawnflags &= ~SF_TRAIN_REVERSE;
	Use( this, this, USE_OFF, 0.0f );
}

/*QUAKED func_train (0 .5 .8) ?
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
sounds
1) ratchet metal
*/

void CFuncTrain :: Spawn( void )
{
	Precache();

	if( !pev->speed )
		pev->speed = 100;
	
	if( FStringNull( pev->target ))
		ALERT( at_error, "%s with name %s has no target\n", GetClassname(), GetTargetname());
	
	if( pev->dmg == 0 )
		pev->dmg = 2;
	else if( pev->dmg == -1 )
		pev->dmg = 0;
	
	pev->movetype = MOVETYPE_PUSH;
	
	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_PASSABLE ))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;

	SET_MODEL( edict(), GetModel() );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	RelinkEntity( TRUE );

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
	m_iState = STATE_OFF;
	m_activated = FALSE;

	if( !m_volume )
		m_volume = 0.85f;
}

void CFuncTrain :: Precache( void )
{
	CBasePlatTrain::Precache();

	SetThink( &CFuncTrain :: SoundSetup );
	SetNextThink( 0.1 );
}

void CFuncTrain :: SoundSetup( void )
{
	if( pev->noise && m_iState == STATE_ON )
		EMIT_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ), m_volume, ATTN_NORM );
}

void CFuncTrain::OverrideReset( void )
{
	CBaseEntity *pTarg;

	// are we moving?
	if( GetLocalVelocity() != g_vecZero )
	{
		pev->target = pev->message;
		// now find our next target
		pTarg = GetNextTarget();

		if( !pTarg )
		{
			Stop();
		}
		else	
		{
			// UNDONE: this code is wrong

			// restore target on a next level
			m_hCurrentTarget = pTarg;
			pev->enemy = pTarg->edict();

			// restore sound on a next level
			SetThink( &CFuncTrain :: SoundSetup );
			SetNextThink( 0.1 );
		}
	}
}

void CTrainSequence :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !ShouldToggle( useType ))
		return;

	if( GetState() == STATE_OFF )
	{
		// start the sequence, take control of the train

		CBaseEntity *pEnt = UTIL_FindEntityByTargetname( NULL, STRING( m_iszEntity ), pActivator );

		if( pEnt )
		{
			m_pDestination = UTIL_FindEntityByTargetname( NULL, STRING( m_iszDestination ), pActivator );

			if( FBitSet( pev->spawnflags, SF_TRAINSEQ_DEBUG ))
			{
				ALERT( at_console, "trainsequence \"%s\" found train \"%s\"", GetTargetname(), pEnt->GetTargetname());

				if( m_pDestination )
					ALERT( at_console, "found destination %s\n", m_pDestination->GetTargetname());
				else
					ALERT( at_console, "missing destination\n" );
			}

			if( FStrEq( pEnt->GetClassname(), "func_train" ))
			{
				CFuncTrain *pTrain = (CFuncTrain*)pEnt;

				// check whether it's being controlled by another sequence
				if( pTrain->m_pSequence )
					return;

				// ok, we can now take control of it.
				pTrain->StartSequence( this );
				m_pTrain = pTrain;

				if( FBitSet( pev->spawnflags, SF_TRAINSEQ_DIRECT ))
				{
					pTrain->pev->target = m_pDestination->pev->targetname;
					pTrain->Next();
				}
				else
				{
					int iDir = DIRECTION_NONE;

					switch( m_iDirection )
					{
					case DIRECTION_DESTINATION:
						if( m_pDestination )
						{
							Vector vecFTemp, vecBTemp;
							CBaseEntity *pTrainDest = UTIL_FindEntityByTargetname( NULL, pTrain->GetMessage( ));
							float fForward;

							if( !pTrainDest )
							{
								// this shouldn't happen.
								ALERT(at_error, "Found no path to reach destination! (train has t %s, m %s; dest is %s)\n",
								pTrain->GetTarget(), pTrain->GetMessage(), m_pDestination->GetTargetname( ));
								return;
							}

							if( FBitSet( pTrain->pev->spawnflags, SF_TRAIN_SETORIGIN ))
								fForward = (pTrainDest->GetLocalOrigin() - pTrain->GetLocalOrigin()).Length();
							else fForward = (pTrainDest->GetLocalOrigin() - (pTrain->GetLocalOrigin() + (pTrain->pev->maxs + pTrain->pev->mins) * 0.5f )).Length();
							float fBackward = -fForward; // the further back from the TrainDest entity we are, the shorter the backward distance.

							CBaseEntity *pCurForward = pTrainDest;
							CBaseEntity *pCurBackward = m_pDestination;
							vecFTemp = pCurForward->GetLocalOrigin();
							vecBTemp = pCurBackward->GetLocalOrigin();
							int loopbreaker = 10;

							while( iDir == DIRECTION_NONE )
							{
								if (pCurForward)
								{
									fForward += (pCurForward->GetLocalOrigin() - vecFTemp).Length();
									vecFTemp = pCurForward->GetLocalOrigin();

									// if we've finished tracing the forward line
									if( pCurForward == m_pDestination )
									{
										// if the backward line is longest
										if( fBackward >= fForward || pCurBackward == NULL )
											iDir = DIRECTION_FORWARDS;
									}
									else
									{
										pCurForward = pCurForward->GetNextTarget();
									}
								}

								if( pCurBackward )
								{
									fBackward += (pCurBackward->GetLocalOrigin() - vecBTemp).Length();
									vecBTemp = pCurBackward->GetLocalOrigin();

									// if we've finished tracng the backward line
									if( pCurBackward == pTrainDest )
									{
										// if the forward line is shorter
										if( fBackward < fForward || pCurForward == NULL )
											iDir = DIRECTION_BACKWARDS;
									}
									else
									{
										pCurBackward = pCurBackward->GetNextTarget();
									}
								}

								if( --loopbreaker <= 0 )
									iDir = DIRECTION_STOP;
							}
						}
						else
						{
							iDir = DIRECTION_STOP;
						}
						break;
					case DIRECTION_FORWARDS:
						iDir = DIRECTION_FORWARDS;
						break;
					case DIRECTION_BACKWARDS:
						iDir = DIRECTION_BACKWARDS;
						break;
					case DIRECTION_STOP:
						iDir = DIRECTION_STOP;
						break;
					}
					
					if( iDir == DIRECTION_BACKWARDS && !FBitSet( pTrain->pev->spawnflags, SF_TRAIN_REVERSE ))
					{
						// change direction
						SetBits( pTrain->pev->spawnflags, SF_TRAIN_REVERSE );

						CBaseEntity *pSearch = m_pDestination;

						while( pSearch )
						{
							if( FStrEq( pSearch->GetTarget(), pTrain->GetMessage()))
							{
								CBaseEntity *pTrainTarg = pSearch->GetNextTarget();
								if( pTrainTarg )
									pTrain->pev->enemy = pTrainTarg->edict();
								else pTrain->pev->enemy = NULL;

								pTrain->pev->target = pSearch->pev->targetname;
								break;
							}

							pSearch = pSearch->GetNextTarget();
						}

						if( !pSearch )
						{
							// this shouldn't happen.
							ALERT(at_error, "Found no path to reach destination! (train has t %s, m %s; dest is %s)\n",
							pTrain->GetTarget(), pTrain->GetMessage(), m_pDestination->GetTargetname( ));
							return;
						}

						// we haven't reached the corner, so don't use its settings
						pTrain->m_hCurrentTarget = NULL;
						pTrain->Next();
					}
					else if( iDir == DIRECTION_FORWARDS )
					{
						pTrain->pev->target = pTrain->pev->message;
						pTrain->Next();
					}
					else if( iDir == DIRECTION_STOP )
					{
						SetThink( &CTrainSequence::EndThink );
						SetNextThink( 0.1f );
						return;
					}
				}
			}
			else
			{
				ALERT( at_error, "scripted_trainsequence %s can't affect %s \"%s\": not a train!\n", GetTargetname(), pEnt->GetClassname(), pEnt->GetTargetname( ));
				return;
			}
		}
		else // no entity with that name
		{
			ALERT(at_error, "Missing train \"%s\" for scripted_trainsequence %s!\n", STRING( m_iszEntity ), GetTargetname( ));
			return;
		}
	}
	else // prematurely end the sequence
	{
		// disable the other end conditions
		DontThink();

		// release control of the train
		StopSequence();
	}
}

void CTrainSequence :: StopSequence( void )
{
	if( m_pTrain )
	{
		m_pTrain->StopSequence();
		m_pTrain = NULL;

		if( FBitSet( pev->spawnflags, SF_TRAINSEQ_REMOVE ))
			UTIL_Remove( this );
	}
	else
	{
		ALERT( at_error, "scripted_trainsequence: StopSequence without a train!?\n" );
		return; // this shouldn't happen.
	}

	UTIL_FireTargets( STRING( m_iszTerminate ), this, this, USE_TOGGLE, 0 );
}

void CTrainSequence :: ArrivalNotify( void )
{
	// check whether the current path is our destination,
	// and end the sequence if it is.

	if( m_pTrain )
	{
		if( m_pTrain->m_hCurrentTarget == m_pDestination )
		{
			// we've reached the destination. Stop now.
			EndThink();
		}
	}
	else
	{
		ALERT( at_error, "scripted_trainsequence: ArrivalNotify without a train!?\n" );
		return; // this shouldn't happen.
	}
}

// ---------------------------------------------------------------------
//
// Track Train
//
// ---------------------------------------------------------------------

BEGIN_DATADESC( CFuncTrackTrain )
	DEFINE_FIELD( m_ppath, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pDoor, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pSpeedControl, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_length, FIELD_FLOAT, "wheels" ),
	DEFINE_KEYFIELD( m_height, FIELD_FLOAT, "height" ),
	DEFINE_KEYFIELD( m_startSpeed, FIELD_FLOAT, "startspeed" ),
	DEFINE_FIELD( m_controlMins, FIELD_VECTOR ),
	DEFINE_FIELD( m_controlMaxs, FIELD_VECTOR ),
	DEFINE_FIELD( m_controlOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_KEYFIELD( m_sounds, FIELD_STRING, "sounds" ),
	DEFINE_KEYFIELD( m_soundStart, FIELD_STRING, "soundstart" ),
	DEFINE_KEYFIELD( m_soundStop, FIELD_STRING, "soundstop" ),
	DEFINE_KEYFIELD( m_eVelocityType, FIELD_INTEGER, "acceltype" ),
	DEFINE_KEYFIELD( m_eOrientationType, FIELD_INTEGER, "orientation" ),
	DEFINE_FIELD( m_flVolume, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_flBank, FIELD_FLOAT, "bank" ),
	DEFINE_FIELD( m_flDesiredSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flAccelSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flReachedDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_oldSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_dir, FIELD_FLOAT ),
	DEFINE_FUNCTION( Next ),
	DEFINE_FUNCTION( Find ),
	DEFINE_FUNCTION( NearestPath ),
	DEFINE_FUNCTION( DeadEnd ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_tracktrain, CFuncTrackTrain );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CFuncTrackTrain::CFuncTrackTrain()
{
#ifdef _DEBUG
	m_controlMins.Init();
	m_controlMaxs.Init();
#endif
	// These defaults match old func_tracktrains. Changing these defaults would
	// require a vmf_tweak of older content to keep it from breaking.
	m_eOrientationType = TrainOrientation_AtPathTracks;
	m_eVelocityType = TrainVelocity_EaseInEaseOut;
}

void CFuncTrackTrain :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "wheels" ))
	{
		m_length = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "height" ))
	{
		m_height = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "startspeed" ))
	{
		m_startSpeed = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "sounds" ))
	{
		m_sounds = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "soundstop" ))
	{
		m_soundStop = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "soundstart" ))
	{
		m_soundStart = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "volume" ))
	{
		m_flVolume = (float)( Q_atoi( pkvd->szValue ));
		m_flVolume *= 0.1f;
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "acceltype" ))
	{
		m_eVelocityType = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "orientation" ))
	{
		m_eOrientationType = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "bank" ))
	{
		m_flBank = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CFuncTrackTrain :: SetDirForward( bool bForward )
{
	if( bForward && ( m_dir != 1 ))
	{
		// Reverse direction.
		if ( m_ppath && m_ppath->GetPrevious() )
		{
			m_ppath = m_ppath->GetPrevious();
		}

		m_dir = 1;
	}
	else if( !bForward && ( m_dir != -1 ))
	{
		// Reverse direction.
		if ( m_ppath && m_ppath->GetNext() )
		{
			m_ppath = m_ppath->GetNext();
		}

		m_dir = -1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the speed of the train to the given value in units per second.
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: SetSpeed( float flSpeed, float flAccel )
{
	float flOldSpeed = pev->speed;
	m_flAccelSpeed = 0.0f;

	if( flAccel != 0.0f )
	{
		m_flDesiredSpeed = fabs( flSpeed ) * m_dir;

		if( pev->speed == 0 ) // little push to get us going
			pev->speed = IsDirForward() ? 0.1f : -0.1f;
		m_flAccelSpeed = flAccel;

		Next();
		return;		
	}

	pev->speed = m_flDesiredSpeed = fabs( flSpeed ) * m_dir;
	m_oldSpeed = flOldSpeed;

	if( pev->speed != flOldSpeed )
	{
		// Changing speed.
		if( pev->speed != 0 )
		{
			// Starting to move.
			Next();
		}
		else
		{
			// Stopping.
			Stop();
		}
	}

	ALERT( at_aiconsole, "TRAIN(%s), speed to %.2f\n", GetDebugName(), pev->speed );
}

void CFuncTrackTrain :: SetSpeedExternal( float flSpeed )
{
	float flOldSpeed = pev->speed;

	if( flSpeed > 0.0f )
		SetDirForward( true );
	else if( flSpeed < 0.0 )
		SetDirForward( false );

	pev->speed = m_flDesiredSpeed = fabs( flSpeed ) * m_dir;
	m_oldSpeed = flOldSpeed;

	if( pev->speed != flOldSpeed )
	{
		// Changing speed.
		if( pev->speed != 0 )
		{
			// Starting to move.
			Next();
		}
		else
		{
			// Stopping.
			Stop();
		}
	}

	ALERT( at_aiconsole, "TRAIN(%s), speed to %.2f\n", GetDebugName(), pev->speed );
}

//-----------------------------------------------------------------------------
// Purpose: Stops the train.
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: Stop( void )
{
	SetLocalVelocity( g_vecZero );
	SetLocalAvelocity( g_vecZero );
	m_oldSpeed = pev->speed;
	SetThink( NULL );
	pev->speed = 0;
	StopSound();
}

void CFuncTrackTrain :: Blocked( CBaseEntity *pOther )
{
	// Blocker is on-ground on the train
	if ( FBitSet( pOther->pev->flags, FL_ONGROUND ) && pOther->GetGroundEntity() == this )
	{
		float deltaSpeed = fabs( pev->speed );

		if( deltaSpeed > 50 )
			deltaSpeed = 50;

		Vector vecNewVelocity = pOther->GetAbsVelocity();

		if( !vecNewVelocity.z )
		{
			vecNewVelocity.z += deltaSpeed;
			pOther->SetAbsVelocity( vecNewVelocity );
		}
		return;
	}
	else
	{
		Vector vecNewVelocity = (pOther->GetAbsOrigin() - GetAbsOrigin()).Normalize() * pev->dmg;
		pOther->SetAbsVelocity( vecNewVelocity );
	}

	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_UNBLOCKABLE ))
	{
		// unblockable shouldn't damage the player in this case
		if ( pOther->IsPlayer() )
			return;
	}

	ALERT( at_aiconsole, "TRAIN(%s): Blocked by %s (dmg:%.2f)\n", GetDebugName(), pOther->GetClassname(), pev->dmg );
	if( pev->dmg <= 0 ) return;

	// we can't hurt this thing, so we're not concerned with it
	pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );
}


void CFuncTrackTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( IsLockedByMaster( ))
		return;

	if( useType == USE_RESET )
	{
		if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_FORWARDONLY ))
			return;	// can't moving backward

		if( !FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
			return; // only for non-controllable trains

		// will change only for non-moving train
		if( pev->speed == 0 )
			SetDirForward( !IsDirForward() );
	}
	else if( useType != USE_SET )
	{
		if( !ShouldToggle( useType, ( pev->speed != 0 )))
			return;

		if( pev->speed == 0 )
		{
			if( m_pDoor && m_pDoor->IsDoorControl( ))
			{
				m_pDoor->Use( pActivator, pCaller, USE_SET, 2.0f );
				return; // wait for door closing first
			}

			SetSpeed( m_maxSpeed );
		}
		else
		{
			SetSpeed( 0 );
		}
	}
	else
	{
		// g-cont. Don't controls, if tracktrain on trackchange
	          if( m_ppath == NULL ) 
			return;

		float delta = value;
		float accel = 0.0f;

		if( pCaller && pCaller->IsPlayer( ))
		{
			delta = ((int)(m_flDesiredSpeed * 4) / (int)m_maxSpeed) * 0.25 + 0.25 * delta;
			accel = 100.0f;
		}
		else delta *= m_dir; // momentary button can't sending negative values

		if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_FORWARDONLY ))
			delta = bound( 0.0f, delta, 1.0f );
		else delta = bound( -1.0f, delta, 1.0f ); // g-cont. limit the backspeed

		// don't reverse if decelerate
		if( pev->speed != 0 && IsDirForward() && delta < 0 )
			return;
		if( pev->speed != 0 && !IsDirForward() && delta > 0 )
			return;

		if( delta > 0.0f )
			SetDirForward( true );
		else if( delta < 0.0 )
			SetDirForward( false );
		delta = fabs( delta );
		SetSpeed( m_maxSpeed * delta, accel );

		ALERT( at_aiconsole, "TRAIN( %s ), speed to %.2f\n", GetTargetname(), pev->speed );
	}
}

static float Fix( float angle )
{
	while ( angle < 0 )
		angle += 360;
	while ( angle > 360 )
		angle -= 360;

	return angle;
}


static void FixupAngles( Vector &v )
{
	v.x = Fix( v.x );
	v.y = Fix( v.y );
	v.z = Fix( v.z );
}

#define TRAIN_MINPITCH	60
#define TRAIN_MAXPITCH	200
#define TRAIN_MAXSPEED	1000	// approx max speed for sound pitch calculation

void CFuncTrackTrain :: StopSound( void )
{
	// if sound playing, stop it
	if( m_soundPlaying && pev->noise )
	{
		STOP_SOUND( edict(), CHAN_STATIC, STRING( pev->noise ));
		EMIT_SOUND_DYN( edict(), CHAN_ITEM, STRING( pev->noise2 ), m_flVolume, ATTN_NORM, 0, 100 );
	}

	m_soundPlaying = 0;
}

// update pitch based on speed, start sound if not playing
// NOTE: when train goes through transition, m_soundPlaying should go to 0, 
// which will cause the looped sound to restart.
void CFuncTrackTrain :: UpdateSound( void )
{
	if( !pev->noise )
		return;

	float flSpeedRatio = 0;

	if ( FBitSet( pev->spawnflags, SF_TRACKTRAIN_SPEEDBASED_PITCH ))
	{
		flSpeedRatio = bound( 0.0f, fabs( pev->speed ) / m_maxSpeed, 1.0f );
	}
	else
	{
		flSpeedRatio = bound( 0.0f, fabs( pev->speed ) / TRAIN_MAXSPEED, 1.0f );
	}

	float flpitch = RemapVal( flSpeedRatio, 0.0f, 1.0f, TRAIN_MINPITCH, TRAIN_MAXPITCH );

	if( !m_soundPlaying )
	{
		// play startup sound for train
		EMIT_SOUND_DYN( edict(), CHAN_ITEM, STRING( pev->noise1 ), m_flVolume, ATTN_NORM, 0, 100 );
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING( pev->noise ), m_flVolume, ATTN_NORM, 0, (int)flpitch );
		m_soundPlaying = 1;
	} 
	else
	{
		// update pitch
		EMIT_SOUND_DYN( edict(), CHAN_STATIC, STRING(pev->noise), m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, (int)flpitch );
	}
}

void CFuncTrackTrain :: ArriveAtNode( CPathTrack *pNode )
{
	// Fire the pass target if there is one
	if ( pNode->pev->message && !FBitSet( pev->spawnflags, SF_TRACKTRAIN_NO_FIRE_ON_PASS ))
	{
		UTIL_FireTargets( pNode->pev->message, this, this, USE_TOGGLE, 0 );
		if ( FBitSet( pNode->pev->spawnflags, SF_PATH_FIREONCE ) )
			pNode->pev->message = NULL_STRING;
	}

	if ( pNode->m_iszFireFow && IsDirForward( ))
	{
		UTIL_FireTargets( pNode->m_iszFireFow, this, this, USE_TOGGLE, 0 );
	}

	if ( pNode->m_iszFireRev && !IsDirForward())
	{
		UTIL_FireTargets( pNode->m_iszFireRev, this, this, USE_TOGGLE, 0 );
	}

	//
	// Disable train controls if this path track says to do so.
	//
	if( FBitSet( pNode->pev->spawnflags, SF_PATH_DISABLE_TRAIN ))
		SetBits( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL );
			
	// Don't override the train speed if it's under user control.
	if ( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
	{
		if ( pNode->pev->speed != 0 )
		{
			// don't copy speed from target if it is 0 (uninitialized)
			SetSpeed( pNode->pev->speed );
			ALERT( at_aiconsole, "TrackTrain %s arrived at %s, speed to %4.2f\n", GetDebugName(), pNode->GetDebugName(), pNode->pev->speed );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pnext - 
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: UpdateTrainVelocity( CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval )
{
	Vector velDesired;

	switch( m_eVelocityType )
	{
	case TrainVelocity_Instantaneous:
		velDesired = nextPos - GetLocalOrigin();
		velDesired = velDesired.Normalize();
		velDesired *= fabs( pev->speed );
		SetLocalVelocity( velDesired );
		break;
	case TrainVelocity_LinearBlend:
	case TrainVelocity_EaseInEaseOut:
		if( m_flAccelSpeed != 0.0f )
		{
			float flPrevSpeed = pev->speed;
			float flNextSpeed = m_flDesiredSpeed;

			if( flPrevSpeed != flNextSpeed )
			{
				pev->speed = UTIL_Approach( m_flDesiredSpeed, pev->speed, m_flAccelSpeed * flInterval );
			}
		}
		else if( pPrev && pNext )
		{
			// Get the speed to blend from.
			float flPrevSpeed = pev->speed;
			if( pPrev->pev->speed != 0 && FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
			{
				flPrevSpeed = pPrev->pev->speed;
			}

			// Get the speed to blend to.
			float flNextSpeed = flPrevSpeed;
			if( pNext->pev->speed != 0 && FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
			{
				flNextSpeed = pNext->pev->speed;
			}

			flNextSpeed = fabs( flNextSpeed );
			flPrevSpeed = fabs( flPrevSpeed );

			// If they're different, do the blend.
			if( flPrevSpeed != flNextSpeed )
			{
				Vector vecSegment = pNext->GetLocalOrigin() - pPrev->GetLocalOrigin();
				float flSegmentLen = vecSegment.Length();

				if( flSegmentLen )
				{
					Vector vecCurOffset = GetLocalOrigin() - pPrev->GetLocalOrigin();
					float p = vecCurOffset.Length() / flSegmentLen;
					if( m_eVelocityType == TrainVelocity_EaseInEaseOut )
					{
						p = SimpleSplineRemapVal( p, 0.0f, 1.0f, 0.0f, 1.0f );
					}

					pev->speed = m_dir * ( flPrevSpeed * ( 1.0 - p ) + flNextSpeed * p );
				}
			}
			else
			{
				pev->speed = m_dir * flPrevSpeed;
			}
		}
//Msg( "pev->speed %g\n", pev->speed );
		velDesired = nextPos - GetLocalOrigin();
		velDesired = velDesired.Normalize();
		velDesired *= fabs( pev->speed );
		SetLocalVelocity( velDesired );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pnext - 
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: UpdateTrainOrientation( CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval )
{
	// Trains *can* work in local space, but only if all elements of the track share
	// the same move parent as the train.
	assert( !pPrev || (pPrev->m_hParent.Get() == m_hParent.Get() ) );

	switch( m_eOrientationType )
	{
	case TrainOrientation_Fixed:
		// Fixed orientation. Do nothing.
		break;
	case TrainOrientation_AtPathTracks:
		UpdateOrientationAtPathTracks( pPrev, pNext, nextPos, flInterval );
		break;
	case TrainOrientation_EaseInEaseOut:
	case TrainOrientation_LinearBlend:
		UpdateOrientationBlend( m_eOrientationType, pPrev, pNext, nextPos, flInterval );
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adjusts our angles as we hit each path track. This is for support of
//			trains with wheels that round corners a la HL1 trains.
// FIXME: move into path_track, have the angles come back from LookAhead
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: UpdateOrientationAtPathTracks( CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval )
{
	if( !m_ppath )
		return;

	Vector nextFront = GetLocalOrigin();
	CPathTrack *pNextNode = NULL;

	nextFront.z -= m_height;
	if ( m_length > 0 )
	{
		m_ppath->LookAhead( nextFront, IsDirForward() ? m_length : -m_length, 0, &pNextNode );
	}
	else
	{
		m_ppath->LookAhead( nextFront, IsDirForward() ? 100.0f : -100.0f, 0, &pNextNode );
	}
	nextFront.z += m_height;

	Vector vecFaceDir = nextFront - GetLocalOrigin();
	if ( !IsDirForward() )
	{
		vecFaceDir *= -1;
	}

	Vector angles = UTIL_VecToAngles( vecFaceDir );
	angles.x = -angles.x;
	// The train actually points west
	angles.y += 180;

	// Wrapped with this bool so we don't affect old trains
	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
	{
		if( pNextNode && pNextNode->GetOrientationType() == TrackOrientation_FacePathAngles )
		{
			angles = pNextNode->GetOrientation( IsDirForward() );
		}
	}

	Vector curAngles = GetLocalAngles();

	if ( !pPrev || (vecFaceDir.x == 0 && vecFaceDir.y == 0) )
		angles = curAngles;

	DoUpdateOrientation( curAngles, angles, flInterval );
}


//-----------------------------------------------------------------------------
// Purpose: Blends our angles using one of two orientation blending types.
//			ASSUMES that eOrientationType is either LinearBlend or EaseInEaseOut.
//			FIXME: move into path_track, have the angles come back from LookAhead
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: UpdateOrientationBlend( int eOrientationType, CPathTrack *pPrev, CPathTrack *pNext, const Vector &nextPos, float flInterval )
{
	// Get the angles to blend from.
	Vector angPrev = pPrev->GetOrientation( IsDirForward() );

	// Get the angles to blend to. 
	Vector angNext;
	if ( pNext )
	{
		angNext = pNext->GetOrientation( IsDirForward() );
	}
	else
	{
		// At a dead end, just use the last path track's angles.
		angNext = angPrev;
	}

	if ( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOPITCH ))
	{
		angNext[PITCH] = angPrev[PITCH];
	}

	// Calculate our parametric distance along the path segment from 0 to 1.
	float p = 0;
	if( pPrev && ( angPrev != angNext ) )
	{
		Vector vecSegment = pNext->GetLocalOrigin() - pPrev->GetLocalOrigin();
		float flSegmentLen = vecSegment.Length();
		if( flSegmentLen ) p = m_flReachedDist / flSegmentLen;
	}

	if ( eOrientationType == TrainOrientation_EaseInEaseOut )
	{
		p = SimpleSplineRemapVal( p, 0.0f, 1.0f, 0.0f, 1.0f );
	}

//	Msg( "UpdateOrientationFacePathAngles: %s->%s, p=%f\n", pPrev->GetDebugName(), pNext->GetDebugName(), p );

	Vector4D qtPrev;
	Vector4D qtNext;
	
	AngleQuaternion( angPrev, qtPrev );
	AngleQuaternion( angNext, qtNext );

	Vector angNew = angNext;
	float flAngleDiff = QuaternionAngleDiff( qtPrev, qtNext );
	if( flAngleDiff )
	{
		Vector4D qtNew;
		QuaternionSlerp( qtPrev, qtNext, p, qtNew );
		QuaternionAngle( qtNew, angNew );
	}

	if ( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOPITCH ))
	{
		angNew[PITCH] = angPrev[PITCH];
	}

	angNew.x = -angNew.x;
	DoUpdateOrientation( GetLocalAngles(), angNew, flInterval );
}


//-----------------------------------------------------------------------------
// Purpose: Sets our angular velocity to approach the target angles over the given interval.
//-----------------------------------------------------------------------------
void CFuncTrackTrain :: DoUpdateOrientation( const Vector &curAngles, const Vector &angles, float flInterval )
{
	float vy, vx;
	if ( !FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOPITCH ))
	{
		vx = UTIL_AngleDistance( Fix( angles.x ), Fix( curAngles.x ));
	}
	else
	{
		vx = 0;
	}

	vy = UTIL_AngleDistance( Fix( angles.y ), Fix( curAngles.y ));
	
	// HACKHACK: Clamp really small angular deltas to avoid rotating movement on things
	// that are close enough
	if ( fabs(vx) < 0.1 )
	{
		vx = 0;
	}
	if ( fabs(vy) < 0.1 )
	{
		vy = 0;
	}

	if ( flInterval == 0 )
	{
		// Avoid dividing by zero
		flInterval = 0.1;
	}

	Vector vecAngVel( vx / flInterval, vy / flInterval, GetLocalAvelocity().z );

	if ( m_flBank != 0 )
	{
		if ( vecAngVel.y < -5 )
		{
			vecAngVel.z = UTIL_AngleDistance( UTIL_ApproachAngle( -m_flBank, curAngles.z, m_flBank * 2 ), curAngles.z );
		}
		else if ( vecAngVel.y > 5 )
		{
			vecAngVel.z = UTIL_AngleDistance( UTIL_ApproachAngle( m_flBank, curAngles.z, m_flBank * 2 ), curAngles.z );
		}
		else
		{
			vecAngVel.z = UTIL_AngleDistance( UTIL_ApproachAngle( 0, curAngles.z, m_flBank * 4 ), curAngles.z ) * 4;
		}
	}

	SetLocalAvelocity( vecAngVel );
}

void CFuncTrackTrain :: TeleportToPathTrack( CPathTrack *pTeleport )
{
	Vector angCur = GetLocalAngles();

	Vector nextPos = pTeleport->GetLocalOrigin();
	Vector look = nextPos;
	pTeleport->LookAhead( look, m_length, 0 );

	Vector nextAngles;
	if( look == nextPos )
	{
		nextAngles = GetLocalAngles();
	}
	else
	{
		nextAngles = pTeleport->GetOrientation(( pev->speed > 0 ));
		if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOPITCH ))
		{
			nextAngles[PITCH] = angCur[PITCH];
		}
	}

	Teleport( &pTeleport->GetLocalOrigin(), &nextAngles, NULL );
	SetLocalAvelocity( g_vecZero );
}

void CFuncTrackTrain :: Next( void )
{
	if ( !pev->speed )
	{
		ALERT( at_aiconsole, "TRAIN(%s): Speed is 0\n", STRING(pev->targetname) );
		Stop();
		return;
	}

	if ( !m_ppath )
	{	
		ALERT( at_aiconsole, "TRAIN(%s): Lost path\n", STRING(pev->targetname) );
		Stop();
		return;
	}

	UpdateSound();

	Vector nextPos = GetLocalOrigin();
	float flSpeed = pev->speed;

	nextPos.z -= m_height;
	CPathTrack *pNextNext = NULL;
	CPathTrack *pNext = m_ppath->LookAhead( nextPos, flSpeed * 0.1, 1, &pNextNext );
	nextPos.z += m_height;

	// If we're moving towards a dead end, but our desired speed goes in the opposite direction
	// this fixes us from stalling
	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ) && (( flSpeed < 0 ) != ( m_flDesiredSpeed < 0 )))
	{
		if( !pNext ) pNext = m_ppath;
	}

	// Trains *can* work in local space, but only if all elements of the track share
	// the same move parent as the train.
	ASSERT( !pNext || ( pNext->m_hParent.Get() == m_hParent.Get() ));

	if( pNext )
	{
		// can't compute correct distance in 3D because angle interpolation change the origin
		if( pNext == m_ppath )
			m_flReachedDist += fabs( pev->speed ) * gpGlobals->frametime;
		else m_flReachedDist = 0.0f;

		UpdateTrainVelocity( pNext, pNextNext, nextPos, gpGlobals->frametime );
		UpdateTrainOrientation( pNext, pNextNext, nextPos, 0.1 );

		if( pNext != m_ppath )
		{
			CPathTrack *pFire;

			if( IsDirForward( ))
				pFire = pNext;
			else pFire = m_ppath;

			//
			// We have reached a new path track. Fire its OnPass output.
			//
			m_ppath = pNext;
			ArriveAtNode( pFire );

			//
			// See if we should teleport to the next path track.
			//
			CPathTrack *pTeleport = pNext->GetNext();
			if(( pTeleport != NULL ) && FBitSet( pTeleport->pev->spawnflags, SF_PATH_TELEPORT ))
			{
				TeleportToPathTrack( pTeleport );
			}
		}

		SetMoveDoneTime( 0.5f );
		SetNextThink( 0.0f );
		SetMoveDone( NULL );
		SetThink( &CFuncTrackTrain::Next );
	}
	else
	{
		//
		// We've reached the end of the path, stop.
		//
		StopSound();
		SetLocalVelocity( nextPos - GetLocalOrigin( ));
		float distance = GetLocalVelocity().Length();
		SetLocalAvelocity( g_vecZero );
		m_oldSpeed = pev->speed;
		m_flDesiredSpeed = 0;
		pev->speed = 0;

		// Move to the dead end
		
		// Are we there yet?
		if ( distance > 0 )
		{
			// no, how long to get there?
			float flTime = distance / fabs( m_oldSpeed );
			SetLocalVelocity( GetLocalVelocity() * (m_oldSpeed / distance) );
			SetMoveDoneTime( flTime );
			SetMoveDone( &CFuncTrackTrain::DeadEnd );
			DontThink();
		}
		else
		{
			DeadEnd();
		}
	}
}

void CFuncTrackTrain :: DeadEnd( void )
{
	// Fire the dead-end target if there is one
	CPathTrack *pTrack, *pNext;

	pTrack = m_ppath;

	ALERT( at_aiconsole, "TRAIN(%s): Dead end ", GetDebugName() );
	// Find the dead end path node
	// HACKHACK -- This is bugly, but the train can actually stop moving at a different node depending on it's speed
	// so we have to traverse the list to it's end.
	if ( pTrack )
	{
		if ( m_oldSpeed < 0 )
		{
			do
			{
				pNext = pTrack->ValidPath( pTrack->GetPrevious(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
		else
		{
			do
			{
				pNext = pTrack->ValidPath( pTrack->GetNext(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
	}

	m_oldSpeed = pev->speed;
	m_flDesiredSpeed = 0;
	pev->speed = 0;
	SetAbsVelocity( g_vecZero );
	SetLocalAvelocity( g_vecZero );

	if ( pTrack )
	{
		ALERT( at_aiconsole, "at %s\n", pTrack->GetDebugName() );
		if ( pTrack->pev->netname )
			UTIL_FireTargets( pTrack->GetNetname(), this, this, USE_TOGGLE, 0 );
	}
	else
		ALERT( at_aiconsole, "\n" );
}

void CFuncTrackTrain :: SetControls( CBaseEntity *pControls )
{
	Vector offset = pControls->GetLocalOrigin() - m_controlOrigin;

	m_controlMins = pControls->pev->mins + offset;
	m_controlMaxs = pControls->pev->maxs + offset;
}

BOOL CFuncTrackTrain :: OnControls( CBaseEntity *pTest )
{
	Vector offset = pTest->GetAbsOrigin() - GetLocalOrigin();

	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ))
		return FALSE;

	// Transform offset into local coordinates
	matrix3x3 tmp = matrix3x3( GetLocalAngles() );
	// rotate into local space
	Vector local = tmp.VectorIRotate( offset );

	if ( local.x >= m_controlMins.x && local.y >= m_controlMins.y && local.z >= m_controlMins.z &&
		 local.x <= m_controlMaxs.x && local.y <= m_controlMaxs.y && local.z <= m_controlMaxs.z )
		 return TRUE;

	return FALSE;
}

void CFuncTrackTrain :: Find( void )
{
	m_ppath = (CPathTrack *)UTIL_FindEntityByTargetname( NULL, GetTarget() );
	if ( !m_ppath )
		return;

	if ( !FClassnameIs( m_ppath, "path_track" ) )
	{
		ALERT( at_error, "func_track_train must be on a path of path_track\n" );
		m_ppath = NULL;
		return;
	}

	Vector nextPos = m_ppath->GetLocalOrigin();
	Vector look = nextPos;
	m_ppath->LookAhead( look, m_length, 0 );
	nextPos.z += m_height;
	look.z += m_height;

	Vector nextAngles = UTIL_VecToAngles( look - nextPos );
	nextAngles.x = -nextAngles.x;
	// The train actually points west
	nextAngles.y += 180;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOPITCH )
		nextAngles.x = 0;

	Teleport( &nextPos, &nextAngles, NULL );
	SetLocalAvelocity( g_vecZero );

	ArriveAtNode( m_ppath );
	pev->speed = m_startSpeed;

	SetNextThink( 0.1f );
	SetThink( &CFuncTrackTrain::Next );
}

void CFuncTrackTrain :: NearestPath( void )
{
	CBaseEntity *pTrack = NULL;
	CBaseEntity *pNearest = NULL;
	float dist, closest;

	closest = 1024;

	while ((pTrack = UTIL_FindEntityInSphere( pTrack, GetAbsOrigin(), 1024 )) != NULL)
	{
		// filter out non-tracks
		if ( !(pTrack->pev->flags & (FL_CLIENT|FL_MONSTER)) && FClassnameIs( pTrack, "path_track" ) )
		{
			dist = (GetLocalOrigin() - pTrack->GetLocalOrigin()).Length();
			if ( dist < closest )
			{
				closest = dist;
				pNearest = pTrack;
			}
		}
	}

	if ( !pNearest )
	{
		ALERT( at_console, "Can't find a nearby track !!!\n" );
		SetThink( NULL);
		return;
	}

	ALERT( at_aiconsole, "TRAIN: %s, Nearest track is %s\n", GetDebugName(), pNearest->GetDebugName() );
	// If I'm closer to the next path_track on this path, then it's my real path
	pTrack = ((CPathTrack *)pNearest)->GetNext();
	if ( pTrack )
	{
		if ( (GetLocalOrigin() - pTrack->GetLocalOrigin()).Length() < (GetLocalOrigin() - pNearest->GetLocalOrigin()).Length() )
			pNearest = pTrack;
	}

	m_ppath = (CPathTrack *)pNearest;

	if ( pev->speed != 0 )
	{
		SetMoveDoneTime( 0.1 );
		SetMoveDone( &CFuncTrackTrain::Next );
	}
}


void CFuncTrackTrain :: OverrideReset( void )
{
	// NOTE: all entities are spawned so we don't need
	// to make delay before searching nearest path
	if( !m_ppath )
	{
		NearestPath();
		SetThink( NULL );
	}
}

CFuncTrackTrain *CFuncTrackTrain :: Instance( edict_t *pent )
{ 
	CBaseEntity *pEntity = CBaseEntity::Instance( pent );
	if( FClassnameIs( pEntity, "func_tracktrain" ))
		return (CFuncTrackTrain *)pEntity;
	return NULL;
}

/*QUAKED func_train (0 .5 .8) ?
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
sounds
1) ratchet metal
*/

void CFuncTrackTrain :: Spawn( void )
{
	if( m_maxSpeed == 0 )
	{
		if( pev->speed == 0 )
			m_maxSpeed = 100;
		else m_maxSpeed = pev->speed;
	}

	SetAbsVelocity( g_vecZero );
	SetLocalAvelocity( g_vecZero );
	pev->speed = 0;
	m_dir = 1;

	if ( FStringNull(pev->target) )
		ALERT( at_console, "FuncTrackTrain '%s' has no target.\n", GetDebugName());

	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_UNBLOCKABLE ))
		SetBits( pev->flags, FL_UNBLOCKABLE );

	if( FBitSet( pev->spawnflags, SF_TRACKTRAIN_PASSABLE ))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	
	SET_MODEL( edict(), GetModel() );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	RelinkEntity( TRUE );

	// NOTE: cull the tracktrain by PHS because it's may out of PVS
	// and hanging the moving sound
	SetBits( pev->effects, EF_REQUEST_PHS );

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );

	// Cache off placed origin for train controls
	m_controlOrigin = GetLocalOrigin();

	m_controlMins = pev->mins;
	m_controlMaxs = pev->maxs;
	m_controlMaxs.z += 72;
// start trains on the next frame, to make sure their targets have had
// a chance to spawn/activate
	SetThink( &CFuncTrackTrain::Find );
	SetNextThink( 0.0f );
	Precache();
}

void CFuncTrackTrain :: Precache( void )
{
	if( !m_flVolume )
		m_flVolume = 1.0f;

	int m_sound = UTIL_LoadSoundPreset( m_sounds );

	switch( m_sound )
	{
	case 0: pev->noise = 0; break; // no sound
	case 1: pev->noise = UTIL_PrecacheSound( "plats/ttrain1.wav" ); break;
	case 2: pev->noise = UTIL_PrecacheSound( "plats/ttrain2.wav" ); break;
	case 3: pev->noise = UTIL_PrecacheSound( "plats/ttrain3.wav" ); break;
	case 4: pev->noise = UTIL_PrecacheSound( "plats/ttrain4.wav" ); break;
	case 5: pev->noise = UTIL_PrecacheSound( "plats/ttrain6.wav" ); break;
	case 6: pev->noise = UTIL_PrecacheSound( "plats/ttrain7.wav" ); break;
	default: pev->noise = UTIL_PrecacheSound( m_sound ); break;
	}

	if( m_soundStart != NULL_STRING )
		pev->noise1 = UTIL_PrecacheSound( m_soundStart );
	else pev->noise1 = UTIL_PrecacheSound( "plats/ttrain_start1.wav" );

	if( m_soundStop != NULL_STRING )
		pev->noise2 = UTIL_PrecacheSound( m_soundStop );
	else pev->noise2 = UTIL_PrecacheSound( "plats/ttrain_brake1.wav" );
}

void CFuncTrackTrain :: UpdateOnRemove( void )
{
	StopSound();
	BaseClass::UpdateOnRemove();
}

// This class defines the volume of space that the player must stand in to control the train
class CFuncTrainControls : public CBaseEntity
{
	DECLARE_CLASS( CFuncTrainControls, CBaseEntity );
public:
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IGNORE_PARENT; }
	void Find( void );
	void Spawn( void );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( func_traincontrols, CFuncTrainControls );

BEGIN_DATADESC( CFuncTrainControls )
	DEFINE_FUNCTION( Find ),
END_DATADESC()

void CFuncTrainControls :: Find( void )
{
	CBaseEntity *pTarget = NULL;

	do 
	{
		pTarget = UTIL_FindEntityByTargetname( pTarget, GetTarget( ));
	} while ( pTarget && !FClassnameIs( pTarget, "func_tracktrain" ));

	if( !pTarget )
	{
		ALERT( at_error, "TrackTrainControls: No train %s\n", GetTarget( ));
		UTIL_Remove( this );
		return;
	}

	// UNDONE: attach traincontrols with parent system if origin-brush is present?
	CFuncTrackTrain *ptrain = (CFuncTrackTrain *)pTarget;
	ptrain->SetControls( this );
	UTIL_Remove( this );
}

void CFuncTrainControls :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( edict(), GetModel() );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	RelinkEntity( TRUE );
	
	SetThink( &CFuncTrainControls::Find );
	SetNextThink( 0.0f );
}



// ----------------------------------------------------------------------------
//
// Track changer / Train elevator
//
// ----------------------------------------------------------------------------

#define SF_TRACK_ACTIVATETRAIN	0x00000001
#define SF_TRACK_RELINK		0x00000002
#define SF_TRACK_ROTMOVE		0x00000004
#define SF_TRACK_STARTBOTTOM		0x00000008
#define SF_TRACK_DONT_MOVE		0x00000010
#define SF_TRACK_ONOFF_MODE		0x00000020

//
// This entity is a rotating/moving platform that will carry a train to a new track.
// It must be larger in X-Y planar area than the train, since it must contain the
// train within these dimensions in order to operate when the train is near it.
//

typedef enum { TRAIN_SAFE, TRAIN_BLOCKING, TRAIN_FOLLOWING } TRAIN_CODE;

class CFuncTrackChange : public CFuncPlatRot
{
	DECLARE_CLASS( CFuncTrackChange, CFuncPlatRot );
public:
	void Spawn( void );
	void Precache( void );

	virtual void	GoUp( void );
	virtual void	GoDown( void );

	void		KeyValue( KeyValueData* pkvd );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void		Find( void );
	TRAIN_CODE	EvaluateTrain( CPathTrack *pcurrent );
	void		UpdateTrain( Vector &dest );
	virtual void	HitBottom( void );
	virtual void	HitTop( void );
	void		Touch( CBaseEntity *pOther ) {};
	virtual void	UpdateAutoTargets( int toggleState );
	virtual BOOL	IsTogglePlat( void ) { return TRUE; }

	void		DisableUse( void ) { m_use = 0; }
	void		EnableUse( void ) { m_use = 1; }
	int		UseEnabled( void ) { return m_use; }

	DECLARE_DATADESC();

	virtual void	OverrideReset( void );

	CPathTrack	*m_trackTop;
	CPathTrack	*m_trackBottom;

	CFuncTrackTrain	*m_train;

	string_t		m_trackTopName;
	string_t		m_trackBottomName;
	string_t		m_trainName;
	TRAIN_CODE	m_code;
	float		m_flRadius;	// custom radius to search train
	int		m_targetState;
	int		m_use;
};

LINK_ENTITY_TO_CLASS( func_trackchange, CFuncTrackChange );

BEGIN_DATADESC( CFuncTrackChange )
	DEFINE_GLOBAL_FIELD( m_trackTop, FIELD_CLASSPTR ),
	DEFINE_GLOBAL_FIELD( m_trackBottom, FIELD_CLASSPTR ),
	DEFINE_GLOBAL_FIELD( m_train, FIELD_CLASSPTR ),
	DEFINE_GLOBAL_KEYFIELD( m_trackTopName, FIELD_STRING, "toptrack" ),
	DEFINE_GLOBAL_KEYFIELD( m_trackBottomName, FIELD_STRING, "bottomtrack" ),
	DEFINE_GLOBAL_KEYFIELD( m_trainName, FIELD_STRING, "train" ),
	DEFINE_FIELD( m_code, FIELD_INTEGER ),
	DEFINE_FIELD( m_targetState, FIELD_INTEGER ),
	DEFINE_FIELD( m_use, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flRadius, FIELD_FLOAT ),
	DEFINE_FUNCTION( GoUp ),
	DEFINE_FUNCTION( GoDown ),
	DEFINE_FUNCTION( Find ),
END_DATADESC()

void CFuncTrackChange :: Spawn( void )
{
	Setup();

	if( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ))
		m_vecPosition2 = m_vecPosition1;

	SetupRotation();

	if( FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
	{
		UTIL_SetOrigin( this, m_vecPosition2 );
		m_toggle_state = TS_AT_BOTTOM;
		SetLocalAngles( m_start );
		m_targetState = TS_AT_TOP;
	}
	else
	{
		UTIL_SetOrigin( this, m_vecPosition1 );
		m_toggle_state = TS_AT_TOP;
		SetLocalAngles( m_end );
		m_targetState = TS_AT_BOTTOM;
	}

	EnableUse();
	SetNextThink( 2.0f ); // let's train spawn
	SetThink( &CFuncTrackChange::Find );
	Precache();
}

void CFuncTrackChange :: Precache( void )
{
	// Can't trigger sound
	PRECACHE_SOUND( "buttons/button11.wav" );
	
	BaseClass::Precache();
}

void CFuncTrackChange :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "train" ))
	{
		m_trainName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "toptrack" ))
	{
		m_trackTopName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "bottomtrack" ))
	{
		m_trackBottomName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "radius" ))
	{
		m_flRadius = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue( pkvd ); // Pass up to base class
	}
}

void CFuncTrackChange::OverrideReset( void )
{
	SetMoveDoneTime( 1.0 );
	SetMoveDone( &CFuncTrackChange::Find );
}

void CFuncTrackChange :: Find( void )
{
	// Find track entities
	CBaseEntity *pTarget;

	pTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_trackTopName ));

	if( pTarget && FClassnameIs( pTarget, "path_track" ))
	{
		m_trackTop = (CPathTrack *)pTarget;
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_trackBottomName ));

		if( pTarget && FClassnameIs( pTarget, "path_track" ))
		{
			m_trackBottom = (CPathTrack *)pTarget;
			pTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_trainName ));

			if( pTarget && FClassnameIs( pTarget, "func_tracktrain" ))
			{
				m_train = (CFuncTrackTrain *)pTarget;
				Vector center = (pev->absmin + pev->absmax) * 0.5f;
				m_trackBottom = m_trackBottom->Nearest( center );
				m_trackTop = m_trackTop->Nearest( center );
				UpdateAutoTargets( m_toggle_state );
				SetThink( NULL );
				return;
			}
			else
			{
				ALERT( at_error, "Can't find train for track change! %s\n", STRING( m_trainName ));
			}
		}
		else
		{
			ALERT( at_error, "Can't find bottom track for track change! %s\n", STRING(m_trackBottomName) );
		}
	}
	else
	{
		ALERT( at_error, "Can't find top track for track change! %s\n", STRING(m_trackTopName) );
	}
}

TRAIN_CODE CFuncTrackChange :: EvaluateTrain( CPathTrack *pcurrent )
{
	// Go ahead and work, we don't have anything to switch, so just be an elevator
	if( !pcurrent || !m_train )
		return TRAIN_SAFE;

	if( m_train->m_ppath == pcurrent || ( pcurrent->m_pprevious && m_train->m_ppath == pcurrent->m_pprevious ) ||
		 ( pcurrent->m_pnext && m_train->m_ppath == pcurrent->m_pnext ))
	{
		if( m_train->GetSpeed() != 0 )
			return TRAIN_BLOCKING;

		Vector dist = GetLocalOrigin() - m_train->GetLocalOrigin();
		float length = dist.Length2D();

		if( length < m_train->m_length ) // empirically determined close distance
			return TRAIN_FOLLOWING;
		else if( length > ( 150 + m_train->m_length ))
			return TRAIN_SAFE;

		return TRAIN_BLOCKING;
	}
	return TRAIN_SAFE;
}

void CFuncTrackChange :: UpdateTrain( Vector &dest )
{
	float time = GetMoveDoneTime();

	m_train->SetAbsVelocity( GetAbsVelocity() );
	m_train->SetLocalAvelocity( GetLocalAvelocity() );
	m_train->SetMoveDoneTime( time );

	// attempt at getting the train to rotate properly around the origin of the trackchange
	if( time <= 0 )
		return;

	Vector offset = m_train->GetLocalOrigin() - GetLocalOrigin();
	Vector local, delta = dest - GetLocalAngles();

	// Transform offset into local coordinates
	UTIL_MakeInvVectors( delta, gpGlobals );
	local.x = DotProduct( offset, gpGlobals->v_forward );
	local.y = DotProduct( offset, gpGlobals->v_right );
	local.z = DotProduct( offset, gpGlobals->v_up );

	local = local - offset;
	m_train->SetAbsVelocity( GetAbsVelocity() + ( local * (1.0f / time )) );
}

void CFuncTrackChange :: GoDown( void )
{
	if( m_code == TRAIN_BLOCKING )
		return;

	// HitBottom may get called during CFuncPlat::GoDown(), so set up for that
	// before you call GoDown()

	UpdateAutoTargets( TS_GOING_DOWN );

	// If ROTMOVE, move & rotate
	if( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ) )
	{
		SetMoveDone( &CFuncPlat::CallHitBottom );
		m_toggle_state = TS_GOING_DOWN;
		AngularMove( m_start, pev->speed );
	}
	else
	{
		CFuncPlat :: GoDown();
		SetMoveDone( &CFuncPlat::CallHitBottom );
		RotMove( m_start, GetMoveDoneTime() );
	}

	// Otherwise, rotate first, move second

	// If the train is moving with the platform, update it
	if( m_code == TRAIN_FOLLOWING )
	{
		UpdateTrain( m_start );
		m_train->m_ppath = NULL;
	}
}

//
// Platform is at bottom, now starts moving up
//
void CFuncTrackChange :: GoUp( void )
{
	if( m_code == TRAIN_BLOCKING )
		return;

	// HitTop may get called during CFuncPlat::GoUp(), so set up for that
	// before you call GoUp();

	UpdateAutoTargets( TS_GOING_UP );

	if( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ))
	{
		m_toggle_state = TS_GOING_UP;
		SetMoveDone( &CFuncPlat::CallHitTop );
		AngularMove( m_end, pev->speed );
	}
	else
	{
		// If ROTMOVE, move & rotate
		CFuncPlat :: GoUp();
		SetMoveDone( &CFuncPlat::CallHitTop );
		RotMove( m_end, GetMoveDoneTime( ));
	}
	
	// Otherwise, move first, rotate second

	// If the train is moving with the platform, update it
	if( m_code == TRAIN_FOLLOWING )
	{
		UpdateTrain( m_end );
		m_train->m_ppath = NULL;
	}
}

// Normal track change
void CFuncTrackChange :: UpdateAutoTargets( int toggleState )
{
	// g-cont. update targets same as func_door
	if( toggleState == TS_AT_BOTTOM )
	{
		if( !FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
			UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
		else
			UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}
	else if( toggleState == TS_AT_TOP )
	{
		if( FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
			UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
		else
			UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}

	if( !m_trackTop || !m_trackBottom )
		return;

	if( toggleState == TS_AT_TOP )
		ClearBits( m_trackTop->pev->spawnflags, SF_PATH_DISABLED );
	else
		SetBits( m_trackTop->pev->spawnflags, SF_PATH_DISABLED );

	if( toggleState == TS_AT_BOTTOM )
		ClearBits( m_trackBottom->pev->spawnflags, SF_PATH_DISABLED );
	else
		SetBits( m_trackBottom->pev->spawnflags, SF_PATH_DISABLED );
}

void CFuncTrackChange :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( FBitSet( pev->spawnflags, SF_TRACK_ONOFF_MODE ))
	{
		if( useType == USE_ON )
		{
			if( m_toggle_state != TS_AT_BOTTOM )
				return;	// ignore
		}
		else if( useType == USE_OFF )
		{
			if( m_toggle_state != TS_AT_TOP )
				return;
		}
		else if( useType == USE_SET || useType == USE_RESET )
			return; // completely ignore
	}


	// g-cont. if trainname not specified trackchange always search tracktrain in the model radius 
	if( FStringNull( m_trainName ))
	{
		// train not specified - search train in radius of trackchange or custom specfied radius
		float radius = m_flRadius;
		if( !radius ) radius = ( Q_max( pev->size.x, Q_max( pev->size.y, pev->size.z ))) / 2.0f;
		CBaseEntity *pFind = NULL;

		while(( pFind = UTIL_FindEntityInSphere( pFind, GetAbsOrigin(), radius )) != NULL )
		{
			if( FClassnameIs( pFind->pev, "func_tracktrain" ))
			{
				m_train = (CFuncTrackTrain *)pFind;
				ALERT( at_aiconsole, "Found train %s\n", STRING( pFind->pev->targetname ));
				break;
			}
		}

//		if( m_train == NULL ) 
//			ALERT( at_error, "%s: couldn't find train to operate\n", GetTargetname( ));
	}

	if( m_train && m_train->m_pDoor && m_train->m_pDoor->IsDoorControl( ))
	{
		m_train->m_pDoor->Use( this, this, USE_SET, 3.0f );
		return;
	}

	if( m_toggle_state != TS_AT_TOP && m_toggle_state != TS_AT_BOTTOM )
		return;

	// If train is in "safe" area, but not on the elevator, play alarm sound
	if( m_toggle_state == TS_AT_TOP )
		m_code = EvaluateTrain( m_trackTop );
	else if( m_toggle_state == TS_AT_BOTTOM )
		m_code = EvaluateTrain( m_trackBottom );
	else
		m_code = TRAIN_BLOCKING;

	if( m_code == TRAIN_BLOCKING )
	{
		// Play alarm and return
		EMIT_SOUND( edict(), CHAN_VOICE, "buttons/button11.wav", 1, ATTN_NORM );
		return;
	}

	// Otherwise, it's safe to move
	// If at top, go down
	// at bottom, go up

	DisableUse();
	if( m_toggle_state == TS_AT_TOP )
		GoDown();
	else
		GoUp();
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncTrackChange :: HitBottom( void )
{
	BaseClass :: HitBottom();

	if( m_code == TRAIN_FOLLOWING )
	{
		m_train->SetTrack( m_trackBottom );
	}

	SetMoveDone( NULL );
	SetMoveDoneTime( -1 );

	UpdateAutoTargets( m_toggle_state );

	EnableUse();
}

//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncTrackChange :: HitTop( void )
{
	BaseClass :: HitTop();

	if( m_code == TRAIN_FOLLOWING )
	{
		m_train->SetTrack( m_trackTop );
	}
	
	// Don't let the plat go back down
	SetMoveDone( NULL );
	SetMoveDoneTime( -1 );
	UpdateAutoTargets( m_toggle_state );
	EnableUse();
}

class CFuncTrackAuto : public CFuncTrackChange
{
	DECLARE_CLASS( CFuncTrackAuto, CFuncTrackChange );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void UpdateAutoTargets( int toggleState );
};

LINK_ENTITY_TO_CLASS( func_trackautochange, CFuncTrackAuto );

// Auto track change
void CFuncTrackAuto :: UpdateAutoTargets( int toggleState )
{
	// g-cont. update targets same as func_door
	if( toggleState == TS_AT_BOTTOM )
	{
		if( !FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
			UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
		else
			UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}
	else if( toggleState == TS_AT_TOP )
	{
		if( FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ))
			UTIL_FireTargets( pev->netname, m_hActivator, this, USE_TOGGLE, 0 );
		else
			UTIL_FireTargets( pev->message, m_hActivator, this, USE_TOGGLE, 0 );
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}

	if( !m_trackTop || !m_trackBottom )
		return;

	CPathTrack *pTarget, *pNextTarget;

	if( m_targetState == TS_AT_TOP )
	{
		pTarget = m_trackTop->GetNext();
		pNextTarget = m_trackBottom->GetNext();
	}
	else
	{
		pTarget = m_trackBottom->GetNext();
		pNextTarget = m_trackTop->GetNext();
	}

	if( pTarget )
	{
		ClearBits( pTarget->pev->spawnflags, SF_PATH_DISABLED );

		if( m_code == TRAIN_FOLLOWING && m_train && !m_train->GetSpeed() )
			m_train->Use( this, this, USE_ON, 0 );
	}

	if( pNextTarget )
		SetBits( pNextTarget->pev->spawnflags, SF_PATH_DISABLED );

}

void CFuncTrackAuto :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CPathTrack *pTarget;

	if( !UseEnabled( ))
		return;

	if( m_toggle_state == TS_AT_TOP )
		pTarget = m_trackTop;
	else if( m_toggle_state == TS_AT_BOTTOM )
		pTarget = m_trackBottom;
	else
		pTarget = NULL;

	m_hActivator = pActivator;

	if( FStringNull( m_trainName ))
		m_train = NULL; // clearing for each call

	if( FClassnameIs( pActivator, "func_tracktrain" ))
	{
		// g-cont. func_trackautochange doesn't search train in specified radius. It will be waiting train as activator
		if( !m_train ) m_train = (CFuncTrackTrain *)pActivator;

		m_code = EvaluateTrain( pTarget );

		// safe to fire?
		if( m_code == TRAIN_FOLLOWING && m_toggle_state != m_targetState )
		{
			DisableUse();
			if( m_toggle_state == TS_AT_TOP )
				GoDown();
			else
				GoUp();
		}
	}
	else if( m_train != NULL )
	{
		if( pTarget )
			pTarget = pTarget->GetNext();

		if( pTarget && m_train->m_ppath != pTarget && ShouldToggle( useType, m_targetState ))
		{
			if( m_targetState == TS_AT_TOP )
				m_targetState = TS_AT_BOTTOM;
			else
				m_targetState = TS_AT_TOP;
		}

		UpdateAutoTargets( m_targetState );
	}
}


// ----------------------------------------------------------
//
//
// pev->speed is the travel speed
// pev->health is current health
// pev->max_health is the amount to reset to each time it starts

#define FGUNTARGET_START_ON			0x0001

class CGunTarget : public CBaseMonster
{
	DECLARE_CLASS( CGunTarget, CBaseMonster );
public:
	void Spawn( void );
	void Activate( void );
	void Next( void );
	void Start( void );
	void Wait( void );
	void Stop( void );

	int BloodColor( void ) { return DONT_BLEED; }
	int Classify( void ) { return CLASS_MACHINE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	Vector BodyTarget( const Vector &posSrc ) { return GetAbsOrigin(); }
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

private:
	BOOL	m_on;
};

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
