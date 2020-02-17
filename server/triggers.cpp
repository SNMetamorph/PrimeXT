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

===== triggers.cpp ========================================================

  spawn and use functions for editor-placed triggers              

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "saverestore.h"
#include "trains.h"			// trigger_camera has train functionality
#include "gamerules.h"
#include "talkmonster.h"

// triggers
#define SF_TRIGGER_ALLOWMONSTERS	1	// monsters allowed to fire this trigger
#define SF_TRIGGER_NOCLIENTS		2	// players not allowed to fire this trigger
#define SF_TRIGGER_PUSHABLES		4	// only pushables can fire this trigger
#define SF_TRIGGER_CHECKANGLES	8	// Quake style triggers - fire when activator angles matched with trigger angles
#define SF_TRIGGER_ALLOWPHYSICS	16	// react on a rigid-bodies

#define SF_TRIGGER_PUSH_START_OFF	2	// spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_TARGETONCE	1	// Only fire hurt target once
#define SF_TRIGGER_HURT_START_OFF	2	// spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_NO_CLIENTS	8	// spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_CLIENTONLYFIRE	16	// trigger hurt will only fire its target if it is hurting a client
#define SF_TRIGGER_HURT_CLIENTONLYTOUCH 32	// only clients may touch this trigger.

extern DLL_GLOBAL BOOL		g_fGameOver;

extern Vector VecBModelOrigin( entvars_t* pevBModel );

class CFrictionModifier : public CBaseEntity
{
	DECLARE_CLASS( CFrictionModifier, CBaseEntity );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void ChangeFriction( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	float		m_frictionFraction;		// Sorry, couldn't resist this name :)
};

LINK_ENTITY_TO_CLASS( func_friction, CFrictionModifier );

// Global Savedata for changelevel friction modifier
BEGIN_DATADESC( CFrictionModifier )
	DEFINE_FIELD( m_frictionFraction, FIELD_FLOAT ),
	DEFINE_FUNCTION( ChangeFriction ),
END_DATADESC()

// Modify an entity's friction
void CFrictionModifier :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	pev->movetype = MOVETYPE_NONE;
	SetTouch( &ChangeFriction );
}


// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CFrictionModifier :: ChangeFriction( CBaseEntity *pOther )
{
	switch( pOther->pev->movetype )
	{
	case MOVETYPE_WALK:
	case MOVETYPE_STEP:
	case MOVETYPE_FLY:
	case MOVETYPE_TOSS:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_PUSHSTEP:
		pOther->pev->friction = m_frictionFraction;
		break;
	}
}

// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CFrictionModifier :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "modifier"))
	{
		m_frictionFraction = atof(pkvd->szValue) / 100.0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

#define SF_ACTIVATE_TRAIN		BIT( 0 )
#define SF_TRAINSPEED_DEBUG		BIT( 1 )

class CTrainSetSpeed : public CBaseDelay
{
	DECLARE_CLASS( CTrainSetSpeed, CBaseDelay );
public:
	void		Spawn( void );
	void		KeyValue( KeyValueData *pkvd );
	void 		Find( void );
	void 		UpdateSpeed( void );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE	GetState( void ) { return m_iState; }

	DECLARE_DATADESC();

	CFuncTrackTrain	*m_pTrain;
	int		m_iMode;
	float		m_flTime;
	float		m_flInterval;
};

LINK_ENTITY_TO_CLASS( train_setspeed, CTrainSetSpeed );

// Global Savedata for train speed modifier
BEGIN_DATADESC( CTrainSetSpeed )
	DEFINE_KEYFIELD( m_iMode, FIELD_CHARACTER, "mode" ),
	DEFINE_KEYFIELD( m_flTime, FIELD_FLOAT, "time" ),
	DEFINE_FIELD( m_flInterval, FIELD_FLOAT ),
	DEFINE_FIELD( m_pTrain, FIELD_CLASSPTR ),
	DEFINE_FUNCTION( UpdateSpeed ),
	DEFINE_FUNCTION( Find ),
END_DATADESC()

// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CTrainSetSpeed :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "time" ))
	{
		m_flTime = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "mode" ))
	{
		m_iMode = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "train" ))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CTrainSetSpeed :: Spawn( void )
{
	if( m_flTime <= 0 ) m_flTime = 10.0f;

	m_iState = STATE_OFF;
	m_hActivator = NULL;
//pev->spawnflags |= SF_TRAINSPEED_DEBUG;
	if( !FStrEq( STRING( pev->netname ), "*locus" ))
	{
		SetThink( &CTrainSetSpeed :: Find );
		SetNextThink( 0.1 );
	}
}

void CTrainSetSpeed :: Find( void )
{
	CBaseEntity *pEntity = NULL;

	while(( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->netname ), m_hActivator )) != NULL )
	{
		// found the tracktrain
		if( FClassnameIs( pEntity->pev, "func_tracktrain" ) )
		{
//			ALERT( at_console, "Found tracktrain: %s\n", STRING( pEntity->pev->targetname ));
			m_pTrain = (CFuncTrackTrain *)pEntity;
			break;
		}
	}
}

void CTrainSetSpeed :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( m_iState == STATE_ON )
		return;	// entity in work

	m_hActivator = pActivator;

	if( FStrEq( STRING( pev->netname ), "*locus" ) || !m_pTrain )
		Find();

	if( !m_pTrain ) return; // couldn't find train

	if( m_pTrain->m_pSpeedControl )
	{
		// cancelling previous controller...
		CTrainSetSpeed *pSpeedControl = (CTrainSetSpeed *)m_pTrain->m_pSpeedControl;
		pSpeedControl->m_iState = STATE_OFF;
		m_pTrain->m_pSpeedControl = NULL;
		pSpeedControl->DontThink();
	}

	if( !m_pTrain->GetSpeed( ))
	{
		if( !FBitSet( pev->spawnflags, SF_ACTIVATE_TRAIN ))
			return;

		// activate train before set speed
		if( FBitSet( pev->spawnflags, SF_TRAINSPEED_DEBUG ))
			ALERT( at_console, "train is activated\n" );
	}

	float m_flDelta;

	switch( m_iMode )
	{
	case 1:
		m_flDelta = ( pev->speed - m_pTrain->GetSpeed( ));
		m_flInterval = m_flDelta / (m_flTime / 0.1 ); // nextthink 0.1
		SetThink( &CTrainSetSpeed :: UpdateSpeed );
		m_pTrain->m_pSpeedControl = this; // now i'm control the train
		m_iState = STATE_ON; // i'm changing the train speed
		SetNextThink( 0.1 );
		break;
	case 0:
		m_pTrain->SetSpeedExternal( pev->speed );
		m_pTrain->m_pSpeedControl = NULL;
		break;
	}
}

void CTrainSetSpeed :: UpdateSpeed( void )
{
	if( !m_pTrain )
	{
		m_iState = STATE_OFF;
		return;	// no train or train not moving
          }

	if( FBitSet( pev->spawnflags, SF_TRAINSPEED_DEBUG ))
		ALERT( at_console, "train_setspeed: %s target speed %g, curspeed %g, step %g\n", GetTargetname(), pev->speed, m_pTrain->GetSpeed( ), m_flInterval );

	if( fabs( m_pTrain->GetSpeed() - pev->speed ) <= fabs( m_flInterval ))
	{
		if( pev->speed == 0.0f && FBitSet( pev->spawnflags, SF_ACTIVATE_TRAIN ))
		{
			if( FBitSet( pev->spawnflags, SF_TRAINSPEED_DEBUG ))
				ALERT( at_console, "train is stopped\n" );
			m_pTrain->SetSpeedExternal( 0 );
		}
		else if( pev->speed != 0.0f )
		{
			if( FBitSet( pev->spawnflags, SF_TRAINSPEED_DEBUG ))
				ALERT( at_console, "train is reached target speed: %g\n", pev->speed );
			m_pTrain->SetSpeedExternal( pev->speed );
		}

		m_iState = STATE_OFF;
		m_pTrain->m_pSpeedControl = NULL;
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
		return; // reached
	}

	m_pTrain->SetSpeedExternal( m_pTrain->GetSpeed() + m_flInterval );

	if( m_pTrain->GetSpeed() >= 2000 )
	{
		m_iState = STATE_OFF;
		m_pTrain->m_pSpeedControl = NULL;
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
		m_pTrain->SetSpeedExternal( 2000 );
		return;
	}

	if( m_pTrain->GetSpeed() <= -2000 )
	{
		m_iState = STATE_OFF;
		m_pTrain->m_pSpeedControl = NULL;
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
		m_pTrain->SetSpeedExternal( -2000 );
		return;
	}

	SetNextThink( 0.1 );	
}

#define SF_CONVSPEED_DEBUG		BIT( 0 )

class CConveyorSetSpeed : public CBaseDelay
{
	DECLARE_CLASS( CConveyorSetSpeed, CBaseDelay );
public:
	void		Spawn( void );
	void		KeyValue( KeyValueData *pkvd );
	BOOL		EvaluateSpeed( BOOL bStartup );
	void 		UpdateSpeed( void );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE	GetState( void ) { return m_iState; }

	DECLARE_DATADESC();

	float		m_flTime;
};

LINK_ENTITY_TO_CLASS( conveyor_setspeed, CConveyorSetSpeed );

// Global Savedata for train speed modifier
BEGIN_DATADESC( CConveyorSetSpeed )
	DEFINE_KEYFIELD( m_flTime, FIELD_FLOAT, "time" ),
	DEFINE_FUNCTION( UpdateSpeed ),
END_DATADESC()

// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CConveyorSetSpeed :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "time" ))
	{
		m_flTime = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CConveyorSetSpeed :: Spawn( void )
{
	m_iState = STATE_OFF;
	m_hActivator = NULL;
	pev->speed = bound( -10000.0f, pev->speed, 10000.0f );
}

BOOL CConveyorSetSpeed :: EvaluateSpeed( BOOL bStartup )
{
	CBaseEntity *pEntity = NULL;
	int affected_conveyors = 0;
	int finished_conveyors = 0;

	while(( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->target ), m_hActivator )) != NULL )
	{
		if( !FClassnameIs( pEntity->pev, "func_transporter" ) )
			continue;

		if( bStartup )
		{
			pEntity->pev->frags = pev->speed - pEntity->pev->speed;
			pEntity->pev->dmg_inflictor = edict();
			continue;
		}

		if( m_flTime > 0 && pEntity->pev->dmg_inflictor != edict( ))
			continue;	// owned by another set speed

		if( fabs( pev->speed - pEntity->pev->speed ) <= 1.0f || m_flTime <= 0 )
		{
			if( FBitSet( pev->spawnflags, SF_CONVSPEED_DEBUG ) && pev->speed != pEntity->pev->speed )
				ALERT( at_console, "conveyor_setspeed: %s target speed %g, curspeed %g\n", GetTargetname(), pev->speed, pEntity->pev->speed );
			pEntity->Use( this, this, USE_SET, pev->speed );
			pEntity->pev->dmg_inflictor = NULL; // done
			finished_conveyors++;
		}
		else
		{
			float flInterval = pEntity->pev->frags / ( m_flTime / 0.1 ); // nextthink 0.1
			pEntity->Use( this, this, USE_SET, pEntity->pev->speed + flInterval );
			if( FBitSet( pev->spawnflags, SF_CONVSPEED_DEBUG ))
				ALERT( at_console, "conveyor_setspeed: %s target speed %g, curspeed %g\n", GetTargetname(), pev->speed, pEntity->pev->speed );
		}
		affected_conveyors++;
	}

	return (affected_conveyors != finished_conveyors);
}

void CConveyorSetSpeed :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( m_iState == STATE_ON )
		return;	// entity in work

	m_hActivator = pActivator;

	if( m_flTime > 0 )
	{
		EvaluateSpeed( TRUE );
		SetThink( &CConveyorSetSpeed :: UpdateSpeed );
		m_iState = STATE_ON;
		SetNextThink( 0.1 );
	}
	else
	{
		EvaluateSpeed( FALSE );
	}
}

void CConveyorSetSpeed :: UpdateSpeed( void )
{
	if( !EvaluateSpeed( FALSE ))
	{
		if( FBitSet( pev->spawnflags, SF_CONVSPEED_DEBUG ))
			ALERT( at_console, "conveyor_setspeed: %s is done\n", GetTargetname( ));
		m_iState = STATE_OFF;
		return; // finished
          }

	SetNextThink( 0.1 );	
}

// This trigger will fire when the level spawns (or respawns if not fire once)
// It will check a global state before firing.  It supports delay and killtargets

#define SF_AUTO_FIREONCE		0x0001

class CAutoTrigger : public CBaseDelay
{
	DECLARE_CLASS( CAutoTrigger, CBaseDelay );
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Precache( void );
	void Think( void );
	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

private:
	int	m_globalstate;
	USE_TYPE	triggerType;
};
LINK_ENTITY_TO_CLASS( trigger_auto, CAutoTrigger );

BEGIN_DATADESC( CAutoTrigger )
	DEFINE_KEYFIELD( m_globalstate, FIELD_STRING, "globalstate" ),
	DEFINE_FIELD( triggerType, FIELD_INTEGER ),
END_DATADESC()

void CAutoTrigger::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "globalstate"))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = atoi( pkvd->szValue );
		switch( type )
		{
		case 0:
			triggerType = (USE_TYPE)-1;	// will be changed in spawn
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}


void CAutoTrigger::Spawn( void )
{
	if( triggerType == -1 )
		triggerType = USE_OFF;	// "triggerstate" is present and set to 'OFF'
	else if( !triggerType )
		triggerType = USE_ON;	// "triggerstate" is missing - defaulting to 'ON'

	// HACKHACK: run circles on a final map
	if( FStrEq( STRING( gpGlobals->mapname ), "c5a1" ) && FStrEq( GetTarget(), "hoop_1" ))
		triggerType = USE_ON;
#if 0
	// don't confuse level-designers with firing after loading savedgame
	if( m_globalstate == NULL_STRING )
		SetBits( pev->spawnflags, SF_AUTO_FIREONCE );
#endif
	Precache();
}

void CAutoTrigger::Precache( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
}

void CAutoTrigger::Think( void )
{
	if ( !m_globalstate || gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
	{
		SUB_UseTargets( this, triggerType, 0 );
		if ( pev->spawnflags & SF_AUTO_FIREONCE )
			UTIL_Remove( this );
	}
}



#define SF_RELAY_FIREONCE		0x0001

class CTriggerRelay : public CBaseDelay
{
	DECLARE_CLASS( CTriggerRelay, CBaseDelay );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();
private:
	USE_TYPE	triggerType;
	string_t	m_iszAltTarget;
};
LINK_ENTITY_TO_CLASS( trigger_relay, CTriggerRelay );

BEGIN_DATADESC( CTriggerRelay )
	DEFINE_FIELD( triggerType, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iszAltTarget, FIELD_STRING, "m_iszAltTarget" ),
END_DATADESC()

void CTriggerRelay::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "triggerstate" ))
	{
		int type = Q_atoi( pkvd->szValue );
		switch( type )
		{
		case 0:
			triggerType = (USE_TYPE)-1;	// will be changed in spawn
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		case 3:
			triggerType = USE_SET;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "value" ))
	{
		pev->scale = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszAltTarget" ))
	{
		m_iszAltTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CTriggerRelay::Spawn( void )
{
	if( triggerType == -1 )
		triggerType = USE_OFF;	// "triggerstate" is present and set to 'OFF'
	else if( !triggerType )
		triggerType = USE_ON;	// "triggerstate" is missing - defaulting to 'ON'

	// HACKHACK: allow Hihilanth working on a c4a3
	if( FStrEq( STRING( gpGlobals->mapname ), "c4a3" ) && FStrEq( GetTargetname(), "n_end_relay" ))
		triggerType = USE_OFF;		
}

void CTriggerRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
	{
		if( m_iszAltTarget )
			SUB_UseTargets( this, triggerType, (pev->scale != 0.0f) ? pev->scale : value, m_iszAltTarget );

		if( FBitSet( pev->spawnflags, SF_RELAY_FIREONCE ))
			UTIL_Remove( this );
		return;
	}

	SUB_UseTargets( this, triggerType, (pev->scale != 0.0f) ? pev->scale : value );

	if( FBitSet( pev->spawnflags, SF_RELAY_FIREONCE ))
		UTIL_Remove( this );
}


//**********************************************************
// The Multimanager Entity - when fired, will fire up to 16 targets 
// at specified times.
// FLAG:		THREAD (create clones when triggered)
// FLAG:		CLONE (this is a clone for a threaded execution)

#define SF_MULTIMAN_CLONE		0x80000000
#define SF_MULTIMAN_THREAD		BIT( 0 )
// used on a Valve maps
#define SF_MULTIMAN_LOOP		BIT( 2 )
#define SF_MULTIMAN_ONLYONCE		BIT( 3 )
#define SF_MULTIMAN_START_ON		BIT( 4 )	// same as START_ON

class CMultiManager : public CBaseDelay
{
	DECLARE_CLASS( CMultiManager, CBaseDelay );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void ManagerThink ( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
#if _DEBUG
	void ManagerReport( void );
#endif
	BOOL HasTarget( string_t targetname );
	int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_NOT_MASTER; }

	DECLARE_DATADESC();

	int	m_cTargets;			// the total number of targets in this manager's fire list.
	int	m_index;				// Current target
	float	m_startTime;			// Time we started firing
	int	m_iTargetName[MAX_MULTI_TARGETS];	// list if indexes into global string array
	float	m_flTargetDelay[MAX_MULTI_TARGETS];	// delay (in seconds) from time of manager fire to target fire
private:
	inline BOOL IsClone( void )
	{
		return (pev->spawnflags & SF_MULTIMAN_CLONE) ? TRUE : FALSE;
	}

	inline BOOL ShouldClone( void ) 
	{ 
		if( IsClone() )
			return FALSE;
		return (FBitSet( pev->spawnflags, SF_MULTIMAN_THREAD )) ? TRUE : FALSE; 
	}

	CMultiManager *Clone( void );
};

LINK_ENTITY_TO_CLASS( multi_manager, CMultiManager );

// Global Savedata for multi_manager
BEGIN_DATADESC( CMultiManager )
	DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
	DEFINE_FIELD( m_index, FIELD_INTEGER ),
	DEFINE_FIELD( m_startTime, FIELD_TIME ),
	DEFINE_AUTO_ARRAY( m_iTargetName, FIELD_STRING ),
	DEFINE_AUTO_ARRAY( m_flTargetDelay, FIELD_FLOAT ),
	DEFINE_FUNCTION( ManagerThink ),
#if _DEBUG
	DEFINE_FUNCTION( ManagerReport ),
#endif
END_DATADESC()

void CMultiManager :: KeyValue( KeyValueData *pkvd )
{
	// UNDONE: Maybe this should do something like this:
	// CBaseToggle::KeyValue( pkvd );
	// if ( !pkvd->fHandled )
	// ... etc.

	if( FStrEq( pkvd->szKeyName, "delay" ))
	{
		m_flDelay = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "master" ))
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "parent" ) || FStrEq( pkvd->szKeyName, "movewith" ))
	{
		m_iParent = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if( m_cTargets < MAX_MULTI_TARGETS )
		{
			char tmp[128];

			UTIL_StripToken( pkvd->szKeyName, tmp );
			m_iTargetName[m_cTargets] = ALLOC_STRING( tmp );
			m_flTargetDelay[m_cTargets] = Q_atof( pkvd->szValue );
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}


void CMultiManager :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetThink( &ManagerThink );

	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while( swapped )
	{
		swapped = 0;

		for( int i = 1; i < m_cTargets; i++ )
		{
			if( m_flTargetDelay[i] < m_flTargetDelay[i-1] )
			{
				// swap out of order elements
				int name = m_iTargetName[i];
				float delay = m_flTargetDelay[i];
				m_iTargetName[i] = m_iTargetName[i-1];
				m_flTargetDelay[i] = m_flTargetDelay[i-1];
				m_iTargetName[i-1] = name;
				m_flTargetDelay[i-1] = delay;
				swapped = 1;
			}
		}
	}

	// HACKHACK: fix env_laser on a c2a1
	if( FStrEq( STRING( gpGlobals->mapname ), "c2a1" ) && FStrEq( GetTargetname(), "gargbeams_mm" ))
		pev->spawnflags = 0;

	if( FBitSet( pev->spawnflags, SF_MULTIMAN_START_ON ))
	{		
		SetThink( &SUB_CallUseToggle );
		SetNextThink( 0.1 );
	}
	
 	m_iState = STATE_OFF;
}

BOOL CMultiManager::HasTarget( string_t targetname )
{ 
	for( int i = 0; i < m_cTargets; i++ )
	{
		if( FStrEq( STRING( targetname ), STRING( m_iTargetName[i] )) )
			return TRUE;
	}
	return FALSE;
}

// Designers were using this to fire targets that may or may not exist -- 
// so I changed it to use the standard target fire code, made it a little simpler.
void CMultiManager :: ManagerThink ( void )
{
	float	time;

	time = gpGlobals->time - m_startTime;
	while( m_index < m_cTargets && m_flTargetDelay[m_index] <= time )
	{
		UTIL_FireTargets( STRING( m_iTargetName[m_index] ), m_hActivator, this, USE_TOGGLE, pev->frags );
		m_index++;
	}

	// have we fired all targets?
	if( m_index >= m_cTargets )
	{
		if( FBitSet( pev->spawnflags, SF_MULTIMAN_LOOP ))
		{
			// starts new cycle
			m_startTime = m_flDelay + gpGlobals->time;
			m_iState = STATE_TURN_ON;
			SetNextThink( m_flDelay );
			SetThink( &ManagerThink );
			m_index = 0;
		}
		else
		{
			m_iState = STATE_OFF;
			SetThink( NULL );
			DontThink();
		}

		if( IsClone( ) || FBitSet( pev->spawnflags, SF_MULTIMAN_ONLYONCE ))
		{
			UTIL_Remove( this );
			return;
		}
	}
	else
	{
		pev->nextthink = m_startTime + m_flTargetDelay[m_index];
	}
}

CMultiManager *CMultiManager::Clone( void )
{
	CMultiManager *pMulti = GetClassPtr(( CMultiManager *)NULL );

	edict_t *pEdict = pMulti->pev->pContainingEntity;
	memcpy( pMulti->pev, pev, sizeof( *pev ));
	pMulti->pev->pContainingEntity = pEdict;

	pMulti->pev->spawnflags |= SF_MULTIMAN_CLONE;
	pMulti->pev->spawnflags &= ~SF_MULTIMAN_THREAD; // g-cont. to prevent recursion
	memcpy( pMulti->m_iTargetName, m_iTargetName, sizeof( m_iTargetName ));
	memcpy( pMulti->m_flTargetDelay, m_flTargetDelay, sizeof( m_flTargetDelay ));
	pMulti->m_cTargets = m_cTargets;

	return pMulti;
}


// The USE function builds the time table and starts the entity thinking.
void CMultiManager :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	pev->frags = value;

	if( IsLockedByMaster( ))
		return;

 	if( useType == USE_SET )
 	{ 
		m_index = 0; // reset fire index
		while( m_index < m_cTargets )
		{
			// firing all targets instantly
			UTIL_FireTargets( m_iTargetName[ m_index ], this, this, USE_TOGGLE );
			m_index++;
			if( m_hActivator == this )
				break; // break if current target - himself
		}
 	}

	if( FBitSet( pev->spawnflags, SF_MULTIMAN_LOOP ))
	{
		if( m_iState != STATE_OFF ) // if we're on, or turning on...
		{
			if( useType != USE_ON ) // ...then turn it off if we're asked to.
			{
				m_iState = STATE_OFF;
				if( IsClone() || FBitSet( pev->spawnflags, SF_MULTIMAN_ONLYONCE ))
				{
					SetThink( &SUB_Remove );
					SetNextThink( 0.1 );
				}
				else
				{
					SetThink( NULL );
					DontThink();
				}
			}
			return;
		}
		else if( useType == USE_OFF )
		{
			return;
		}
		// otherwise, start firing targets as normal.
	}

	// In multiplayer games, clone the MM and execute in the clone (like a thread)
	// to allow multiple players to trigger the same multimanager
	if( ShouldClone( ))
	{
		CMultiManager *pClone = Clone();
		pClone->Use( pActivator, pCaller, useType, value );
		return;
	}

	if( ShouldToggle( useType ))
	{
		if( useType == USE_ON || useType == USE_TOGGLE || useType == USE_RESET )
		{
			if( m_iState == STATE_OFF || useType == USE_RESET )
			{
				m_startTime = m_flDelay + gpGlobals->time;
				m_iState = STATE_TURN_ON;
				SetNextThink( m_flDelay );
				SetThink( &ManagerThink );
				m_index = 0;
			}
		}	
		else if( useType == USE_OFF )
		{	
			m_iState = STATE_OFF;
			SetThink( NULL );
			DontThink();
		}
	}
}

#if _DEBUG
void CMultiManager :: ManagerReport ( void )
{
	for( int cIndex = 0; cIndex < m_cTargets; cIndex++ )
	{
		ALERT( at_console, "%s %f\n", STRING(m_iTargetName[cIndex]), m_flTargetDelay[cIndex] );
	}
}
#endif

//***********************************************************


//=======================================================================
// 		   multi_watcher
//=======================================================================
#define SF_WATCHER_START_OFF	1

#define LOGIC_AND  		0 // fire if all objects active
#define LOGIC_OR   		1 // fire if any object active
#define LOGIC_NAND 		2 // fire if not all objects active
#define LOGIC_NOR  		3 // fire if all objects disable
#define LOGIC_XOR		4 // fire if only one (any) object active
#define LOGIC_XNOR		5 // fire if active any number objects, but < then all

class CMultiMaster : public CBaseDelay
{
	DECLARE_CLASS( CMultiMaster, CBaseDelay );
public:
	void Spawn( void );
	void Think( void );
	void KeyValue( KeyValueData *pkvd );
	int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE GetState( CBaseEntity *pActivator ) { return EvalLogic( pActivator ) ? STATE_ON : STATE_OFF; };
	virtual STATE GetState( void ) { return m_iState; };
	int GetLogicModeForString( const char *string );
	bool CheckState( STATE state, int targetnum );

	DECLARE_DATADESC();

	int	m_cTargets; // the total number of targets in this manager's fire list.
	int	m_iTargetName[MAX_MULTI_TARGETS]; // list of indexes into global string array
          STATE	m_iTargetState[MAX_MULTI_TARGETS]; // list of wishstate targets
	STATE	m_iSharedState;
	int	m_iLogicMode;
	
	BOOL	EvalLogic( CBaseEntity *pEntity );
	bool	globalstate;
};

LINK_ENTITY_TO_CLASS( multi_watcher, CMultiMaster );

BEGIN_DATADESC( CMultiMaster )
	DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iLogicMode, FIELD_INTEGER, "logic" ),
	DEFINE_FIELD( m_iSharedState, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iTargetName, FIELD_STRING ),
	DEFINE_AUTO_ARRAY( m_iTargetState, FIELD_INTEGER ),
END_DATADESC()

void CMultiMaster :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "logic" ))
	{
		m_iLogicMode = GetLogicModeForString( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "state" ))
	{
		m_iSharedState = GetStateForString( pkvd->szValue );
		pkvd->fHandled = TRUE;
		globalstate = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "offtarget" ))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "parent" ) || FStrEq( pkvd->szKeyName, "movewith" ))
	{
		m_iParent = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if( m_cTargets < MAX_MULTI_TARGETS )
		{
			char tmp[128];
			
			UTIL_StripToken( pkvd->szKeyName, tmp );
			m_iTargetName[m_cTargets] = ALLOC_STRING( tmp );
			m_iTargetState[m_cTargets] = GetStateForString( pkvd->szValue );
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
	}
}

int CMultiMaster :: GetLogicModeForString( const char *string )
{
	if( !Q_stricmp( string, "AND" ))
		return LOGIC_AND;
	else if( !Q_stricmp( string, "OR" ))
		return LOGIC_OR;
	else if( !Q_stricmp( string, "NAND" ) || !Q_stricmp( string, "!AND" ))
		return LOGIC_NAND;
	else if( !Q_stricmp( string, "NOR" ) || !Q_stricmp( string, "!OR" ))
		return LOGIC_NOR;
	else if( !Q_stricmp( string, "XOR" ) || !Q_stricmp( string, "^OR" ))
		return LOGIC_XOR;
	else if( !Q_stricmp( string, "XNOR" ) || !Q_stricmp( string, "^!OR" ))
		return LOGIC_XNOR;
	else if( Q_isdigit( string ))
		return Q_atoi( string );

	// assume error
	ALERT( at_error, "Unknown logic mode '%s' specified\n", string );
	return -1;
}

void CMultiMaster :: Spawn( void )
{
	// use local states instead
	if( !globalstate )
	{
		m_iSharedState = (STATE)-1;
	}

	if( !FBitSet( pev->spawnflags, SF_WATCHER_START_OFF ))
		SetNextThink( 0.1 );
}

bool CMultiMaster :: CheckState( STATE state, int targetnum )
{
	// global state for all targets
	if( m_iSharedState != -1 )
	{
		if( m_iSharedState == state )
			return TRUE;
		return FALSE;
	}

	if((STATE)m_iTargetState[targetnum] == state )
		return TRUE;
	return FALSE;
}

void CMultiMaster :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !ShouldToggle( useType, ( pev->nextthink != 0 )))
		return;

	if( pev->nextthink == 0 )
	{
 		SetNextThink( 0.01 );
	}
	else
	{
		// disabled watcher is always off
		m_iState = STATE_OFF;
		DontThink();
	}
}

void CMultiMaster :: Think ( void )
{
	if( EvalLogic( NULL )) 
	{
		if( m_iState == STATE_OFF )
		{
			m_iState = STATE_ON;
			UTIL_FireTargets( pev->target, this, this, USE_ON );
		}
	}
	else 
	{
		if( m_iState == STATE_ON )
		{
			m_iState = STATE_OFF;
			UTIL_FireTargets( pev->netname, this, this, USE_OFF );
		}
          }

 	SetNextThink( 0.01 );
}

BOOL CMultiMaster :: EvalLogic( CBaseEntity *pActivator )
{
	BOOL xorgot = FALSE;
	CBaseEntity *pEntity;

	for( int i = 0; i < m_cTargets; i++ )
	{
		pEntity = UTIL_FindEntityByTargetname( NULL, STRING( m_iTargetName[i] ), pActivator );
		if( !pEntity ) continue;

		// handle the states for this logic mode
		if( CheckState( pEntity->GetState( ), i ))
		{
			switch( m_iLogicMode )
			{
			case LOGIC_OR:
				return TRUE;
			case LOGIC_NOR:
				return FALSE;
			case LOGIC_XOR: 
				if( xorgot )
					return FALSE;
				xorgot = TRUE;
				break;
			case LOGIC_XNOR:
				if( xorgot )
					return TRUE;
				xorgot = TRUE;
				break;
			}
		}
		else // state is false
		{
			switch( m_iLogicMode )
			{
	         		case LOGIC_AND:
	         			return FALSE;
			case LOGIC_NAND:
				return TRUE;
			}
		}
	}

	// handle the default cases for each logic mode
	switch( m_iLogicMode )
	{
	case LOGIC_AND:
	case LOGIC_NOR:
		return TRUE;
	case LOGIC_XOR:
		return xorgot;
	case LOGIC_XNOR:
		return !xorgot;
	default:
		return FALSE;
	}
}

//=======================================================================
// 		   multi_switcher
//=======================================================================
#define MODE_INCREMENT		0
#define MODE_DECREMENT		1
#define MODE_RANDOM_VALUE		2

#define SF_SWITCHER_START_ON		BIT( 0 )

class CSwitcher : public CBaseDelay
{
	DECLARE_CLASS( CSwitcher, CBaseDelay );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Spawn( void );
	void Think( void );

	DECLARE_DATADESC();

	int m_iTargetName[MAX_MULTI_TARGETS]; // list if indexes into global string array
	int m_iTargetOrder[MAX_MULTI_TARGETS];	// don't save\restore
	int m_cTargets; // the total number of targets in this manager's fire list.
	int m_index; // Current target
};

LINK_ENTITY_TO_CLASS( multi_switcher, CSwitcher );

// Global Savedata for switcher
BEGIN_DATADESC( CSwitcher )
	DEFINE_FIELD( m_index, FIELD_INTEGER ),
	DEFINE_FIELD( m_cTargets, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( m_iTargetName, FIELD_STRING ),
END_DATADESC()

void CSwitcher :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "mode" ))
	{
		pev->button = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( m_cTargets < MAX_MULTI_TARGETS ) 
	{
		// add this field to the target list
		// this assumes that additional fields are targetnames and their values are delay values.
		
		char tmp[128];
		UTIL_StripToken( pkvd->szKeyName, tmp );
		m_iTargetName[m_cTargets] = ALLOC_STRING( tmp );
		m_iTargetOrder[m_cTargets] = Q_atoi( pkvd->szValue ); 
		m_cTargets++;
		pkvd->fHandled = TRUE;
	}
}

void CSwitcher :: Spawn( void )
{
	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while( swapped )
	{
		swapped = 0;

		for( int i = 1; i < m_cTargets; i++ )
		{
			if( m_iTargetOrder[i] < m_iTargetOrder[i-1] )
			{
				// swap out of order elements
				int name = m_iTargetName[i];
				int order = m_iTargetOrder[i];
				m_iTargetName[i] = m_iTargetName[i-1];
				m_iTargetOrder[i] = m_iTargetOrder[i-1];
				m_iTargetName[i-1] = name;
				m_iTargetOrder[i-1] = order;
				swapped = 1;
			}
		}
	}

	m_iState = STATE_OFF;
	m_index = 0;

	if( FBitSet( pev->spawnflags, SF_SWITCHER_START_ON ))
	{
 		SetNextThink( m_flDelay );
		m_iState = STATE_ON;
	}	
}

void CSwitcher :: Think( void )
{
	if( pev->button == MODE_INCREMENT )
	{
		// increase target number
		m_index++;
		if( m_index >= m_cTargets )
			m_index = 0;
	}
	else if( pev->button == MODE_DECREMENT )
	{
		m_index--;
		if( m_index < 0 )
			m_index = m_cTargets - 1;
	}
	else if( pev->button == MODE_RANDOM_VALUE )
	{
		m_index = RANDOM_LONG( 0, m_cTargets - 1 );
	}

	SetNextThink( m_flDelay );
}

void CSwitcher :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if( IsLockedByMaster( ))
		return;

	if( useType == USE_SET )
	{
		// set new target for activate (direct choose or increment\decrement)
		if( FBitSet( pev->spawnflags, SF_SWITCHER_START_ON ))
		{
			m_iState = STATE_ON;
			SetNextThink( m_flDelay );
			return;
		}

		// set maximum priority for direct choose
		if( value ) 
		{
			m_index = (value - 1);
			if( m_index >= m_cTargets || m_index < -1 )
				m_index = -1;
			return;
		}

		if( pev->button == MODE_INCREMENT )
		{
			m_index++;
			if( m_index >= m_cTargets )
				m_index = 0; 	
		}
                    else if( pev->button == MODE_DECREMENT )
		{
			m_index--;
			if( m_index < 0 )
				m_index = m_cTargets - 1;
		}
		else if( pev->button == MODE_RANDOM_VALUE )
		{
			m_index = RANDOM_LONG( 0, m_cTargets - 1 );
		}
	}
	else if( useType == USE_RESET )
	{
		// reset switcher
		m_iState = STATE_OFF;
		DontThink();
		m_index = 0;
		return;
	}
	else if( m_index != -1 ) // fire any other USE_TYPE and right index
	{
		UTIL_FireTargets( m_iTargetName[m_index], m_hActivator, this, useType, value );
	}
}

//
// Render parameters trigger
//
// This entity will copy its render parameters (renderfx, rendermode, rendercolor, renderamt)
// to its targets when triggered.
//


// Flags to indicate masking off various render parameters that are normally copied to the targets
#define SF_RENDER_MASKFX	(1<<0)
#define SF_RENDER_MASKAMT	(1<<1)
#define SF_RENDER_MASKMODE	(1<<2)
#define SF_RENDER_MASKCOLOR	(1<<3)
//LRC
#define SF_RENDER_KILLTARGET	(1<<5)
#define SF_RENDER_ONLYONCE		(1<<6)

//LRC-  RenderFxFader, a subsidiary entity for RenderFxManager
class CRenderFxFader : public CBaseEntity
{
	DECLARE_CLASS( CRenderFxFader, CBaseEntity );
public:
	void Spawn( void );
	void FadeThink( void );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	float m_flStartTime;
	float m_flDuration;
	float m_flCoarseness;
	int m_iStartAmt;
	int m_iOffsetAmt;
	Vector m_vecStartColor;
	Vector m_vecOffsetColor;
	float m_fStartScale;
	float m_fOffsetScale;
	EHANDLE m_hTarget;

	int m_iszAmtFactor;
};

LINK_ENTITY_TO_CLASS( fader, CRenderFxFader );

BEGIN_DATADESC( CRenderFxFader )
	DEFINE_FIELD( m_flStartTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDuration, FIELD_FLOAT ),
	DEFINE_FIELD( m_flCoarseness, FIELD_FLOAT ),
	DEFINE_FIELD( m_iStartAmt, FIELD_INTEGER ),
	DEFINE_FIELD( m_iOffsetAmt, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecStartColor, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecOffsetColor, FIELD_VECTOR ),
	DEFINE_FIELD( m_fStartScale, FIELD_FLOAT),
	DEFINE_FIELD( m_fOffsetScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),
	DEFINE_FUNCTION( FadeThink ),
END_DATADESC()

void CRenderFxFader :: Spawn( void )
{
	SetThink( &FadeThink );
}

void CRenderFxFader :: FadeThink( void )
{
	if ( m_hTarget == NULL )
	{
		SUB_Remove();
		return;
	}

	float flDegree = (gpGlobals->time - m_flStartTime) / m_flDuration;

	if (flDegree >= 1)
	{
		m_hTarget->pev->renderamt = m_iStartAmt + m_iOffsetAmt;
		m_hTarget->pev->rendercolor = m_vecStartColor + m_vecOffsetColor;
		m_hTarget->pev->scale = m_fStartScale + m_fOffsetScale;

		SUB_UseTargets( m_hTarget, USE_TOGGLE, 0 );

		if (pev->spawnflags & SF_RENDER_KILLTARGET)
		{
			m_hTarget->SetThink( &SUB_Remove );
			m_hTarget->SetNextThink( 0.1 );
		}

		m_hTarget = NULL;

		SetNextThink( 0.1 );
		SetThink( &SUB_Remove );
	}
	else
	{
		m_hTarget->pev->renderamt = m_iStartAmt + m_iOffsetAmt * flDegree;

		m_hTarget->pev->rendercolor.x = m_vecStartColor.x + m_vecOffsetColor.x * flDegree;
		m_hTarget->pev->rendercolor.y = m_vecStartColor.y + m_vecOffsetColor.y * flDegree;
		m_hTarget->pev->rendercolor.z = m_vecStartColor.z + m_vecOffsetColor.z * flDegree;

		m_hTarget->pev->scale = m_fStartScale + m_fOffsetScale * flDegree;

		SetNextThink( m_flCoarseness );
	}
}

// RenderFxManager itself
class CRenderFxManager : public CPointEntity
{
	DECLARE_CLASS( CRenderFxManager, CPointEntity );
public:
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Affect( CBaseEntity *pEntity, BOOL bIsLocus, CBaseEntity *pActivator );

	void KeyValue( KeyValueData *pkvd );
};

LINK_ENTITY_TO_CLASS( env_render, CRenderFxManager );

void CRenderFxManager :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_fScale"))
	{
		pev->scale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CRenderFxManager :: Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (!FStringNull(pev->target))
	{
		CBaseEntity* pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->target), pActivator);
		BOOL first = TRUE;
		while ( pTarget != NULL )
		{
			Affect( pTarget, first, pActivator );
			first = FALSE;
			pTarget = UTIL_FindEntityByTargetname( pTarget, STRING(pev->target), pActivator );
		}
	}

	if (pev->spawnflags & SF_RENDER_ONLYONCE)
	{
		SetThink( &SUB_Remove );
		SetNextThink( 0.1 );
	}
}

void CRenderFxManager::Affect( CBaseEntity *pTarget, BOOL bIsFirst, CBaseEntity *pActivator )
{
	entvars_t *pevTarget = pTarget->pev;

	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKFX ) )
		pevTarget->renderfx = pev->renderfx;

	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKMODE ) )
	{
		//LRC - amt is often 0 when mode is normal. Set it to be fully visible, for fade purposes.
		if (pev->frags && pevTarget->renderamt == 0 && pevTarget->rendermode == kRenderNormal)
			pevTarget->renderamt = 255;
		pevTarget->rendermode = pev->rendermode;
	}

	if (pev->frags == 0) // not fading?
	{
		if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKAMT ) )
			pevTarget->renderamt = pev->renderamt;
		if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKCOLOR ) )
			pevTarget->rendercolor = pev->rendercolor;
		if ( pev->scale )
			pevTarget->scale = pev->scale;

		if (bIsFirst)
			UTIL_FireTargets( STRING(pev->netname), pTarget, this, USE_TOGGLE, 0 );
	}
	else
	{
		//LRC - fade the entity in/out!
		// (We create seperate fader entities to do this, one for each entity that needs fading.)
		CRenderFxFader *pFader = GetClassPtr( (CRenderFxFader *)NULL );
		pFader->pev->classname = MAKE_STRING( "fader" );
		pFader->m_hTarget = pTarget;
		pFader->m_iStartAmt = pevTarget->renderamt;
		pFader->m_vecStartColor = pevTarget->rendercolor;
		pFader->m_fStartScale = pevTarget->scale;
		if (pFader->m_fStartScale == 0)
			pFader->m_fStartScale = 1; // When we're scaling, 0 is treated as 1. Use 1 as the number to fade from.
		pFader->pev->spawnflags = pev->spawnflags;

		if (bIsFirst)
			pFader->pev->target = pev->netname;

		if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKAMT ) )
			pFader->m_iOffsetAmt = pev->renderamt - pevTarget->renderamt;
		else
			pFader->m_iOffsetAmt = 0;

		if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKCOLOR ) )
		{
			pFader->m_vecOffsetColor.x = pev->rendercolor.x - pevTarget->rendercolor.x;
			pFader->m_vecOffsetColor.y = pev->rendercolor.y - pevTarget->rendercolor.y;
			pFader->m_vecOffsetColor.z = pev->rendercolor.z - pevTarget->rendercolor.z;
		}
		else
		{
			pFader->m_vecOffsetColor = g_vecZero;
		}

		if ( pev->scale )
			pFader->m_fOffsetScale = pev->scale - pevTarget->scale;
		else
			pFader->m_fOffsetScale = 0;

		pFader->m_flStartTime = gpGlobals->time;
		pFader->m_flDuration = pev->frags;
		pFader->m_flCoarseness = pev->armorvalue;
		pFader->SetNextThink( 0 );
		pFader->Spawn();
	}
}

//***********************************************************
//
// EnvCustomize
//
// Changes various properties of an entity (some properties only apply to monsters.)
//

#define SF_CUSTOM_AFFECTDEAD	1
#define SF_CUSTOM_ONCE			2
#define SF_CUSTOM_DEBUG			4

#define CUSTOM_FLAG_NOCHANGE	0
#define CUSTOM_FLAG_ON			1
#define CUSTOM_FLAG_OFF			2
#define CUSTOM_FLAG_TOGGLE		3
#define CUSTOM_FLAG_USETYPE		4
#define CUSTOM_FLAG_INVUSETYPE	5

class CEnvCustomize : public CBaseEntity
{
	DECLARE_CLASS( CEnvCustomize, CBaseEntity );
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void Affect (CBaseEntity *pTarget, USE_TYPE useType);
	int GetActionFor( int iField, int iActive, USE_TYPE useType, char *szDebug );
	void SetBoneController (float fController, int cnum, CBaseEntity *pTarget);

	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();

	int	m_iszModel;
	int	m_iClass;
	int	m_iPlayerReact;
	int	m_iPrisoner;
	int	m_iMonsterClip;
	int	m_iVisible;
	int	m_iSolid;
	int	m_iProvoked;
	int	m_voicePitch;
	int	m_iBloodColor;
	float	m_fFramerate;
	float	m_fController0;
	float	m_fController1;
	float	m_fController2;
	float	m_fController3;
	int	m_iReflection;	// reflection style
};

LINK_ENTITY_TO_CLASS( env_customize, CEnvCustomize );

BEGIN_DATADESC( CEnvCustomize )
	DEFINE_KEYFIELD( m_iszModel, FIELD_STRING, "m_iszModel" ),
	DEFINE_KEYFIELD( m_iClass, FIELD_INTEGER, "m_iClass" ),
	DEFINE_KEYFIELD( m_iPlayerReact, FIELD_INTEGER, "m_iPlayerReact" ),
	DEFINE_KEYFIELD( m_iPrisoner, FIELD_INTEGER, "m_iPrisoner" ),
	DEFINE_KEYFIELD( m_iMonsterClip, FIELD_INTEGER, "m_iMonsterClip" ),
	DEFINE_KEYFIELD( m_iVisible, FIELD_INTEGER, "m_iVisible" ),
	DEFINE_KEYFIELD( m_iSolid, FIELD_INTEGER, "m_iSolid" ),
	DEFINE_KEYFIELD( m_iProvoked, FIELD_INTEGER, "m_iProvoked" ),
	DEFINE_KEYFIELD( m_voicePitch, FIELD_INTEGER, "m_voicePitch" ),
	DEFINE_KEYFIELD( m_iBloodColor, FIELD_INTEGER, "m_iBloodColor" ),
	DEFINE_KEYFIELD( m_fFramerate, FIELD_FLOAT, "m_fFramerate" ),
	DEFINE_KEYFIELD( m_fController0, FIELD_FLOAT, "m_fController0" ),
	DEFINE_KEYFIELD( m_fController1, FIELD_FLOAT, "m_fController1" ),
	DEFINE_KEYFIELD( m_fController2, FIELD_FLOAT, "m_fController2" ),
	DEFINE_KEYFIELD( m_fController3, FIELD_FLOAT, "m_fController3" ),
	DEFINE_KEYFIELD( m_iReflection, FIELD_INTEGER, "m_iReflection" ),
END_DATADESC()

void CEnvCustomize :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iVisible"))
	{
		m_iVisible = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iSolid"))
	{
		m_iSolid = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszModel"))
	{
		m_iszModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_voicePitch"))
	{
		m_voicePitch = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPrisoner"))
	{
		m_iPrisoner = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iMonsterClip"))
	{
		m_iMonsterClip = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iClass"))
	{
		m_iClass = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPlayerReact"))
	{
		m_iPlayerReact = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iProvoked"))
	{
		m_iProvoked = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_bloodColor") || FStrEq(pkvd->szKeyName, "m_iBloodColor"))
	{
		m_iBloodColor = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fFramerate"))
	{
		m_fFramerate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fController0"))
	{
		m_fController0 = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fController1"))
	{
		m_fController1 = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fController2"))
	{
		m_fController2 = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fController3"))
	{
		m_fController3 = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iReflection"))
	{
		m_iReflection = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CEnvCustomize :: Spawn ( void )
{
	Precache();

	if (!pev->targetname)
	{
		SetThink( &SUB_CallUseToggle );
		SetNextThink( 0.1 );
	}
}

void CEnvCustomize :: Precache( void )
{
	if (m_iszModel)
		PRECACHE_MODEL((char*)STRING(m_iszModel));
}

void CEnvCustomize :: Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull(pev->target) )
	{
		if ( pActivator )
			Affect(pActivator, useType);
		else if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, "DEBUG: env_customize \"%s\" was fired without a locus!\n", STRING(pev->targetname));
	}
	else
	{
		BOOL fail = TRUE;
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname(NULL, STRING(pev->target), pActivator);
		while (pTarget)
		{
			Affect(pTarget, useType);
			fail = FALSE;
			pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(pev->target), pActivator);
		}
		pTarget = UTIL_FindEntityByClassname(NULL, STRING(pev->target));
		while (pTarget)
		{
			Affect(pTarget, useType);
			fail = FALSE;
			pTarget = UTIL_FindEntityByClassname(pTarget, STRING(pev->target));
		}
		if (fail && pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, "DEBUG: env_customize \"%s\" does nothing; can't find any entity with name or class \"%s\".\n", STRING(pev->target));
	}

	if (pev->spawnflags & SF_CUSTOM_ONCE)
	{
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, "DEBUG: env_customize \"%s\" removes itself.\n", STRING(pev->targetname));
		UTIL_Remove(this);
	}
}

void CEnvCustomize :: Affect (CBaseEntity *pTarget, USE_TYPE useType)
{
	CBaseMonster* pMonster = pTarget->MyMonsterPointer();
	if (!FBitSet(pev->spawnflags, SF_CUSTOM_AFFECTDEAD) && pMonster && pMonster->m_MonsterState == MONSTERSTATE_DEAD)
	{
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, "DEBUG: env_customize %s does nothing; can't apply to a corpse.\n", STRING(pev->targetname));
		return;
	}

	if (pev->spawnflags & SF_CUSTOM_DEBUG)
		ALERT(at_console, "DEBUG: env_customize \"%s\" affects %s \"%s\": [", STRING(pev->targetname), STRING(pTarget->pev->classname), STRING(pTarget->pev->targetname));

	if (m_iszModel)
	{
		Vector vecMins, vecMaxs;
		vecMins = pTarget->pev->mins;
		vecMaxs = pTarget->pev->maxs;
		SET_MODEL(pTarget->edict(),STRING(m_iszModel));
		UTIL_SetSize(pTarget->pev,vecMins,vecMaxs);
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " model=%s", STRING(m_iszModel));
	}

	SetBoneController( m_fController0, 0, pTarget );
	SetBoneController( m_fController1, 1, pTarget );
	SetBoneController( m_fController2, 2, pTarget );
	SetBoneController( m_fController3, 3, pTarget );

	if (m_fFramerate != -1)
	{
		//FIXME: check for env_model, stop it from changing its own framerate
		pTarget->pev->framerate = m_fFramerate;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " framerate=%f", m_fFramerate);
	}
	if (pev->body != -1)
	{
		pTarget->pev->body = pev->body;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " body = %d", pev->body);
	}
	if (pev->skin != -1)
	{
		if (pev->skin == -2)
		{
			if (pTarget->pev->skin)
			{
				pTarget->pev->skin = 0;
				if (pev->spawnflags & SF_CUSTOM_DEBUG)
					ALERT(at_console, " skin=0");
			}
			else
			{
				pTarget->pev->skin = 1;
				if (pev->spawnflags & SF_CUSTOM_DEBUG)
					ALERT(at_console, " skin=1");
			}
		}
		else if (pev->skin == -99) // special option to set CONTENTS_EMPTY
		{
			pTarget->pev->skin = -1;
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " skin=-1");
		}
		else
		{
			pTarget->pev->skin = pev->skin;
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " skin=%d", pTarget->pev->skin);
		}
	}

	if( m_iReflection != -1 )
	{
		switch( m_iReflection )
		{
		case 0:
			pTarget->pev->effects &= ~EF_NOREFLECT;
			pTarget->pev->effects &= ~EF_REFLECTONLY;
			break;
		case 1:
			pTarget->pev->effects |= EF_NOREFLECT;
			pTarget->pev->effects &= ~EF_REFLECTONLY;
			break;
		case 2:
			pTarget->pev->effects |= EF_REFLECTONLY;
			pTarget->pev->effects &= ~EF_NOREFLECT;
			break;					
		}
	}

	switch ( GetActionFor(m_iVisible, !(pTarget->pev->effects & EF_NODRAW), useType, "visible"))
	{
	case CUSTOM_FLAG_ON: pTarget->pev->effects &= ~EF_NODRAW; break;
	case CUSTOM_FLAG_OFF: pTarget->pev->effects |= EF_NODRAW; break;
	}

	switch ( GetActionFor(m_iSolid, pTarget->pev->solid != SOLID_NOT, useType, "solid"))
	{
	case CUSTOM_FLAG_ON: pTarget->RestoreSolid(); break;
	case CUSTOM_FLAG_OFF: pTarget->MakeNonSolid(); break;
	}

	if (!pMonster)
	{
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " ]\n");
		return;
	}

	if (m_iBloodColor != 0)
	{
		pMonster->m_bloodColor = m_iBloodColor;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " bloodcolor=%d", m_iBloodColor);
	}
	if (m_voicePitch > 0)
	{
		if (FClassnameIs(pTarget->pev,"monster_barney") || FClassnameIs(pTarget->pev,"monster_scientist") || FClassnameIs(pTarget->pev,"monster_sitting_scientist"))
		{
			((CTalkMonster*)pTarget)->m_voicePitch = m_voicePitch;
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " voicePitch(talk)=%d", m_voicePitch);
		}
		else if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " voicePitch=unchanged");
	}

	if (m_iClass != 0)
	{
		pMonster->m_iClass = m_iClass;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " class=%d", m_iClass);
		if (pMonster->m_hEnemy)
		{
			pMonster->m_hEnemy = NULL;
			// make 'em stop attacking... might be better to use a different signal?
			pMonster->SetConditions( bits_COND_NEW_ENEMY );
		}
	}
	if (m_iPlayerReact != -1)
	{
		pMonster->m_iPlayerReact = m_iPlayerReact;
		if (pev->spawnflags & SF_CUSTOM_DEBUG)
			ALERT(at_console, " playerreact=%d", m_iPlayerReact);
	}

	switch (GetActionFor(m_iPrisoner, pMonster->pev->spawnflags & SF_MONSTER_PRISONER, useType, "prisoner") )
	{
	case CUSTOM_FLAG_ON:
		pMonster->pev->spawnflags |= SF_MONSTER_PRISONER;
		if (pMonster->m_hEnemy)
		{
			pMonster->m_hEnemy = NULL;
			// make 'em stop attacking... might be better to use a different signal?
			pMonster->SetConditions( bits_COND_NEW_ENEMY );
		}
		break;
	case CUSTOM_FLAG_OFF:
		pMonster->pev->spawnflags &= ~SF_MONSTER_PRISONER;
		break;
	}

	switch (GetActionFor(m_iMonsterClip, pMonster->pev->flags & FL_MONSTERCLIP, useType, "monsterclip") )
	{
	case CUSTOM_FLAG_ON: pMonster->pev->flags |= FL_MONSTERCLIP; break;
	case CUSTOM_FLAG_OFF: pMonster->pev->flags &= ~FL_MONSTERCLIP; break;
	}

	switch (GetActionFor(m_iProvoked, pMonster->m_afMemory & bits_MEMORY_PROVOKED, useType, "provoked") )
	{
	case CUSTOM_FLAG_ON: pMonster->Remember(bits_MEMORY_PROVOKED); break;
	case CUSTOM_FLAG_OFF: pMonster->Forget(bits_MEMORY_PROVOKED); break;
	}

	if (pev->spawnflags & SF_CUSTOM_DEBUG)
		ALERT(at_console, " ]\n");
}

int CEnvCustomize::GetActionFor( int iField, int iActive, USE_TYPE useType, char* szDebug)
{
	int iAction = iField;

	if (iAction == CUSTOM_FLAG_USETYPE)
	{
		if (useType == USE_ON)
			iAction = CUSTOM_FLAG_ON;
		else if (useType == USE_OFF)
			iAction = CUSTOM_FLAG_OFF;
		else
			iAction = CUSTOM_FLAG_TOGGLE;
	}
	else if (iAction == CUSTOM_FLAG_INVUSETYPE)
	{
		if (useType == USE_ON)
			iAction = CUSTOM_FLAG_OFF;
		else if (useType == USE_OFF)
			iAction = CUSTOM_FLAG_ON;
		else
			iAction = CUSTOM_FLAG_TOGGLE;
	}

	if (iAction == CUSTOM_FLAG_TOGGLE)
	{
		if (iActive)
			iAction = CUSTOM_FLAG_OFF;
		else
			iAction = CUSTOM_FLAG_ON;
	}

	if (pev->spawnflags & SF_CUSTOM_DEBUG)
	{
		if (iAction == CUSTOM_FLAG_ON)
			ALERT(at_console, " %s=YES", szDebug);
		else if (iAction == CUSTOM_FLAG_OFF)
			ALERT(at_console, " %s=NO", szDebug);
	}
	return iAction;
}

void CEnvCustomize::SetBoneController( float fController, int cnum, CBaseEntity *pTarget)
{
	if (fController) //FIXME: the pTarget isn't necessarily a CBaseAnimating.
	{
		if (fController == 1024)
		{
			((CBaseAnimating*)pTarget)->SetBoneController( cnum, 0 );
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " bone%d=0", cnum);
		}
		else
		{
			((CBaseAnimating*)pTarget)->SetBoneController( cnum, fController );
			if (pev->spawnflags & SF_CUSTOM_DEBUG)
				ALERT(at_console, " bone%d=%f", cnum, fController);
		}
	}
}

class CBaseTrigger : public CBaseToggle
{
	DECLARE_CLASS( CBaseTrigger, CBaseToggle );
public:
	void TeleportTouch ( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void MultiTouch( CBaseEntity *pOther );
	void HurtTouch ( CBaseEntity *pOther );
	void ActivateMultiTrigger( CBaseEntity *pActivator );
	void MultiWaitOver( void );
	void CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL CanTouch( CBaseEntity *pOther );
	void InitTrigger( void );

	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_SET_MOVEDIR; }

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( trigger, CBaseTrigger );

BEGIN_DATADESC( CBaseTrigger )
	DEFINE_FUNCTION( TeleportTouch ),
	DEFINE_FUNCTION( MultiTouch ),
	DEFINE_FUNCTION( HurtTouch ),
	DEFINE_FUNCTION( MultiWaitOver ),
	DEFINE_FUNCTION( CounterUse ),
	DEFINE_FUNCTION( ToggleUse ),
END_DATADESC()

/*
================
InitTrigger
================
*/
void CBaseTrigger::InitTrigger( )
{
	// trigger angles are used for one-way touches.  An angle of 0 is assumed
	// to mean no restrictions, so use a yaw of 360 instead.
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;

	// is mapper forget set angles?
	if( pev->movedir == g_vecZero )
	{
		pev->movedir = Vector( 1, 0, 0 );
		SetLocalAngles( g_vecZero );
	}

	SET_MODEL( edict(), GetModel());    // set size and link into world
	SetBits( m_iFlags, MF_TRIGGER );
	pev->effects = EF_NODRAW;
}

BOOL CBaseTrigger :: CanTouch( CBaseEntity *pOther )
{
	if( FStringNull( pev->netname ))
	{
		// Only touch clients, monsters, or pushables (depending on flags)
		if( pOther->pev->flags & FL_CLIENT )
			return !FBitSet( pev->spawnflags, SF_TRIGGER_NOCLIENTS );
		else if( pOther->pev->flags & FL_MONSTER )
			return FBitSet( pev->spawnflags, SF_TRIGGER_ALLOWMONSTERS );
		else if( pOther->IsPushable( ))
			return FBitSet( pev->spawnflags, SF_TRIGGER_PUSHABLES );
		else if( pOther->IsRigidBody( ))
			return FBitSet( pev->spawnflags, SF_TRIGGER_ALLOWPHYSICS );
		return FALSE;
	}
	else
	{
		// If netname is set, it's an entity-specific trigger; we ignore the spawnflags.
		if( !FClassnameIs( pOther, STRING( pev->netname )) &&
			(!pOther->pev->targetname || !FStrEq( pOther->GetTargetname(), STRING( pev->netname ))))
			return FALSE;
	}
	return TRUE;
}

//
// Cache user-entity-field values until spawn is called.
//

void CBaseTrigger :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "count"))
	{
		m_cTriggersLeft = (int) atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damagetype"))
	{
		m_bitsDamageInflict = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

class CTriggerHurt : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerHurt, CBaseTrigger );
public:
	void Spawn( void );
	void RadiationThink( void );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( trigger_hurt, CTriggerHurt );

BEGIN_DATADESC( CTriggerHurt )
	DEFINE_FUNCTION( RadiationThink ),
END_DATADESC()

//
// trigger_monsterjump
//
class CTriggerMonsterJump : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerMonsterJump, CBaseTrigger );
public:
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void Think( void );
};

LINK_ENTITY_TO_CLASS( trigger_monsterjump, CTriggerMonsterJump );

void CTriggerMonsterJump :: Spawn ( void )
{
	InitTrigger ();

	pev->nextthink = 0;
	pev->speed = 200;
	m_flHeight = 150;

	if ( !FStringNull ( pev->targetname ) )
	{
		// if targetted, spawn turned off
		pev->solid = SOLID_NOT;
		RelinkEntity( FALSE ); // Unlink from trigger list
		SetUse( &ToggleUse );
	}
}


void CTriggerMonsterJump :: Think( void )
{
	pev->solid = SOLID_NOT;// kill the trigger for now !!!UNDONE
	RelinkEntity( FALSE ); // Unlink from trigger list
	SetThink( NULL );
}

void CTriggerMonsterJump :: Touch( CBaseEntity *pOther )
{
	entvars_t *pevOther = pOther->pev;

	if ( !FBitSet ( pevOther->flags , FL_MONSTER ) ) 
	{// touched by a non-monster.
		return;
	}

	pevOther->origin.z += 1;
	
	if ( FBitSet ( pevOther->flags, FL_ONGROUND ) ) 
	{// clear the onground so physics don't bitch
		pevOther->flags &= ~FL_ONGROUND;
	}

	// toss the monster!
	Vector vecVelocity = pev->movedir * pev->speed;
	vecVelocity.z += m_flHeight;
	pOther->SetLocalVelocity( vecVelocity );
	SetNextThink( 0 );
}


//=====================================
//
// trigger_cdaudio - starts/stops cd audio tracks
//
class CTriggerCDAudio : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerCDAudio, CBaseTrigger );
public:
	void Spawn( void );

	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void PlayTrack( void );
	void Touch ( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( trigger_cdaudio, CTriggerCDAudio );

//
// Changes tracks or stops CD when player touches
//
// !!!HACK - overloaded HEALTH to avoid adding new field
void CTriggerCDAudio :: Touch ( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{// only clients may trigger these events
		return;
	}

	PlayTrack();
}

void CTriggerCDAudio :: Spawn( void )
{
	InitTrigger();
}

void CTriggerCDAudio::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	PlayTrack();
}

void PlayCDTrack( int iTrack )
{
	edict_t *pClient;
	
	// manually find the single player. 
	pClient = INDEXENT( 1 );
	
	// Can't play if the client is not connected!
	if ( !pClient )
		return;

	if ( iTrack < -1 || iTrack > 30 )
	{
		ALERT ( at_console, "TriggerCDAudio - Track %d out of range\n" );
		return;
	}

	if ( iTrack == -1 )
	{
		CLIENT_COMMAND ( pClient, "cd pause\n");
	}
	else
	{
		char string [ 64 ];

		sprintf( string, "cd play %3d\n", iTrack );
		CLIENT_COMMAND ( pClient, string);
	}
}


// only plays for ONE client, so only use in single play!
void CTriggerCDAudio :: PlayTrack( void )
{
	PlayCDTrack( (int)pev->health );
	
	SetTouch( NULL );
	UTIL_Remove( this );
}


// This plays a CD track when fired or when the player enters it's radius
class CTargetCDAudio : public CPointEntity
{
	DECLARE_CLASS( CTargetCDAudio, CPointEntity );
public:
	void		Spawn( void );
	void		KeyValue( KeyValueData *pkvd );

	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void		Think( void );
	void		Play( void );
};

LINK_ENTITY_TO_CLASS( target_cdaudio, CTargetCDAudio );

void CTargetCDAudio :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->scale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTargetCDAudio :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if ( pev->scale > 0 )
		SetNextThink( 1.0 );
}

void CTargetCDAudio::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Play();
}

// only plays for ONE client, so only use in single play!
void CTargetCDAudio::Think( void )
{
	CBaseEntity *pClient;
	
	// manually find the single player. 
	pClient = UTIL_PlayerByIndex( 1 );
	
	// Can't play if the client is not connected!
	if ( !pClient )
		return;
	
	SetNextThink( 0.5 );

	if ( (pClient->GetAbsOrigin() - GetAbsOrigin()).Length() <= pev->scale )
		Play();

}

void CTargetCDAudio::Play( void ) 
{ 
	PlayCDTrack( (int)pev->health );
	UTIL_Remove(this); 
}

//=====================================

//
// trigger_hurt - hurts anything that touches it. if the trigger has a targetname, firing it will toggle state
//
//int gfToggleState = 0; // used to determine when all radiation trigger hurts have called 'RadiationThink'

void CTriggerHurt :: Spawn( void )
{
	InitTrigger();
	SetTouch( &HurtTouch );

	if ( !FStringNull ( pev->targetname ) )
	{
		SetUse( &ToggleUse );
	}
	else
	{
		SetUse( NULL );
	}

	if (m_bitsDamageInflict & DMG_RADIATION)
	{
		SetThink( &RadiationThink );
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

//
// ToggleUse - If this is the USE function for a trigger, its state will toggle every time it's fired
//
void CBaseTrigger :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( !ShouldToggle( useType, ( pev->solid == SOLID_TRIGGER )))
		return;

	if( pev->solid == SOLID_NOT )
	{
		// if the trigger is off, turn it on
		pev->solid = SOLID_TRIGGER;
		
		// Force retouch
		gpGlobals->force_retouch++;
	}
	else
	{
		// turn the trigger off
		pev->solid = SOLID_NOT;
	}

	RelinkEntity( TRUE );
}

// When touched, a hurt trigger does DMG points of damage each half-second
void CBaseTrigger :: HurtTouch ( CBaseEntity *pOther )
{
	float fldmg;

	if ( !pOther->pev->takedamage )
		return;

	if ( (pev->spawnflags & SF_TRIGGER_HURT_CLIENTONLYTOUCH) && !pOther->IsPlayer() )
	{
		// this trigger is only allowed to touch clients, and this ain't a client.
		return;
	}

	if ( (pev->spawnflags & SF_TRIGGER_HURT_NO_CLIENTS) && pOther->IsPlayer() )
		return;

	// HACKHACK -- In multiplayer, players touch this based on packet receipt.
	// So the players who send packets later aren't always hurt.  Keep track of
	// how much time has passed and whether or not you've touched that player
	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( pev->dmgtime > gpGlobals->time )
		{
			if ( gpGlobals->time != pev->pain_finished )
			{// too early to hurt again, and not same frame with a different entity
				if ( pOther->IsPlayer() )
				{
					int playerMask = 1 << (pOther->entindex() - 1);

					// If I've already touched this player (this time), then bail out
					if ( pev->impulse & playerMask )
						return;

					// Mark this player as touched
					// BUGBUG - There can be only 32 players!
					pev->impulse |= playerMask;
				}
				else
				{
					return;
				}
			}
		}
		else
		{
			// New clock, "un-touch" all players
			pev->impulse = 0;
			if ( pOther->IsPlayer() )
			{
				int playerMask = 1 << (pOther->entindex() - 1);

				// Mark this player as touched
				// BUGBUG - There can be only 32 players!
				pev->impulse |= playerMask;
			}
		}
	}
	else	// Original code -- single player
	{
		if ( pev->dmgtime > gpGlobals->time && gpGlobals->time != pev->pain_finished )
		{// too early to hurt again, and not same frame with a different entity
			return;
		}
	}



	// If this is time_based damage (poison, radiation), override the pev->dmg with a 
	// default for the given damage type.  Monsters only take time-based damage
	// while touching the trigger.  Player continues taking damage for a while after
	// leaving the trigger

	fldmg = pev->dmg * 0.5;	// 0.5 seconds worth of damage, pev->dmg is damage/second


	// JAY: Cut this because it wasn't fully realized.  Damage is simpler now.
#if 0
	switch (m_bitsDamageInflict)
	{
	default: break;
	case DMG_POISON:		fldmg = POISON_DAMAGE/4; break;
	case DMG_NERVEGAS:		fldmg = NERVEGAS_DAMAGE/4; break;
	case DMG_RADIATION:		fldmg = RADIATION_DAMAGE/4; break;
	case DMG_PARALYZE:		fldmg = PARALYZE_DAMAGE/4; break; // UNDONE: cut this? should slow movement to 50%
	case DMG_ACID:			fldmg = ACID_DAMAGE/4; break;
	case DMG_SLOWBURN:		fldmg = SLOWBURN_DAMAGE/4; break;
	case DMG_SLOWFREEZE:	fldmg = SLOWFREEZE_DAMAGE/4; break;
	}
#endif

	if ( fldmg < 0 )
		pOther->TakeHealth( -fldmg, m_bitsDamageInflict );
	else
		pOther->TakeDamage( pev, pev, fldmg, m_bitsDamageInflict );

	// Store pain time so we can get all of the other entities on this frame
	pev->pain_finished = gpGlobals->time;

	// Apply damage every half second
	pev->dmgtime = gpGlobals->time + 0.5;// half second delay until this trigger can hurt toucher again

  
	
	if ( pev->target )
	{
		// trigger has a target it wants to fire. 
		if ( pev->spawnflags & SF_TRIGGER_HURT_CLIENTONLYFIRE )
		{
			// if the toucher isn't a client, don't fire the target!
			if ( !pOther->IsPlayer() )
			{
				return;
			}
		}

		SUB_UseTargets( pOther, USE_TOGGLE, 0 );
		if ( pev->spawnflags & SF_TRIGGER_HURT_TARGETONCE )
			pev->target = 0;
	}
}


/*QUAKED trigger_multiple (.5 .5 .5) ? notouch
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "health" is set, the trigger must be killed to activate each time.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
If notouch is set, the trigger is only fired by other entities, not by touching.
NOTOUCH has been obsoleted by trigger_relay!
sounds
1)      secret
2)      beep beep
3)      large switch
4)
NEW
if a trigger has a NETNAME, that NETNAME will become the TARGET of the triggered object.
*/
class CTriggerMultiple : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerMultiple, CBaseTrigger );
public:
	void Spawn( void );
	STATE GetState( void ) { return ( pev->nextthink > gpGlobals->time ) ? STATE_OFF : STATE_ON; }
};

LINK_ENTITY_TO_CLASS( trigger_multiple, CTriggerMultiple );


void CTriggerMultiple :: Spawn( void )
{
	if (m_flWait == 0)
		m_flWait = 0.2;

	InitTrigger();
	SetTouch( &MultiTouch );
}


/*QUAKED trigger_once (.5 .5 .5) ? notouch
Variable sized trigger. Triggers once, then removes itself.  You must set the key "target" to the name of another object in the level that has a matching
"targetname".  If "health" is set, the trigger must be killed to activate.
If notouch is set, the trigger is only fired by other entities, not by touching.
if "killtarget" is set, any objects that have a matching "target" will be removed when the trigger is fired.
if "angle" is set, the trigger will only fire when someone is facing the direction of the angle.  Use "360" for an angle of 0.
sounds
1)      secret
2)      beep beep
3)      large switch
4)
*/
class CTriggerOnce : public CTriggerMultiple
{
	DECLARE_CLASS( CTriggerOnce, CTriggerMultiple );
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( trigger_once, CTriggerOnce );
void CTriggerOnce::Spawn( void )
{
	m_flWait = -1;
	
	BaseClass::Spawn();
}



void CBaseTrigger :: MultiTouch( CBaseEntity *pOther )
{
	entvars_t	*pevToucher;

	pevToucher = pOther->pev;

	// Only touch clients, monsters, or pushables (depending on flags)
	if( CanTouch( pOther ))
	{

		// if the trigger has an angles field, check player's facing direction
		if( pev->movedir != g_vecZero && FBitSet( pev->spawnflags, SF_TRIGGER_CHECKANGLES ))
		{
			Vector vecDir = EntityToWorldTransform().VectorRotate( pev->movedir );

			UTIL_MakeVectors( pevToucher->angles );
			if ( DotProduct( gpGlobals->v_forward, vecDir ) < 0.0f )
				return; // not facing the right way
		}

		ActivateMultiTrigger( pOther );
	}
}


//
// the trigger was just touched/killed/used
// self.enemy should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
//
void CBaseTrigger :: ActivateMultiTrigger( CBaseEntity *pActivator )
{
	if (pev->nextthink > gpGlobals->time)
		return;         // still waiting for reset time

	if (IsLockedByMaster( pActivator ))
		return;

	if (FClassnameIs(pev, "trigger_secret"))
	{
		if ( pev->enemy == NULL || !FClassnameIs(pev->enemy, "player"))
			return;
		gpGlobals->found_secrets++;
	}

	if (!FStringNull(pev->noise))
		EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise), 1, ATTN_NORM);

// don't trigger again until reset
// pev->takedamage = DAMAGE_NO;

	m_hActivator = pActivator;
	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );

	if ( pev->message && pActivator->IsPlayer() )
	{
		UTIL_ShowMessage( STRING(pev->message), pActivator );
//		CLIENT_PRINTF( ENT( pActivator->pev ), print_center, STRING(pev->message) );
	}

	if (m_flWait > 0)
	{
		SetThink( &MultiWaitOver );
		SetNextThink( m_flWait );
	}
	else
	{
		// we can't just remove (self) here, because this is a touch function
		// called while C code is looping through area links...
		SetTouch( NULL );
		SetNextThink( 0.1 );
		SetThink( &SUB_Remove );
	}
}


// the wait time has passed, so set back up for another activation
void CBaseTrigger :: MultiWaitOver( void )
{
	SetThink( NULL );
}

//===========================================================
//LRC - trigger_inout, a trigger which fires _only_ when
// the player enters or leaves it.
//   If there's more than one entity it can trigger off, then
// it will trigger for each one that enters and leaves.
//===========================================================
class CTriggerInOut;

class CInOutRegister : public CPointEntity
{
	DECLARE_CLASS( CInOutRegister, CPointEntity );
public:
	// returns true if found in the list
	BOOL IsRegistered ( CBaseEntity *pValue );
	// remove all invalid entries from the list, trigger their targets as appropriate
	// returns the new list
	CInOutRegister *Prune( void );
	// adds a new entry to the list
	CInOutRegister *Add( CBaseEntity *pValue );
	BOOL IsEmpty( void ) { return m_pNext ? FALSE : TRUE; };

	DECLARE_DATADESC();

	CTriggerInOut *m_pField;
	CInOutRegister *m_pNext;
	EHANDLE m_hValue;
};

class CTriggerInOut : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerInOut, CBaseTrigger );
public:
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void Think( void );
	int Restore( CRestore &restore );
	virtual void FireOnEntry( CBaseEntity *pOther );
	virtual void FireOnLeaving( CBaseEntity *pOther );
	virtual void OnRemove( void );
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();

	STATE GetState() { return m_pRegister->IsEmpty() ? STATE_OFF : STATE_ON; }

	string_t m_iszAltTarget;
	string_t m_iszBothTarget;
	CInOutRegister *m_pRegister;
};

// CInOutRegister method bodies:
BEGIN_DATADESC( CInOutRegister )
	DEFINE_FIELD( m_pField, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pNext, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_hValue, FIELD_EHANDLE ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( inout_register, CInOutRegister );

BOOL CInOutRegister :: IsRegistered ( CBaseEntity *pValue )
{
	if (m_hValue == pValue)
		return TRUE;
	else if (m_pNext)
		return m_pNext->IsRegistered( pValue );
	else
		return FALSE;
}

CInOutRegister *CInOutRegister::Add( CBaseEntity *pValue )
{
	if( m_hValue == pValue )
	{
		// it's already in the list, don't need to do anything
		return this;
	}
	else if( m_pNext )
	{
		// keep looking
		m_pNext = m_pNext->Add( pValue );
		return this;
	}
	else
	{
		// reached the end of the list; add the new entry, and trigger
		CInOutRegister *pResult = GetClassPtr( (CInOutRegister*)NULL );
		pResult->m_hValue = pValue;
		pResult->m_pNext = this;
		pResult->m_pField = m_pField;
		pResult->pev->classname = MAKE_STRING("inout_register");

		m_pField->FireOnEntry( pValue );
		return pResult;
	}
}

CInOutRegister *CInOutRegister::Prune( void )
{
	if ( m_hValue )
	{
		ASSERTSZ(m_pNext != NULL, "invalid InOut registry terminator\n");
		if ( m_pField->TriggerIntersects( m_hValue ) && !FBitSet( m_pField->pev->flags, FL_KILLME ))
		{
			// this entity is still inside the field, do nothing
			m_pNext = m_pNext->Prune();
			return this;
		}
		else
		{
			// this entity has just left the field, trigger
			m_pField->FireOnLeaving( m_hValue );
			SetThink( &CInOutRegister:: SUB_Remove );
			SetNextThink( 0.1 );
			return m_pNext->Prune();
		}
	}
	else
	{	// this register has a missing or null value
		if( m_pNext )
		{
			// this is an invalid list entry, remove it
			SetThink( &CInOutRegister:: SUB_Remove );
			SetNextThink( 0.1 );
			return m_pNext->Prune();
		}
		else
		{
			// this is the list terminator, leave it.
			return this;
		}
	}
}

// CTriggerInOut method bodies:
LINK_ENTITY_TO_CLASS( trigger_inout, CTriggerInOut );

BEGIN_DATADESC( CTriggerInOut )
	DEFINE_KEYFIELD( m_iszAltTarget, FIELD_STRING, "m_iszAltTarget" ),
	DEFINE_KEYFIELD( m_iszBothTarget, FIELD_STRING, "m_iszBothTarget" ),
	DEFINE_FIELD( m_pRegister, FIELD_CLASSPTR ),
END_DATADESC()

void CTriggerInOut::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iszAltTarget" ))
	{
		m_iszAltTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszBothTarget" ))
	{
		m_iszBothTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

int CTriggerInOut :: Restore( CRestore &restore )
{
	CInOutRegister *pRegister;

	if( restore.IsGlobalMode() )
	{
		// we already have the valid chain.
		// Don't break it with bad pointers from previous level
		pRegister = m_pRegister;
	}

	int status = BaseClass::Restore(restore);
	if ( !status )
		return 0;

	if( restore.IsGlobalMode() )
	{
		// restore our chian here
		m_pRegister = pRegister;
	}

	return status;
}

void CTriggerInOut :: Spawn( void )
{
	InitTrigger();
	// create a null-terminator for the registry
	m_pRegister = GetClassPtr( (CInOutRegister*)NULL );
	m_pRegister->m_hValue = NULL;
	m_pRegister->m_pNext = NULL;
	m_pRegister->m_pField = this;
	m_pRegister->pev->classname = MAKE_STRING("inout_register");
}

void CTriggerInOut :: Touch( CBaseEntity *pOther )
{
	if( !CanTouch( pOther ))
		return;

	m_pRegister = m_pRegister->Add( pOther );

	if( pev->nextthink <= 0.0f && !m_pRegister->IsEmpty( ))
		SetNextThink( 0.05 );
}

void CTriggerInOut :: Think( void )
{
	// Prune handles all Intersects tests and fires targets as appropriate
	m_pRegister = m_pRegister->Prune();

	if (m_pRegister->IsEmpty())
		DontThink();
	else
		SetNextThink( 0.05 );
}

void CTriggerInOut :: FireOnEntry( CBaseEntity *pOther )
{
	if( !IsLockedByMaster( pOther ))
	{
//Msg( "FireOnEntry( %s )\n", STRING( pev->target ));
		UTIL_FireTargets( m_iszBothTarget, pOther, this, USE_ON, 0 );
		UTIL_FireTargets( pev->target, pOther, this, USE_TOGGLE, 0 );
	}
}

void CTriggerInOut :: FireOnLeaving( CBaseEntity *pOther )
{
	if( !IsLockedByMaster( pOther ))
	{
//Msg( "FireOnLeaving( %s )\n", STRING( m_iszAltTarget ));
		UTIL_FireTargets( m_iszBothTarget, pOther, this, USE_OFF, 0 );
		UTIL_FireTargets( m_iszAltTarget, pOther, this, USE_TOGGLE, 0 );
	}
}

void CTriggerInOut :: OnRemove( void )
{
	if( !m_pRegister ) return; // e.g. moved from another level

	// Prune handles all Intersects tests and fires targets as appropriate
	m_pRegister = m_pRegister->Prune();
}

class CTriggerGravityField : public CTriggerInOut
{
	DECLARE_CLASS( CTriggerGravityField, CTriggerInOut );
public:
	virtual void FireOnEntry( CBaseEntity *pOther );
	virtual void FireOnLeaving( CBaseEntity *pOther );
};

void CTriggerGravityField :: FireOnEntry( CBaseEntity *pOther )
{
	// Only save on clients
	if( !pOther->IsPlayer( )) return;
	pOther->pev->gravity = pev->gravity;
}

void CTriggerGravityField :: FireOnLeaving( CBaseEntity *pOther )
{
	if( !pOther->IsPlayer( )) return;
	pOther->pev->gravity = 1.0f;
}

LINK_ENTITY_TO_CLASS( trigger_gravity_field, CTriggerGravityField );

class CTriggerDSPZone : public CTriggerInOut
{
	DECLARE_CLASS( CTriggerDSPZone, CTriggerInOut );
public:
	void KeyValue( KeyValueData* pkvd );
	virtual void FireOnEntry( CBaseEntity *pOther );
	virtual void FireOnLeaving( CBaseEntity *pOther );
};

void CTriggerDSPZone :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "roomtype"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}

void CTriggerDSPZone :: FireOnEntry( CBaseEntity *pOther )
{
	// Only save on clients
	if( !pOther->IsPlayer( )) return;
	pev->button = ((CBasePlayer *)pOther)->m_iSndRoomtype;
	((CBasePlayer *)pOther)->m_iSndRoomtype = pev->impulse;
}

void CTriggerDSPZone :: FireOnLeaving( CBaseEntity *pOther )
{
	if( !pOther->IsPlayer( )) return;
	((CBasePlayer *)pOther)->m_iSndRoomtype = pev->button;
}

LINK_ENTITY_TO_CLASS( trigger_dsp_zone, CTriggerDSPZone );

// ========================= COUNTING TRIGGER =====================================

//
// GLOBALS ASSUMED SET:  g_eoActivator
//
void CBaseTrigger::CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_cTriggersLeft--;
	m_hActivator = pActivator;

	if (m_cTriggersLeft < 0)
		return;
	
	BOOL fTellActivator =
		(m_hActivator != 0) &&
		FClassnameIs(m_hActivator->pev, "player") &&
		!FBitSet(pev->spawnflags, SPAWNFLAG_NOMESSAGE);
	if (m_cTriggersLeft != 0)
	{
		if (fTellActivator)
		{
			// UNDONE: I don't think we want these Quakesque messages
			switch (m_cTriggersLeft)
			{
			case 1:		ALERT(at_console, "Only 1 more to go...");		break;
			case 2:		ALERT(at_console, "Only 2 more to go...");		break;
			case 3:		ALERT(at_console, "Only 3 more to go...");		break;
			default:	ALERT(at_console, "There are more to go...");	break;
			}
		}
		return;
	}

	// !!!UNDONE: I don't think we want these Quakesque messages
	if (fTellActivator)
		ALERT(at_console, "Sequence completed!");
	
	ActivateMultiTrigger( m_hActivator );
}


/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.
If nomessage is not set, it will print "1 more.. " etc when triggered and
"sequence complete" when finished.  After the counter has been triggered "cTriggersLeft"
times (default 2), it will fire all of it's targets and remove itself.
*/
class CTriggerCounter : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerCounter, CBaseTrigger );
public:
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( trigger_counter, CTriggerCounter );

void CTriggerCounter :: Spawn( void )
{
	// By making the flWait be -1, this counter-trigger will disappear after it's activated
	// (but of course it needs cTriggersLeft "uses" before that happens).
	m_flWait = -1;

	if (m_cTriggersLeft == 0)
		m_cTriggersLeft = 2;
	SetUse( &CounterUse );
}

// ====================== TRIGGER_CHANGELEVEL ================================

class CTriggerVolume : public CPointEntity	// Derive from point entity so this doesn't move across levels
{
	DECLARE_CLASS( CTriggerVolume, CPointEntity );
public:
	void	Spawn( void );
};

LINK_ENTITY_TO_CLASS( trigger_transition, CTriggerVolume );

// Define space that travels across a level transition
void CTriggerVolume :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	pev->model = NULL;
	pev->modelindex = 0;
}


// Fires a target after level transition and then dies
class CFireAndDie : public CBaseDelay
{
	DECLARE_CLASS( CFireAndDie, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void Think( void );
	int ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_FORCE_TRANSITION; }	// Always go across transitions
};
LINK_ENTITY_TO_CLASS( fireanddie, CFireAndDie );

void CFireAndDie::Spawn( void )
{
	pev->classname = MAKE_STRING("fireanddie");
	// Don't call Precache() - it should be called on restore
}

void CFireAndDie::Precache( void )
{
	// This gets called on restore
	pev->nextthink = gpGlobals->time + m_flDelay;
}

void CFireAndDie::Think( void )
{
	SUB_UseTargets( this, USE_TOGGLE, 0 );
	UTIL_Remove( this );
}

#define SF_CHANGELEVEL_USEONLY		0x0002

class CChangeLevel : public CBaseTrigger
{
	DECLARE_CLASS( CChangeLevel, CBaseTrigger );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void UseChangeLevel ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void ExecuteChangeLevel( void );
	void TouchChangeLevel( CBaseEntity *pOther );
	void ChangeLevelNow( CBaseEntity *pActivator );

	static edict_t *FindLandmark( const char *pLandmarkName );
	static int ChangeList( LEVELLIST *pLevelList, int maxList );
	static int AddTransitionToList( LEVELLIST *pLevelList, int listCount, const char *pMapName, const char *pLandmarkName, edict_t *pentLandmark );
	static int InTransitionVolume( CBaseEntity *pEntity, char *pVolumeName );

	DECLARE_DATADESC();

	char	m_szMapName[cchMapNameMost];		// trigger_changelevel only:  next map
	char	m_szLandmarkName[cchMapNameMost];	// trigger_changelevel only:  landmark on next map
	string_t	m_changeTarget;
	float	m_changeTargetDelay;
};
LINK_ENTITY_TO_CLASS( trigger_changelevel, CChangeLevel );

// Global Savedata for changelevel trigger
BEGIN_DATADESC( CChangeLevel )
	DEFINE_AUTO_ARRAY( m_szMapName, FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY( m_szLandmarkName, FIELD_CHARACTER ),
	DEFINE_KEYFIELD( m_changeTarget, FIELD_STRING, "changetarget" ),
	DEFINE_KEYFIELD( m_changeTargetDelay, FIELD_FLOAT, "changedelay" ),
	DEFINE_FUNCTION( UseChangeLevel ),
	DEFINE_FUNCTION( ExecuteChangeLevel ),
	DEFINE_FUNCTION( TouchChangeLevel ),
END_DATADESC()

//
// Cache user-entity-field values until spawn is called.
//

void CChangeLevel :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "map"))
	{
		if (strlen(pkvd->szValue) >= cchMapNameMost)
			ALERT( at_error, "Map name '%s' too long (32 chars)\n", pkvd->szValue );
		strcpy(m_szMapName, pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "landmark"))
	{
		if (strlen(pkvd->szValue) >= cchMapNameMost)
			ALERT( at_error, "Landmark name '%s' too long (32 chars)\n", pkvd->szValue );
		strcpy(m_szLandmarkName, pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "changetarget"))
	{
		m_changeTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "changedelay"))
	{
		m_changeTargetDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


/*QUAKED trigger_changelevel (0.5 0.5 0.5) ? NO_INTERMISSION
When the player touches this, he gets sent to the map listed in the "map" variable.  Unless the NO_INTERMISSION flag is set, the view will go to the info_intermission spot and display stats.
*/

void CChangeLevel :: Spawn( void )
{
	if ( FStrEq( m_szMapName, "" ) )
		ALERT( at_console, "a trigger_changelevel doesn't have a map" );

	if ( FStrEq( m_szLandmarkName, "" ) )
		ALERT( at_console, "trigger_changelevel to %s doesn't have a landmark\n", m_szMapName );

	if (!FStringNull ( pev->targetname ) )
	{
		SetUse( &UseChangeLevel );
	}
	InitTrigger();
	if ( !(pev->spawnflags & SF_CHANGELEVEL_USEONLY) )
		SetTouch( &TouchChangeLevel );
//	ALERT( at_console, "TRANSITION: %s (%s)\n", m_szMapName, m_szLandmarkName );
}

void CChangeLevel :: ExecuteChangeLevel( void )
{
	MESSAGE_BEGIN( MSG_ALL, SVC_CDTRACK );
		WRITE_BYTE( 3 );
		WRITE_BYTE( 3 );
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();
}

FILE_GLOBAL char st_szNextMap[cchMapNameMost];
FILE_GLOBAL char st_szNextSpot[cchMapNameMost];

edict_t *CChangeLevel :: FindLandmark( const char *pLandmarkName )
{
	edict_t	*pentLandmark;

	pentLandmark = FIND_ENTITY_BY_STRING( NULL, "targetname", pLandmarkName );
	while ( !FNullEnt( pentLandmark ) )
	{
		// Found the landmark
		if ( FClassnameIs( pentLandmark, "info_landmark" ) )
			return pentLandmark;
		else
			pentLandmark = FIND_ENTITY_BY_STRING( pentLandmark, "targetname", pLandmarkName );
	}
	ALERT( at_error, "Can't find landmark %s\n", pLandmarkName );
	return NULL;
}


//=========================================================
// CChangeLevel :: Use - allows level transitions to be 
// triggered by buttons, etc.
//
//=========================================================
void CChangeLevel :: UseChangeLevel ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	ChangeLevelNow( pActivator );
}

void CChangeLevel :: ChangeLevelNow( CBaseEntity *pActivator )
{
	edict_t	*pentLandmark;
	LEVELLIST	levels[16];

	ASSERT(!FStrEq(m_szMapName, ""));

	// Don't work in deathmatch
	if ( g_pGameRules->IsDeathmatch() )
		return;

	// Some people are firing these multiple times in a frame, disable
	if ( gpGlobals->time == pev->dmgtime )
		return;

	pev->dmgtime = gpGlobals->time;


	CBaseEntity *pPlayer = CBaseEntity::Instance( INDEXENT( 1 ) );
	if( !pPlayer ) return;

	if ( !InTransitionVolume( pPlayer, m_szLandmarkName ) )
	{
		ALERT( at_aiconsole, "Player isn't in the transition volume %s, aborting\n", m_szLandmarkName );
		return;
	}

	// Create an entity to fire the changetarget
	if ( m_changeTarget )
	{
		CFireAndDie *pFireAndDie = GetClassPtr( (CFireAndDie *)NULL );
		if ( pFireAndDie )
		{
			// Set target and delay
			pFireAndDie->pev->target = m_changeTarget;
			pFireAndDie->m_flDelay = m_changeTargetDelay;
			pFireAndDie->SetAbsOrigin( pPlayer->GetAbsOrigin( ));
			// Call spawn
			DispatchSpawn( pFireAndDie->edict() );
		}
	}
	// This object will get removed in the call to CHANGE_LEVEL, copy the params into "safe" memory
	strcpy(st_szNextMap, m_szMapName);

	m_hActivator = pActivator;
	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
	st_szNextSpot[0] = 0;	// Init landmark to NULL

	// look for a landmark entity		
	pentLandmark = FindLandmark( m_szLandmarkName );
	if ( !FNullEnt( pentLandmark ) )
	{
		strcpy(st_szNextSpot, m_szLandmarkName);
		gpGlobals->vecLandmarkOffset = VARS(pentLandmark)->origin;
	}
//	ALERT( at_console, "Level touches %d levels\n", ChangeList( levels, 16 ) );
	ALERT( at_console, "CHANGE LEVEL: %s %s\n", st_szNextMap, st_szNextSpot );
	CHANGE_LEVEL( st_szNextMap, st_szNextSpot );
}

//
// GLOBALS ASSUMED SET:  st_szNextMap
//
void CChangeLevel :: TouchChangeLevel( CBaseEntity *pOther )
{
	if (!FClassnameIs(pOther->pev, "player"))
		return;

	ChangeLevelNow( pOther );
}


// Add a transition to the list, but ignore duplicates 
// (a designer may have placed multiple trigger_changelevels with the same landmark)
int CChangeLevel::AddTransitionToList( LEVELLIST *pLevelList, int listCount, const char *pMapName, const char *pLandmarkName, edict_t *pentLandmark )
{
	int i;

	if ( !pLevelList || !pMapName || !pLandmarkName || !pentLandmark )
		return 0;

	for ( i = 0; i < listCount; i++ )
	{
		if ( pLevelList[i].pentLandmark == pentLandmark && strcmp( pLevelList[i].mapName, pMapName ) == 0 )
			return 0;
	}
	strcpy( pLevelList[listCount].mapName, pMapName );
	strcpy( pLevelList[listCount].landmarkName, pLandmarkName );
	pLevelList[listCount].pentLandmark = pentLandmark;
	pLevelList[listCount].vecLandmarkOrigin = VARS(pentLandmark)->origin;

	return 1;
}

int BuildChangeList( LEVELLIST *pLevelList, int maxList )
{
	return CChangeLevel::ChangeList( pLevelList, maxList );
}


int CChangeLevel::InTransitionVolume( CBaseEntity *pEntity, char *pVolumeName )
{
	edict_t	*pentVolume;


	if ( pEntity->ObjectCaps() & FCAP_FORCE_TRANSITION )
		return 1;

	// If you're following another entity, follow it through the transition (weapons follow the player)
	if ( pEntity->pev->movetype == MOVETYPE_FOLLOW )
	{
		if ( pEntity->pev->aiment != NULL )
			pEntity = CBaseEntity::Instance( pEntity->pev->aiment );
	}

	int inVolume = 1;	// Unless we find a trigger_transition, everything is in the volume

	pentVolume = FIND_ENTITY_BY_TARGETNAME( NULL, pVolumeName );
	while ( !FNullEnt( pentVolume ) )
	{
		CBaseEntity *pVolume = CBaseEntity::Instance( pentVolume );
		
		if ( pVolume && FClassnameIs( pVolume->pev, "trigger_transition" ) )
		{
			if ( pVolume->Intersects( pEntity ) )	// It touches one, it's in the volume
				return 1;
			else
				inVolume = 0;	// Found a trigger_transition, but I don't intersect it -- if I don't find another, don't go!
		}
		pentVolume = FIND_ENTITY_BY_TARGETNAME( pentVolume, pVolumeName );
	}

	return inVolume;
}


// We can only ever move 512 entities across a transition
#define MAX_ENTITY 512

// This has grown into a complicated beast
// Can we make this more elegant?
// This builds the list of all transitions on this level and which entities are in their PVS's and can / should
// be moved across.
int CChangeLevel::ChangeList( LEVELLIST *pLevelList, int maxList )
{
	edict_t	*pentChangelevel, *pentLandmark;
	int			i, count;

	count = 0;

	// Find all of the possible level changes on this BSP
	pentChangelevel = FIND_ENTITY_BY_STRING( NULL, "classname", "trigger_changelevel" );
	if ( FNullEnt( pentChangelevel ) )
		return 0;
	while ( !FNullEnt( pentChangelevel ) )
	{
		CChangeLevel *pTrigger;
		
		pTrigger = GetClassPtr((CChangeLevel *)VARS(pentChangelevel));
		if ( pTrigger )
		{
			// Find the corresponding landmark
			pentLandmark = FindLandmark( pTrigger->m_szLandmarkName );
			if ( pentLandmark )
			{
				// Build a list of unique transitions
				if ( AddTransitionToList( pLevelList, count, pTrigger->m_szMapName, pTrigger->m_szLandmarkName, pentLandmark ) )
				{
					count++;
					if ( count >= maxList )		// FULL!!
						break;
				}
			}
		}
		pentChangelevel = FIND_ENTITY_BY_STRING( pentChangelevel, "classname", "trigger_changelevel" );
	}

	if ( gpGlobals->pSaveData && ((SAVERESTOREDATA *)gpGlobals->pSaveData)->pTable )
	{
		CSave saveHelper( (SAVERESTOREDATA *)gpGlobals->pSaveData );

		for ( i = 0; i < count; i++ )
		{
			int j, entityCount = 0;
			CBaseEntity *pEntList[ MAX_ENTITY ];
			int			 entityFlags[ MAX_ENTITY ];

			// Follow the linked list of entities in the PVS of the transition landmark
			edict_t *pent = UTIL_EntitiesInPVS( pLevelList[i].pentLandmark );

			// Build a list of valid entities in this linked list (we're going to use pent->v.chain again)
			while ( !FNullEnt( pent ) )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance(pent);
				if ( pEntity )
				{
//					ALERT( at_console, "Trying %s\n", STRING(pEntity->pev->classname) );
					int caps = pEntity->ObjectCaps();
					if ( !(caps & FCAP_DONT_SAVE) )
					{
						int flags = 0;

						// If this entity can be moved or is global, mark it
						if ( caps & FCAP_ACROSS_TRANSITION )
							flags |= FENTTABLE_MOVEABLE;
						if ( pEntity->pev->globalname && !pEntity->IsDormant() )
							flags |= FENTTABLE_GLOBAL;
						if ( flags )
						{
//							ALERT( at_console, "Passed %s %s\n", pEntity->GetClassname(), pEntity->GetGlobalname() );
							pEntList[ entityCount ] = pEntity;
							entityFlags[ entityCount ] = flags;
							entityCount++;
							if ( entityCount > MAX_ENTITY )
								ALERT( at_error, "Too many entities across a transition!" );
						}
//						else
//							ALERT( at_console, "Failed %s %s\n", pEntity->GetClassname(), pEntity->GetGlobalname() );
					}
//					else
//						ALERT( at_console, "DON'T SAVE %s\n", pEntity->GetClassname() );
				}
				pent = pent->v.chain;
			}

			for ( j = 0; j < entityCount; j++ )
			{
				// Check to make sure the entity isn't screened out by a trigger_transition
				if ( entityFlags[j] && InTransitionVolume( pEntList[j], pLevelList[i].landmarkName ) )
				{
					// do something before changelevel
					if( entityFlags[j] & FENTTABLE_MOVEABLE && gpGlobals->changelevel )
					{
						pEntList[j]->OnChangeLevel();
					}

					// Mark entity table with 1<<i
					int index = saveHelper.EntityIndex( pEntList[j] );
					// Flag it with the level number
					saveHelper.EntityFlagsSet( index, entityFlags[j] | (1<<i) );
				}
//				else
//					ALERT( at_console, "Screened out %s\n", STRING(pEntList[j]->pev->classname) );

			}
		}
	}

	return count;
}

/*
go to the next level for deathmatch
only called if a time or frag limit has expired
*/
void NextLevel( void )
{
	edict_t* pent;
	CChangeLevel *pChange;
	
	// find a trigger_changelevel
	pent = FIND_ENTITY_BY_CLASSNAME(NULL, "trigger_changelevel");
	
	// go back to start if no trigger_changelevel
	if (FNullEnt(pent))
	{
		gpGlobals->mapname = ALLOC_STRING("start");
		pChange = GetClassPtr( (CChangeLevel *)NULL );
		strcpy(pChange->m_szMapName, "start");
	}
	else
		pChange = GetClassPtr( (CChangeLevel *)VARS(pent));
	
	strcpy(st_szNextMap, pChange->m_szMapName);
	g_fGameOver = TRUE;
	
	if (pChange->pev->nextthink < gpGlobals->time)
	{
		pChange->SetThink( &CChangeLevel::ExecuteChangeLevel );
		pChange->pev->nextthink = gpGlobals->time + 0.1;
	}
}


// ============================== LADDER =======================================

class CLadder : public CBaseTrigger
{
	DECLARE_CLASS( CLadder, CBaseTrigger );
public:
	void Spawn( void );
	void Precache( void );
};

LINK_ENTITY_TO_CLASS( func_ladder, CLadder );

//=========================================================
// func_ladder - makes an area vertically negotiable
//=========================================================
void CLadder :: Precache( void )
{
	// Do all of this in here because we need to 'convert' old saved games
	pev->solid = SOLID_NOT;
	pev->skin = CONTENTS_LADDER;

	if( !CVAR_GET_FLOAT( "showtriggers" ))
	{
		pev->rendermode = kRenderTransTexture;
		pev->renderamt = 0;
	}

	pev->effects &= ~EF_NODRAW;
}


void CLadder :: Spawn( void )
{
	Precache();

	SET_MODEL( edict(), GetModel( )); // set size and link into world
	pev->movetype = MOVETYPE_PUSH;
	SetBits( m_iFlags, MF_LADDER );
}

// ========================== A TRIGGER THAT PUSHES YOU ===============================

#define SF_TRIG_PUSH_ONCE	BIT( 0 )
#define SF_TRIG_PUSH_SETUP	BIT( 31 )

class CTriggerPush : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerPush, CBaseTrigger );
public:
	void Spawn( void );
	void Activate( void );
	void Touch( CBaseEntity *pOther );
	Vector m_vecPushDir;
};
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

	SetUse( &ToggleUse );
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

//===========================================================
//LRC- trigger_bounce
//===========================================================
#define SF_BOUNCE_CUTOFF 	BIT( 4 )

class CTriggerBounce : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerBounce, CBaseTrigger );
public:
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( trigger_bounce, CTriggerBounce );


void CTriggerBounce :: Spawn( void )
{
	InitTrigger();
}

void CTriggerBounce :: Touch( CBaseEntity *pOther )
{
	if (!UTIL_IsMasterTriggered(m_sMaster, pOther))
		return;
	if (!CanTouch( pOther ))
		return;

	float dot = DotProduct(pev->movedir, pOther->pev->velocity);
	if (dot < -pev->armorvalue)
	{
		if (pev->spawnflags & SF_BOUNCE_CUTOFF)
			pOther->pev->velocity = pOther->pev->velocity - (dot + pev->frags*(dot+pev->armorvalue))*pev->movedir;
		else
			pOther->pev->velocity = pOther->pev->velocity - (dot + pev->frags*dot)*pev->movedir;
		SUB_UseTargets( pOther, USE_TOGGLE, 0 );
	}
}

//===========================================================
//LRC- trigger_onsight
//===========================================================
#define SF_ONSIGHT_NOLOS	0x00001
#define SF_ONSIGHT_NOGLASS	0x00002
#define SF_ONSIGHT_ACTIVE	0x08000
#define SF_ONSIGHT_DEMAND	0x10000

class CTriggerOnSight : public CBaseDelay
{
	DECLARE_CLASS( CTriggerOnSight, CBaseDelay );
public:
	void Spawn( void );
	void Think( void );
	BOOL VisionCheck( void );
	BOOL CanSee( CBaseEntity *pLooker, CBaseEntity *pSeen );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	STATE GetState();
};

LINK_ENTITY_TO_CLASS( trigger_onsight, CTriggerOnSight );

void CTriggerOnSight :: Spawn( void )
{
	if (pev->target || pev->noise)
		// if we're going to have to trigger stuff, start thinking
		SetNextThink( 1 );
	else
		// otherwise, just check whenever someone asks about our state.
		pev->spawnflags |= SF_ONSIGHT_DEMAND;

	if (pev->max_health > 0)
	{
		pev->health = cos(pev->max_health/2 * M_PI/180.0);
	}
}

STATE CTriggerOnSight :: GetState( void )
{
	if (pev->spawnflags & SF_ONSIGHT_DEMAND)
		return VisionCheck()?STATE_ON:STATE_OFF;
	else
		return (pev->spawnflags & SF_ONSIGHT_ACTIVE) ? STATE_ON : STATE_OFF;
}

void CTriggerOnSight :: Think( void )
{
	// is this a sensible rate?
	SetNextThink( 0.1 );

	if (VisionCheck())
	{
		if (!FBitSet(pev->spawnflags, SF_ONSIGHT_ACTIVE))
		{
			UTIL_FireTargets(STRING(pev->target), this, this, USE_TOGGLE, 0);
			UTIL_FireTargets(STRING(pev->noise1), this, this, USE_ON, 0);
			pev->spawnflags |= SF_ONSIGHT_ACTIVE;
		}
	}
	else
	{
		if (pev->spawnflags & SF_ONSIGHT_ACTIVE)
		{
			UTIL_FireTargets(STRING(pev->noise), this, this, USE_TOGGLE, 0);
			UTIL_FireTargets(STRING(pev->noise1), this, this, USE_OFF, 0);
			pev->spawnflags &= ~SF_ONSIGHT_ACTIVE;
		}
	}
}

BOOL CTriggerOnSight :: VisionCheck( void )
{
	// and GetState check (stops dead monsters seeing)
	CBaseEntity *pLooker;
	if (pev->netname)
	{
		pLooker = UTIL_FindEntityByTargetname(NULL, STRING(pev->netname));
		if (!pLooker)
			return FALSE; // if we can't find the eye entity, give up
	}
	else
	{
		pLooker = UTIL_FindEntityByClassname(NULL, "player");
		if (!pLooker)
		{
			ALERT(at_error, "trigger_onsight can't find player!?\n");
			return FALSE;
		}
	}

	CBaseEntity *pSeen;
	if (pev->message)
		pSeen = UTIL_FindEntityByTargetname(NULL, STRING(pev->message));
	else
		return CanSee(pLooker, this);

	if (!pSeen)
	{
		// must be a classname.
		pSeen = UTIL_FindEntityByClassname(pSeen, STRING(pev->message));
		while (pSeen != NULL)
		{
			if (CanSee(pLooker, pSeen))
				return TRUE;
			pSeen = UTIL_FindEntityByClassname(pSeen, STRING(pev->message));
		}
		return FALSE;
	}
	else
	{
		while (pSeen != NULL)
		{
			if (CanSee(pLooker, pSeen))
				return TRUE;
			pSeen = UTIL_FindEntityByTargetname(pSeen, STRING(pev->message));
		}
		return FALSE;
	}
}

// by the criteria we're using, can the Looker see the Seen entity?
BOOL CTriggerOnSight :: CanSee(CBaseEntity *pLooker, CBaseEntity *pSeen)
{
	// out of range?
	if (pev->frags && (pLooker->pev->origin - pSeen->pev->origin).Length() > pev->frags)
		return FALSE;

	// check FOV if appropriate
	if (pev->max_health < 360)
	{
		// copied from CBaseMonster's FInViewCone function
		Vector2D	vec2LOS;
		float	flDot;
		float flComp = pev->health;
		UTIL_MakeVectors ( pLooker->pev->angles );
		vec2LOS = ( pSeen->pev->origin - pLooker->pev->origin ).Make2D();
		vec2LOS = vec2LOS.Normalize();
		flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

		if ( pev->max_health == -1 )
		{
			CBaseMonster *pMonst = pLooker->MyMonsterPointer();
			if (pMonst)
				flComp = pMonst->m_flFieldOfView;
			else
				return FALSE; // not a monster, can't use M-M-M-MonsterVision
		}

		// outside field of view
		if (flDot <= flComp)
			return FALSE;
	}

	// check LOS if appropriate
	if (!FBitSet(pev->spawnflags, SF_ONSIGHT_NOLOS))
	{
		TraceResult tr;
		if (SF_ONSIGHT_NOGLASS)
			UTIL_TraceLine( pLooker->EyePosition(), pSeen->pev->origin, ignore_monsters, ignore_glass, pLooker->edict(), &tr );
		else
			UTIL_TraceLine( pLooker->EyePosition(), pSeen->pev->origin, ignore_monsters, dont_ignore_glass, pLooker->edict(), &tr );
		if (tr.flFraction < 1.0 && tr.pHit != pSeen->edict())
			return FALSE;
	}

	return TRUE;
}

//======================================
// teleport trigger
//
//

void CBaseTrigger :: TeleportTouch( CBaseEntity *pOther )
{
	CBaseEntity *pTarget = NULL;

	if( !CanTouch( pOther )) return;

	if( IsLockedByMaster( pOther ))
		return;

	pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ));
	if( !pTarget ) return;

	Vector tmp = pTarget->GetAbsOrigin();
	Vector pAngles = pTarget->GetAbsAngles();

	if( pOther->IsPlayer( ))
		tmp.z -= pOther->pev->mins.z; // make origin adjustments in case the teleportee is a player. (origin in center, not at feet)
	tmp.z++;

	pOther->Teleport( &tmp, &pAngles, &g_vecZero );
	pOther->pev->flags &= ~FL_ONGROUND;

	UTIL_FireTargets( pev->message, pOther, this, USE_TOGGLE ); // fire target on pass
}

class CTriggerTeleport : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerTeleport, CBaseTrigger );
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( trigger_teleport, CTriggerTeleport );

void CTriggerTeleport :: Spawn( void )
{
	InitTrigger();
	SetTouch( &TeleportTouch );
}

LINK_ENTITY_TO_CLASS( info_teleport_destination, CPointEntity );

class CTriggerSave : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerSave, CBaseTrigger );
public:
	void Spawn( void );
	void SaveTouch( CBaseEntity *pOther );

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS( trigger_autosave, CTriggerSave );

BEGIN_DATADESC( CTriggerSave )
	DEFINE_FUNCTION( SaveTouch ),
END_DATADESC()

void CTriggerSave::Spawn( void )
{
	if ( g_pGameRules->IsDeathmatch() )
	{
		REMOVE_ENTITY( ENT(pev) );
		return;
	}

	InitTrigger();
	SetTouch( &SaveTouch );
}

void CTriggerSave::SaveTouch( CBaseEntity *pOther )
{
	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	// Only save on clients
	if ( !pOther->IsPlayer() )
		return;
    
	SetTouch( NULL );
	UTIL_Remove( this );
	SERVER_COMMAND( "autosave\n" );
}

#define SF_ENDSECTION_USEONLY		0x0001

class CTriggerEndSection : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerEndSection, CBaseTrigger );
public:
	void Spawn( void );
	void EndSectionTouch( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void EndSectionUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS( trigger_endsection, CTriggerEndSection );

BEGIN_DATADESC( CTriggerEndSection )
	DEFINE_FUNCTION( EndSectionTouch ),
	DEFINE_FUNCTION( EndSectionUse ),
END_DATADESC()

void CTriggerEndSection::EndSectionUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Only save on clients
	if ( pActivator && !pActivator->IsNetClient() )
		return;
    
	SetUse( NULL );

	if ( pev->message )
	{
		g_engfuncs.pfnEndSection(STRING(pev->message));
	}
	UTIL_Remove( this );
}

void CTriggerEndSection::Spawn( void )
{
	if ( g_pGameRules->IsDeathmatch() )
	{
		REMOVE_ENTITY( ENT(pev) );
		return;
	}

	InitTrigger();

	SetUse( &EndSectionUse );
	// If it is a "use only" trigger, then don't set the touch function.
	if ( ! (pev->spawnflags & SF_ENDSECTION_USEONLY) )
		SetTouch( &EndSectionTouch );
}

void CTriggerEndSection::EndSectionTouch( CBaseEntity *pOther )
{
	// Only save on clients
	if ( !pOther->IsNetClient() )
		return;
    
	SetTouch( NULL );

	if (pev->message)
	{
		g_engfuncs.pfnEndSection(STRING(pev->message));
	}
	UTIL_Remove( this );
}

void CTriggerEndSection :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "section"))
	{
		// Store this in message so we don't have to write save/restore for this ent
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


class CTriggerGravity : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerGravity, CBaseTrigger );
public:
	void Spawn( void );
	void GravityTouch( CBaseEntity *pOther );

	DECLARE_DATADESC();
};
LINK_ENTITY_TO_CLASS( trigger_gravity, CTriggerGravity );

BEGIN_DATADESC( CTriggerGravity )
	DEFINE_FUNCTION( GravityTouch ),
END_DATADESC()

void CTriggerGravity::Spawn( void )
{
	InitTrigger();
	SetTouch( &GravityTouch );
}

void CTriggerGravity::GravityTouch( CBaseEntity *pOther )
{
	// Only save on clients
	if ( !pOther->IsPlayer() )
		return;

	pOther->pev->gravity = pev->gravity;
}

class CTriggerPlayerFreeze : public CBaseDelay
{
	DECLARE_CLASS( CTriggerPlayerFreeze, CBaseDelay );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( trigger_playerfreeze, CTriggerPlayerFreeze );

void CTriggerPlayerFreeze::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance(INDEXENT( 1 ));

	if( !pActivator || !ShouldToggle( useType, FBitSet( pActivator->pev->flags, FL_FROZEN )))
		return;

	if (pActivator->pev->flags & FL_FROZEN)
		((CBasePlayer *)((CBaseEntity *)pActivator))->EnableControl(TRUE);
	else ((CBasePlayer *)((CBaseEntity *)pActivator))->EnableControl(FALSE);

	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
}

class CTriggerHideWeapon : public CBaseDelay
{
	DECLARE_CLASS( CTriggerHideWeapon, CBaseDelay );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( trigger_hideweapon, CTriggerHideWeapon );

void CTriggerHideWeapon::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		pActivator = CBaseEntity::Instance(INDEXENT( 1 ));

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if( !pActivator || !ShouldToggle( useType, FBitSet( pPlayer->m_iHideHUD, HIDEHUD_WEAPONS )))
		return;

	if (pPlayer->m_iHideHUD & HIDEHUD_WEAPONS)
		pPlayer->HideWeapons(FALSE);
	else pPlayer->HideWeapons(TRUE);

	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
}

//===========================================================
//LRC- trigger_startpatrol
//===========================================================
class CTriggerSetPatrol : public CBaseDelay
{
	DECLARE_CLASS( CTriggerSetPatrol, CBaseDelay );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( trigger_startpatrol, CTriggerSetPatrol );

void CTriggerSetPatrol::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iszPath"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerSetPatrol::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ), pActivator );
	CBaseEntity *pPath = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ), pActivator );

	if( pTarget && pPath )
	{
		CBaseMonster *pMonster = pTarget->MyMonsterPointer();
		if( pMonster ) pMonster->StartPatrol( pPath );
	}
}

// this is a really bad idea.
class CTriggerChangeTarget : public CBaseDelay
{
	DECLARE_CLASS( CTriggerChangeTarget, CBaseDelay );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();
private:
	string_t	m_iszNewTarget;
};
LINK_ENTITY_TO_CLASS( trigger_changetarget, CTriggerChangeTarget );

BEGIN_DATADESC( CTriggerChangeTarget )
	DEFINE_KEYFIELD( m_iszNewTarget, FIELD_STRING, "m_iszNewTarget" ),
END_DATADESC()

void CTriggerChangeTarget::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iszNewTarget"))
	{
		m_iszNewTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ), pActivator );

	if( pTarget )
	{
		if( FStrEq( STRING( m_iszNewTarget ), "*locus" ))
		{
			if( pActivator )
				pTarget->pev->target = pActivator->pev->targetname;
			else ALERT( at_error, "trigger_changetarget \"%s\" requires a locus!\n", STRING( pev->targetname ));
		}
		else
		{
			pTarget->pev->target = m_iszNewTarget;
		}

		CBaseMonster *pMonster = pTarget->MyMonsterPointer( );
		if( pMonster )
		{
			pMonster->m_pGoalEnt = NULL;
		}

		if( pTarget->IsFuncScreen( ) )
			pTarget->Activate(); // HACKHACK update portal camera
	}
}

class CTriggerChangeParent : public CBaseDelay
{
	DECLARE_CLASS( CTriggerChangeParent, CBaseDelay );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();
private:
	string_t	m_iszNewParent;
};

LINK_ENTITY_TO_CLASS( trigger_changeparent, CTriggerChangeParent );

BEGIN_DATADESC( CTriggerChangeParent )
	DEFINE_KEYFIELD( m_iszNewParent, FIELD_STRING, "m_iszNewParent" ),
END_DATADESC()

void CTriggerChangeParent :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iszNewParent" ))
	{
		m_iszNewParent = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeParent :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ), pActivator );

	if( pTarget )
	{
		if( FStrEq( STRING( m_iszNewParent ), "*locus" ))
		{
			if( pActivator )
				pTarget->SetParent( pActivator );
			else ALERT( at_error, "trigger_changeparent \"%s\" requires a locus!\n", STRING( pev->targetname ));
		}
		else
		{
			if( m_iszNewParent != NULL_STRING )
				pTarget->SetParent( m_iszNewParent );
			else pTarget->SetParent( NULL );
		}
	}
}

#define SF_CAMERA_PLAYER_POSITION	1
#define SF_CAMERA_PLAYER_TARGET	2
#define SF_CAMERA_PLAYER_TAKECONTROL	4
#define SF_CAMERA_PLAYER_HIDEHUD	8

class CTriggerCamera : public CBaseDelay
{
	DECLARE_CLASS( CTriggerCamera, CBaseDelay );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	STATE GetState( void ) { return m_state ? STATE_ON : STATE_OFF; }
	void FollowTarget( void );
	void Move( void );
	void Stop( void );

	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	EHANDLE m_hPlayer;
	EHANDLE m_hTarget;
	CBaseEntity *m_pPath;
	string_t m_sPath;
	string_t m_iszViewEntity;
	float m_flReturnTime;
	float m_flStopTime;
	float m_moveDistance;
	float m_targetSpeed;
	float m_initialSpeed;
	float m_acceleration;
	float m_deceleration;
	int m_state;
};

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
	SetThink( &FollowTarget );
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

	Vector vecGoal = UTIL_VecToAngles( m_hTarget->GetLocalOrigin() - GetLocalOrigin() );

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

//=======================================================================
// 		   Logic_generator
//=======================================================================
#define NORMAL_MODE			0
#define RANDOM_MODE			1
#define RANDOM_MODE_WITH_DEC		2

#define SF_GENERATOR_START_ON		BIT( 0 )

class CGenerator : public CBaseDelay
{
	DECLARE_CLASS( CGenerator, CBaseDelay );
public:
	void Spawn( void );
	void Think( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();
private:
	int m_iFireCount;
	int m_iMaxFireCount;
};

LINK_ENTITY_TO_CLASS( generator, CGenerator );

BEGIN_DATADESC( CGenerator )
	DEFINE_KEYFIELD( m_iMaxFireCount, FIELD_INTEGER, "maxcount" ),
	DEFINE_FIELD( m_iFireCount, FIELD_INTEGER ),
END_DATADESC()

void CGenerator :: Spawn( void )
{
	if( pev->button == RANDOM_MODE || pev->button == RANDOM_MODE_WITH_DEC )
	{ 	
		// set delay with decceleration
		if( !m_flDelay || pev->button == RANDOM_MODE_WITH_DEC )
			m_flDelay = 0.05;

		// generate max count automaticallly, if not set on map
		if( !m_iMaxFireCount ) m_iMaxFireCount = RANDOM_LONG( 100, 200 );
	}
	else
	{
		// Smart Field System ©
		if( !m_iMaxFireCount ) m_iMaxFireCount = -1; // disable counting for normal mode
	}

	if( FBitSet( pev->spawnflags, SF_GENERATOR_START_ON ))
	{
		m_iState = STATE_ON; // initialy off in random mode
		SetNextThink( m_flDelay );
	}
}

void CGenerator :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "maxcount" ))
	{
		m_iMaxFireCount = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	if( FStrEq( pkvd->szKeyName, "mode" ))
	{
		pev->button = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CGenerator :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_TOGGLE )
	{
		if( m_iState )
			useType = USE_OFF;
		else useType = USE_ON;
	}

	if( useType == USE_ON )
	{
		if( pev->button == RANDOM_MODE || pev->button == RANDOM_MODE_WITH_DEC )
		{
			if( pev->button == RANDOM_MODE_WITH_DEC )
				m_flDelay = 0.05f;
			m_iFireCount = RANDOM_LONG( 0, m_iMaxFireCount / 2 );
		}

		m_iState = STATE_ON;
		SetNextThink( 0 ); // immediately start firing targets
	}
	else if( useType == USE_OFF )
	{
		m_iState = STATE_OFF;
		DontThink();
	}
	else if( useType == USE_SET )
	{
		// set max count of impulses
		m_iMaxFireCount = value;
	}		
	else if( useType == USE_RESET )
	{
		// immediately reset
		m_iFireCount = 0;
	}
}

void CGenerator :: Think( void )
{
	if( m_iFireCount != -1 )
	{
		// if counter enabled
		if( m_iFireCount == m_iMaxFireCount )
		{
			m_iFireCount = NULL;
			m_iState = STATE_OFF;
			DontThink();
			return;
		}
 		else m_iFireCount++;
	}

	if( pev->button == RANDOM_MODE_WITH_DEC )
	{	
		// deceleration for random mode
		if( m_iMaxFireCount - m_iFireCount < 40 )
			m_flDelay += 0.05;
	}

	UTIL_FireTargets( pev->target, this, this, USE_TOGGLE );
	SetNextThink( m_flDelay );
}

//=====================================================
// trigger_command: activate a console command
//=====================================================
class CTriggerCommand : public CBaseEntity
{
	DECLARE_CLASS( CTriggerCommand, CBaseEntity );
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
LINK_ENTITY_TO_CLASS( trigger_command, CTriggerCommand );

void CTriggerCommand::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	char szCommand[256];

	if (pev->netname)
	{
		Q_snprintf( szCommand, sizeof( szCommand ), "%s\n", STRING( pev->netname ));
		SERVER_COMMAND( szCommand );
	}
}

//=====================================================
// trigger_impulse: activate a player command
//=====================================================
class CTriggerImpulse : public CBaseDelay
{
	DECLARE_CLASS( CTriggerImpulse, CBaseDelay );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};
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
