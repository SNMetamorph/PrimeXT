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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "client.h"
#include "player.h"
#include "doors.h"

// =================== ENV_GLOBAL ==============================================

#define SF_GLOBAL_SET		BIT( 0 )	// set global state to initial state on spawn

class CEnvGlobal : public CBaseDelay
{
	DECLARE_CLASS( CEnvGlobal, CBaseDelay );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();
	
	int	m_triggermode;
	int	m_initialstate;
	string_t	m_globalstate;
};

BEGIN_DATADESC( CEnvGlobal )
	DEFINE_KEYFIELD( m_globalstate, FIELD_STRING, "globalstate" ),
	DEFINE_KEYFIELD( m_triggermode, FIELD_INTEGER, "triggermode" ),
	DEFINE_KEYFIELD( m_initialstate, FIELD_INTEGER, "initialstate" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_global, CEnvGlobal );

void CEnvGlobal :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "triggermode" ))
	{
		m_triggermode = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "initialstate" ))
	{
		m_initialstate = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "globalstate" ))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CEnvGlobal :: Spawn( void )
{
	if( !m_globalstate )
	{
		REMOVE_ENTITY( edict() );
		return;
	}

	if( FBitSet( pev->spawnflags, SF_GLOBAL_SET ))
	{
		if ( !gGlobalState.EntityInTable( m_globalstate ) )
			gGlobalState.EntityAdd( m_globalstate, gpGlobals->mapname, (GLOBALESTATE)m_initialstate );
	}
}

void CEnvGlobal :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	GLOBALESTATE oldState = gGlobalState.EntityGetState( m_globalstate );
	GLOBALESTATE newState;

	switch( m_triggermode )
	{
	case 0:
		newState = GLOBAL_OFF;
		break;
	case 1:
		newState = GLOBAL_ON;
		break;
	case 2:
		newState = GLOBAL_DEAD;
		break;
	case 3:
	default:
		if( oldState == GLOBAL_ON )
			newState = GLOBAL_OFF;
		else if( oldState == GLOBAL_OFF )
			newState = GLOBAL_ON;
		else newState = oldState;
		break;
	}

	if( gGlobalState.EntityInTable( m_globalstate ))
		gGlobalState.EntitySetState( m_globalstate, newState );
	else gGlobalState.EntityAdd( m_globalstate, gpGlobals->mapname, newState );
}

// =================== ENV_LOCAL ==============================================

#define SF_LOCAL_START_ON		BIT( 0 )
#define SF_LOCAL_REMOVE_ON_FIRE	BIT( 1 )

class CEnvLocal : public CBaseDelay
{
	DECLARE_CLASS( CEnvLocal, CBaseDelay );
public:
	void Spawn( void );
	void Think( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( env_local, CEnvLocal );

void CEnvLocal :: Spawn( void )
{
	if( FBitSet( pev->spawnflags, SF_LOCAL_START_ON ))
		m_iState = STATE_ON;
	else m_iState = STATE_OFF;
}

void CEnvLocal :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	m_hActivator = pActivator;
	pev->scale = value;

	if( useType == USE_TOGGLE )
	{
		// set use type
		if( m_iState == STATE_TURN_OFF || m_iState == STATE_OFF )
			useType = USE_ON;
		else if( m_iState == STATE_TURN_ON || m_iState == STATE_ON )
			useType = USE_OFF;
	}

	if( useType == USE_ON )
	{
		// enable entity
		if( m_iState == STATE_TURN_OFF || m_iState == STATE_OFF )
		{
 			if( m_flDelay )
 			{
 				// we have time to turning on
				m_iState = STATE_TURN_ON;
				SetNextThink( m_flDelay );
			}
			else
			{
				// just enable entity
				m_iState = STATE_ON;
				UTIL_FireTargets( pev->target, pActivator, this, USE_ON, pev->scale );
				if( FBitSet( pev->spawnflags, SF_LOCAL_REMOVE_ON_FIRE ))
					UTIL_Remove( this );
				else DontThink(); // break thinking
			}
		}
	}
	else if( useType == USE_OFF )
	{
		// disable entity
		if( m_iState == STATE_TURN_ON || m_iState == STATE_ON ) // activate turning off entity
		{
 			if( m_flWait )
 			{
 				// we have time to turning off
				m_iState = STATE_TURN_OFF;
				SetNextThink( m_flWait );
			}
			else
			{
				// just enable entity
				m_iState = STATE_OFF;
				UTIL_FireTargets( pev->target, pActivator, this, USE_OFF, pev->scale );
				if( FBitSet( pev->spawnflags, SF_LOCAL_REMOVE_ON_FIRE ))
					UTIL_Remove( this );
				else DontThink(); // break thinking
			}
		}
	}
 	else if( useType == USE_SET )
 	{
 		// just set state
 		if( value != 0.0f )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		DontThink(); // break thinking
	}
}

void CEnvLocal :: Think( void )
{
	if( m_iState == STATE_TURN_ON )
	{
		m_iState = STATE_ON;
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_ON, pev->scale );
	}
	else if( m_iState == STATE_TURN_OFF )
	{
		m_iState = STATE_OFF;
		UTIL_FireTargets( pev->target, m_hActivator, this, USE_OFF, pev->scale );
	}

	if( FBitSet( pev->spawnflags, SF_LOCAL_REMOVE_ON_FIRE ))
		UTIL_Remove( this );
	else DontThink(); // break thinking
}

// =================== ENV_TIME ==============================================
class CEnvTime : public CBaseDelay
{
	DECLARE_CLASS( CEnvTime, CBaseDelay );
public:
	void Spawn( void );
	void Think( void );
	void Activate( void );
	void KeyValue( KeyValueData *pkvd );
	int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
private:
	float m_flLevelTime;
	Vector m_vecTime;
};

LINK_ENTITY_TO_CLASS( env_time, CEnvTime );

void CEnvTime :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "time" ))
	{
		UTIL_StringToVector( (float*)(m_vecTime), pkvd->szValue );
		pkvd->fHandled = TRUE;
		pev->button = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CEnvTime :: Spawn( void )
{
	if( !pev->scale )
		pev->scale = 1.0f;	// default time scale

	if( !pev->button ) return;

	// first initialize?
	m_flLevelTime = bound( 0.0f, m_vecTime.x, 24.0f );
	m_flLevelTime += bound( 0.0f, m_vecTime.y, 60.0f ) * (1.0f/60.0f);
	m_flLevelTime += bound( 0.0f, m_vecTime.z, 60.0f ) * (1.0f/3600.0f);
	int name = MAKE_STRING( "GLOBAL_TIME" );

	if( !gGlobalState.EntityInTable( name )) // first initialize ?
		gGlobalState.EntityAdd( name, gpGlobals->mapname, (GLOBALESTATE)GLOBAL_ON, m_flLevelTime );
}

void CEnvTime :: Activate( void )
{
	// initialize level-time here
	m_flLevelTime = gGlobalState.EntityGetTime( MAKE_STRING( "GLOBAL_TIME" ));

	if( m_flLevelTime != -1.0f )
		SetNextThink( 0.0f );
}

void CEnvTime :: Think( void )
{
	if( m_flLevelTime == -1.0f )
		return; // no time specified

	m_flLevelTime += pev->scale * (1.0f/3600.0f); // evaluate level time
	if( m_flLevelTime > 24.0f ) m_flLevelTime = 0.001f; // A little hack to prevent disable the env_counter 

	// update clocks
	UTIL_FireTargets( pev->target, this, this, USE_SET, m_flLevelTime * 60.0f );	// encoded hours & minutes out
	UTIL_FireTargets( pev->netname, this, this, USE_SET, m_flLevelTime * 3600.0f );	// encoded hours & minutes & seconds out

	gGlobalState.EntitySetTime( MAKE_STRING( "GLOBAL_TIME" ), m_flLevelTime );
	SetNextThink( 1.0f ); // tick every second
}

// =================== MULTISOURCE ==============================================

#define SF_MULTI_INIT		BIT( 0 )

class CMultiSource : public CBaseDelay
{
	DECLARE_CLASS( CMultiSource, CBaseDelay );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Register( void );
	STATE GetState( void );

	DECLARE_DATADESC();

	string_t	m_globalstate;
	EHANDLE	m_rgEntities[MAX_MASTER_TARGETS];
	int	m_rgTriggered[MAX_MASTER_TARGETS];
	int	m_iTotal;
};

BEGIN_DATADESC( CMultiSource )
	DEFINE_AUTO_ARRAY( m_rgEntities, FIELD_EHANDLE ),
	DEFINE_AUTO_ARRAY( m_rgTriggered, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_globalstate, FIELD_STRING, "globalstate" ),
	DEFINE_FIELD( m_iTotal, FIELD_INTEGER ),
	DEFINE_FUNCTION( Register ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( multisource, CMultiSource );

void CMultiSource :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "offtarget" ))
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "globalstate" ))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CMultiSource :: Spawn()
{ 
	// set up think for later registration
	SetBits( pev->spawnflags, SF_MULTI_INIT ); // until it's initialized
	SetThink( &CMultiSource::Register );
	SetNextThink( 0.1 );
}

void CMultiSource :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	int i = 0;

	// find the entity in our list
	while( i < m_iTotal )
	{
		if( m_rgEntities[i++] == pCaller )
			break;
	}

	// if we didn't find it, report error and leave
	if( i > m_iTotal )
	{
		if( pCaller->GetTargetname( ))
			ALERT( at_error, "multisource \"%s\": Used by non-member %s \"%s\"\n", GetTargetname(), pCaller->GetTargetname( ));
		else ALERT( at_error, "multisource \"%s\": Used by non-member %s\n", GetTargetname(), pCaller->GetClassname( ));
		return;	
	}

	// store the state before the change, so we can compare it to the new state
	STATE s = GetState();

	// do the change
	m_rgTriggered[i-1] ^= 1;

	// did we change state?
	if( s == GetState( )) return;

	if( s == STATE_ON && !FStringNull( pev->netname ))
	{
		// the change disabled me and I have a "fire on disable" field
		ALERT( at_aiconsole, "Multisource %s deactivated (%d inputs)\n", GetTargetname(), m_iTotal );

		if( m_globalstate )
			UTIL_FireTargets( STRING( pev->netname ), NULL, this, USE_OFF, 0 );
		else UTIL_FireTargets( STRING( pev->netname ), NULL, this, USE_TOGGLE, 0 );
	}
	else if( s == STATE_OFF )
	{
		// the change activated me
		ALERT( at_aiconsole, "Multisource %s enabled (%d inputs)\n", GetTargetname(), m_iTotal );

		if( m_globalstate )
			UTIL_FireTargets( STRING( pev->target ), NULL, this, USE_ON, 0 );
		else UTIL_FireTargets( STRING( pev->target ), NULL, this, USE_TOGGLE, 0 );
	}
}

STATE CMultiSource :: GetState( void )
{
	// still initializing?
	if( FBitSet( pev->spawnflags, SF_MULTI_INIT ))
		return STATE_OFF;

	int i = 0;

	while( i < m_iTotal )
	{
		if( m_rgTriggered[i] == 0 )
			break;
		i++;
	}

	if( i == m_iTotal )
	{
		if( !m_globalstate || gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
			return STATE_ON;
	}
	return STATE_OFF;
}

void CMultiSource :: Register( void )
{ 
	m_iTotal = 0;
	memset( m_rgEntities, 0, MAX_MASTER_TARGETS * sizeof( EHANDLE ));

	SetThink( NULL );

	// search for all entities which target this multisource (pev->target)
	CBaseEntity *pTarget = UTIL_FindEntityByTarget( NULL, GetTargetname( ));

	while( pTarget && ( m_iTotal < MAX_MASTER_TARGETS ))
	{
		m_rgEntities[m_iTotal++] = pTarget;
		pTarget = UTIL_FindEntityByTarget( pTarget, GetTargetname( ));
	}

	// search for all monsters which target this multisource (TriggerTarget)
	pTarget = UTIL_FindEntityByMonsterTarget( NULL, GetTargetname( ));

	while( pTarget && ( m_iTotal < MAX_MASTER_TARGETS ))
	{
		m_rgEntities[m_iTotal++] = pTarget;
		pTarget = UTIL_FindEntityByMonsterTarget( pTarget, GetTargetname( ));
	}

	pTarget = UTIL_FindEntityByClassname( NULL, "multi_manager" );

	while( pTarget && ( m_iTotal < MAX_MASTER_TARGETS ))
	{
		if( pTarget->HasTarget( pev->targetname ))
			m_rgEntities[m_iTotal++] = pTarget;
		pTarget = UTIL_FindEntityByClassname( pTarget, "multi_manager" );
	}

	ClearBits( pev->spawnflags, SF_MULTI_INIT );
}

// =================== FUNC_BUTTON ==============================================

#define SF_BUTTON_DONTMOVE		BIT( 0 )

#define SF_BUTTON_ONLYDIRECT		BIT( 4 )	// LRC - button can't be used through walls.
#define SF_BUTTON_TOGGLE		BIT( 5 )	// button stays pushed until reactivated
#define SF_BUTTON_SPARK_IF_OFF	BIT( 6 )	// button sparks in OFF state
#define SF_BUTTON_DAMAGED_AT_LASER	BIT( 7 )	// if health is set can be damaged only at env_laser or gauss
#define SF_BUTTON_TOUCH_ONLY		BIT( 8 )	// button only fires as a result of USE key.

class CBaseButton : public CBaseToggle
{
	DECLARE_CLASS( CBaseButton, CBaseToggle );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData* pkvd);

	void ButtonActivate( );

	void ButtonTouch( CBaseEntity *pOther );
	void ButtonSpark ( void );
	void TriggerAndWait( void );
	void ButtonReturn( void );
	void ButtonBackHome( void );
	void ButtonUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	
	enum BUTTON_CODE { BUTTON_NOTHING, BUTTON_ACTIVATE, BUTTON_RETURN };
	BUTTON_CODE ButtonResponseToTouch( void );
	
	DECLARE_DATADESC();

	STATE GetState( void ) { return m_iState; }

	// Buttons that don't take damage can be IMPULSE used
	virtual int ObjectCaps( void )
	{
		int flags = FCAP_SET_MOVEDIR;
		if( pev->takedamage == DAMAGE_NO )
			flags |= FCAP_IMPULSE_USE;
		if( FBitSet( pev->spawnflags, SF_BUTTON_ONLYDIRECT ))
			flags |= FCAP_ONLYDIRECT_USE;
		return (BaseClass :: ObjectCaps() & (~FCAP_ACROSS_TRANSITION) | flags);
	}

	BOOL	m_fStayPushed;	// button stays pushed in until touched again?
	BOOL	m_fRotating;	// a rotating button?  default is a sliding button.

	locksound_t m_ls;		// door lock sounds
	
	int	m_iLockedSound;	// ordinals from entity selection
	byte	m_bLockedSentence;	
	int	m_iUnlockedSound;	
	byte	m_bUnlockedSentence;
	int	m_sounds;
};

BEGIN_DATADESC( CBaseButton )
	DEFINE_FIELD( m_fStayPushed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fRotating, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_sounds, FIELD_STRING, "sounds" ),
	DEFINE_KEYFIELD( m_iLockedSound, FIELD_STRING, "locked_sound" ),
	DEFINE_KEYFIELD( m_bLockedSentence, FIELD_CHARACTER, "locked_sentence" ),
	DEFINE_KEYFIELD( m_iUnlockedSound, FIELD_STRING, "unlocked_sound" ),	
	DEFINE_KEYFIELD( m_bUnlockedSentence, FIELD_CHARACTER, "unlocked_sentence" ),
	DEFINE_FUNCTION( ButtonTouch ),
	DEFINE_FUNCTION( ButtonSpark ),
	DEFINE_FUNCTION( TriggerAndWait ),
	DEFINE_FUNCTION( ButtonReturn ),
	DEFINE_FUNCTION( ButtonBackHome ),
	DEFINE_FUNCTION( ButtonUse ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_button, CBaseButton );

void CBaseButton :: Precache( void )
{
	const char *pszSound;

	if( FBitSet( pev->spawnflags, SF_BUTTON_SPARK_IF_OFF ))
	{
		// this button should spark in OFF state
		UTIL_PrecacheSound( "buttons/spark1.wav" );
		UTIL_PrecacheSound( "buttons/spark2.wav" );
		UTIL_PrecacheSound( "buttons/spark3.wav" );
		UTIL_PrecacheSound( "buttons/spark4.wav" );
		UTIL_PrecacheSound( "buttons/spark5.wav" );
		UTIL_PrecacheSound( "buttons/spark6.wav" );
	}

	pszSound = UTIL_ButtonSound( m_sounds );
	pev->noise = UTIL_PrecacheSound( pszSound );

	// get door button sounds, for doors which require buttons to open
	if( m_iLockedSound )
	{
		pszSound = UTIL_ButtonSound( m_iLockedSound );
		m_ls.sLockedSound = UTIL_PrecacheSound( pszSound );
	}

	if( m_iUnlockedSound )
	{
		pszSound = UTIL_ButtonSound( m_iUnlockedSound );
		m_ls.sUnlockedSound = UTIL_PrecacheSound( pszSound );
	}

	// get sentence group names, for doors which are directly 'touched' to open
	switch( m_bLockedSentence )
	{
	case 1: m_ls.sLockedSentence = MAKE_STRING( "NA" ); break; // access denied
	case 2: m_ls.sLockedSentence = MAKE_STRING( "ND" ); break; // security lockout
	case 3: m_ls.sLockedSentence = MAKE_STRING( "NF" ); break; // blast door
	case 4: m_ls.sLockedSentence = MAKE_STRING( "NFIRE" ); break; // fire door
	case 5: m_ls.sLockedSentence = MAKE_STRING( "NCHEM" ); break; // chemical door
	case 6: m_ls.sLockedSentence = MAKE_STRING( "NRAD" ); break; // radiation door
	case 7: m_ls.sLockedSentence = MAKE_STRING( "NCON" ); break; // gen containment
	case 8: m_ls.sLockedSentence = MAKE_STRING( "NH" ); break; // maintenance door
	case 9: m_ls.sLockedSentence = MAKE_STRING( "NG" ); break; // broken door
	default: m_ls.sLockedSentence = 0; break;
	}

	switch( m_bUnlockedSentence )
	{
	case 1: m_ls.sUnlockedSentence = MAKE_STRING( "EA" ); break; // access granted
	case 2: m_ls.sUnlockedSentence = MAKE_STRING( "ED" ); break; // security door
	case 3: m_ls.sUnlockedSentence = MAKE_STRING( "EF" ); break; // blast door
	case 4: m_ls.sUnlockedSentence = MAKE_STRING( "EFIRE" ); break; // fire door
	case 5: m_ls.sUnlockedSentence = MAKE_STRING( "ECHEM" ); break; // chemical door
	case 6: m_ls.sUnlockedSentence = MAKE_STRING( "ERAD" ); break; // radiation door
	case 7: m_ls.sUnlockedSentence = MAKE_STRING( "ECON" ); break; // gen containment
	case 8: m_ls.sUnlockedSentence = MAKE_STRING( "EH" ); break; // maintenance door
	default: m_ls.sUnlockedSentence = 0; break;
	}
}

void CBaseButton :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "locked_sound" ))
	{
		m_iLockedSound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "locked_sentence" ))
	{
		m_bLockedSentence = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "unlocked_sound" ))
	{
		m_iUnlockedSound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "unlocked_sentence" ))
	{
		m_bUnlockedSentence = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "sounds" ))
	{
		m_sounds = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseToggle::KeyValue( pkvd );
}

int CBaseButton :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if( FBitSet( pev->spawnflags, SF_BUTTON_DAMAGED_AT_LASER ) && !( bitsDamageType & DMG_ENERGYBEAM ))
		return 0;

	BUTTON_CODE code = ButtonResponseToTouch();
	
	if( code == BUTTON_NOTHING )
		return 0;

	// temporarily disable the touch function, until movement is finished.
	SetTouch( NULL );

	m_hActivator = CBaseEntity::Instance( pevAttacker );

	if( m_hActivator == NULL )
		return 0;

	if( code == BUTTON_RETURN )
	{
		EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise ), 1, ATTN_NORM );

		// toggle buttons fire when they get back to their "home" position
		if( !FBitSet( pev->spawnflags, SF_BUTTON_TOGGLE ))
			SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
		ButtonReturn();
	}
	else
	{
		// code == BUTTON_ACTIVATE
		ButtonActivate( );
	}

	return 0;
}

void CBaseButton :: Spawn( void )
{ 
	Precache();

	// this button should spark in OFF state
	if( FBitSet( pev->spawnflags, SF_BUTTON_SPARK_IF_OFF ))
	{
		SetThink( &CBaseButton::ButtonSpark );
		SetNextThink( 0.5f );
	}

	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;
	SET_MODEL( edict(), GetModel() );
	
	if( pev->speed == 0 )
		pev->speed = 40;

	if( pev->health > 0 )
	{
		pev->takedamage = DAMAGE_YES;
	}

	if( m_flWait == 0 ) m_flWait = 1;
	if( m_flLip == 0 ) m_flLip = 4;

	m_iState = STATE_OFF;

	m_vecPosition1 = GetLocalOrigin();

	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	Vector vecSize = pev->size - Vector( 2, 2, 2 );
	m_vecPosition2 = m_vecPosition1 + (pev->movedir * (DotProductAbs( pev->movedir, vecSize ) - m_flLip));

	// Is this a non-moving button?
	if((( m_vecPosition2 - m_vecPosition1 ).Length() < 1 ) || FBitSet( pev->spawnflags, SF_BUTTON_DONTMOVE ))
		m_vecPosition2 = m_vecPosition1;

	m_fStayPushed = (m_flWait == -1) ? TRUE : FALSE;
	m_fRotating = FALSE;

	// if the button is flagged for USE button activation only, take away it's touch function and add a use function
	if( FBitSet( pev->spawnflags, SF_BUTTON_TOUCH_ONLY ))
	{
		SetTouch( &CBaseButton::ButtonTouch );
		SetUse( NULL );
	}
	else 
	{
		SetTouch( NULL );
		SetUse( &CBaseButton::ButtonUse );
	}

	UTIL_SetOrigin( this, m_vecPosition1 );
	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
}

void CBaseButton :: ButtonSpark( void )
{
	UTIL_DoSpark( pev, pev->mins );

	// spark again at random interval
	SetNextThink( 0.1f + RANDOM_FLOAT ( 0.0f, 1.5f ));
}

void CBaseButton :: ButtonUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// ignore touches if button is moving, or pushed-in and waiting to auto-come-out.
	if( m_iState == STATE_TURN_ON || m_iState == STATE_TURN_OFF )
		return;		

	m_hActivator = pActivator;

	if( m_iState == STATE_ON )
	{
		if( !m_fStayPushed && FBitSet( pev->spawnflags, SF_BUTTON_TOGGLE ))
		{
			EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise ), 1, ATTN_NORM );
			ButtonReturn();
		}
	}
	else
	{
		ButtonActivate( );
	}
}

CBaseButton::BUTTON_CODE CBaseButton::ButtonResponseToTouch( void )
{
	// Ignore touches if button is moving, or pushed-in and waiting to auto-come-out.
	if( m_iState == STATE_TURN_ON || m_iState == STATE_TURN_OFF )
		return BUTTON_NOTHING;

	if( m_iState == STATE_ON && m_fStayPushed && !FBitSet( pev->spawnflags, SF_BUTTON_TOGGLE ))
		return BUTTON_NOTHING;

	if( m_iState == STATE_ON )
	{
		if(( FBitSet( pev->spawnflags, SF_BUTTON_TOGGLE )) && !m_fStayPushed )
		{
			return BUTTON_RETURN;
		}
	}

	return BUTTON_ACTIVATE;
}

void CBaseButton:: ButtonTouch( CBaseEntity *pOther )
{
	// ignore touches by anything but players
	if( !pOther->IsPlayer( )) return;

	m_hActivator = pOther;

	BUTTON_CODE code = ButtonResponseToTouch();
	if( code == BUTTON_NOTHING ) return;

	if( IsLockedByMaster( ))
	{
		// play button locked sound
		PlayLockSounds( pev, &m_ls, TRUE, TRUE );
		return;
	}

	// temporarily disable the touch function, until movement is finished.
	SetTouch( NULL );

	if( code == BUTTON_RETURN )
	{
		EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise ), 1, ATTN_NORM );
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
		ButtonReturn();
	}
	else
	{
		ButtonActivate( );
	}
}

void CBaseButton :: ButtonActivate( void )
{
	EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise ), 1, ATTN_NORM );
	
	if( IsLockedByMaster( ))
	{
		// button is locked, play locked sound
		PlayLockSounds( pev, &m_ls, TRUE, TRUE );
		return;
	}
	else
	{
		// button is unlocked, play unlocked sound
		PlayLockSounds( pev, &m_ls, FALSE, TRUE );
	}

	ASSERT( m_iState == STATE_OFF );
	m_iState = STATE_TURN_ON;
	
	if( pev->spawnflags & SF_BUTTON_DONTMOVE )
	{
		TriggerAndWait();
	}
	else
	{
		SetMoveDone( &CBaseButton::TriggerAndWait );

		if( !m_fRotating )
			LinearMove( m_vecPosition2, pev->speed );
		else AngularMove( m_vecAngle2, pev->speed );
	}
}

void CBaseButton::TriggerAndWait( void )
{
	ASSERT( m_iState == STATE_TURN_ON );

	if( IsLockedByMaster( )) return;

	m_iState = STATE_ON;
	
	// if button automatically comes back out, start it moving out.
	// else re-instate touch method
	if( m_fStayPushed || FBitSet( pev->spawnflags, SF_BUTTON_TOGGLE ))
	{ 
		// this button only works if USED, not touched!
		if( !FBitSet( pev->spawnflags, SF_BUTTON_TOUCH_ONLY ))
		{
			// all buttons are now use only
			SetTouch( NULL );
		}
		else
		{
			SetTouch( &CBaseButton::ButtonTouch );
		}
	}
	else
	{
		SetThink( &CBaseButton::ButtonReturn );
		if( m_flWait )
		{
			SetNextThink( m_flWait );
		}
		else
		{
			ButtonReturn();
		}
	}

	// use alternate textures	
	pev->frame = 1;

	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
}

void CBaseButton::ButtonReturn( void )
{
	ASSERT( m_iState == STATE_ON );
	m_iState = STATE_TURN_OFF;

	if( pev->spawnflags & SF_BUTTON_DONTMOVE )
	{
		ButtonBackHome();
	}
	else
	{
		SetMoveDone( &CBaseButton::ButtonBackHome );

		if( !m_fRotating )
			LinearMove( m_vecPosition1, pev->speed );
		else AngularMove( m_vecAngle1, pev->speed );
	}

	// use normal textures
	pev->frame = 0;
}

void CBaseButton::ButtonBackHome( void )
{
	ASSERT( m_iState == STATE_TURN_OFF );
	m_iState = STATE_OFF;

	if( FBitSet( pev->spawnflags, SF_BUTTON_TOGGLE ))
	{
		SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}

	if( !FStringNull( pev->target ))
	{
		CBaseEntity *pTarget = NULL;
		while( 1 )
		{
			pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ));

			if( FNullEnt( pTarget ))
				break;

			if( !FClassnameIs( pTarget->pev, "multisource" ))
				continue;

			pTarget->Use( m_hActivator, this, USE_TOGGLE, 0 );
		}
	}

	// Re-instate touch method, movement cycle is complete.

	// this button only works if USED, not touched!
	if( !FBitSet( pev->spawnflags, SF_BUTTON_TOUCH_ONLY ))
	{
		// all buttons are now use only	
		SetTouch( NULL );
	}
	else
	{
		SetTouch( &CBaseButton::ButtonTouch );
	}

	// reset think for a sparking button
	if( FBitSet( pev->spawnflags, SF_BUTTON_SPARK_IF_OFF ))
	{
		SetThink( &CBaseButton::ButtonSpark );
		SetNextThink( 0.5 );
	}
	else
	{
		DontThink();
	}
}

// =================== FUNC_ROT_BUTTON ==============================================

#define SF_ROTBUTTON_PASSABLE		BIT( 0 )
#define SF_ROTBUTTON_ROTATE_BACKWARDS	BIT( 1 )

class CRotButton : public CBaseButton
{
	DECLARE_CLASS( CRotButton, CBaseButton );
public:
	void Spawn( void );
	void Precache( void );
};

LINK_ENTITY_TO_CLASS( func_rot_button, CRotButton );

void CRotButton::Precache( void )
{
	const char *pszSound;

	pszSound = UTIL_ButtonSound( m_sounds );
	pev->noise = UTIL_PrecacheSound( pszSound );
}

void CRotButton::Spawn( void )
{
	Precache();

	// set the axis of rotation
	AxisDir( pev );

	// check for clockwise rotation
	if( FBitSet( pev->spawnflags, SF_ROTBUTTON_ROTATE_BACKWARDS ))
		pev->movedir = pev->movedir * -1;

	pev->movetype = MOVETYPE_PUSH;
	
	if( FBitSet( pev->spawnflags, SF_ROTBUTTON_PASSABLE ))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;

	// shared code use this flag as BUTTON_DONTMOVE so we need to clear it here
	ClearBits( pev->spawnflags, SF_ROTBUTTON_PASSABLE );
	ClearBits( pev->spawnflags, SF_BUTTON_SPARK_IF_OFF );
	ClearBits( pev->spawnflags, SF_BUTTON_DAMAGED_AT_LASER );

	SET_MODEL( edict(), GetModel() );
	
	if( pev->speed == 0 )
		pev->speed = 40;

	if( m_flWait == 0 )
		m_flWait = 1;

	if( pev->health > 0 )
	{
		pev->takedamage = DAMAGE_YES;
	}

	m_iState = STATE_OFF;
	m_vecAngle1 = GetLocalAngles();
	m_vecAngle2 = GetLocalAngles() + pev->movedir * m_flMoveDistance;

	ASSERTSZ( m_vecAngle1 != m_vecAngle2, "rotating button start/end positions are equal" );

	m_fStayPushed = (m_flWait == -1) ? TRUE : FALSE;
	m_fRotating = TRUE;

	// if the button is flagged for USE button activation only, take away it's touch function and add a use function
	if( !FBitSet( pev->spawnflags, SF_BUTTON_TOUCH_ONLY ))
	{
		SetTouch( NULL );
		SetUse( &CBaseButton::ButtonUse );
	}
	else
	{	
		// touchable button
		SetTouch( &CBaseButton::ButtonTouch );
	}

	UTIL_SetOrigin( this, GetLocalOrigin( ));
	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
}

// =================== MOMENTARY_ROT_BUTTON ==============================================

#define SF_MOMENTARY_ROT_DOOR			BIT( 0 )
#define SF_MOMENTARY_ROT_BUTTON_AUTO_RETURN	BIT( 4 )

class CMomentaryRotButton : public CBaseToggle
{
	DECLARE_CLASS( CMomentaryRotButton, CBaseToggle );
public:
	void	Spawn ( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	ButtonUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UseMoveDone( void );
	void	ReturnMoveDone( void );
	void	UpdateThink( void );
	void	SetPositionMoveDone( void );
	STATE	GetState( void ) { return m_iState; }
	void	UpdateSelf( float value, bool bPlaySound );
	void	PlaySound( void );
	void	UpdateTarget( float value );
	float	GetPos( const Vector &vecAngles );
	void	UpdateAllButtons( void );
	void	UpdateButton( void );
	void	SetPosition( float value );

	DECLARE_DATADESC();

	static CMomentaryRotButton *Instance( edict_t *pent )
	{
		return (CMomentaryRotButton *)GET_PRIVATE( pent );
	}

	virtual float GetPosition( void )
	{
		return GetPos( GetLocalAngles( ));
	}

	virtual int ObjectCaps( void ) 
	{ 
		int flags = (CBaseToggle::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)); 
		if( FBitSet( pev->spawnflags, SF_MOMENTARY_ROT_DOOR ))
			return flags;
		return flags | FCAP_CONTINUOUS_USE;
	}

	int	m_sounds;
	int	m_lastUsed;
	int	m_direction;
	bool	m_bUpdateTarget;	// used when jiggling so that we don't jiggle the target (door, etc)
	float	m_startPosition;
	float	m_returnSpeed;
	Vector	m_start;
	Vector	m_end;
};

BEGIN_DATADESC( CMomentaryRotButton )
	DEFINE_FIELD( m_bUpdateTarget, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_lastUsed, FIELD_INTEGER ),
	DEFINE_FIELD( m_direction, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_startPosition, FIELD_FLOAT, "m_flStartPos" ),
	DEFINE_KEYFIELD( m_returnSpeed, FIELD_FLOAT, "returnspeed" ),
	DEFINE_KEYFIELD( m_sounds, FIELD_STRING, "sounds" ),
	DEFINE_FIELD( m_start, FIELD_VECTOR ),
	DEFINE_FIELD( m_end, FIELD_VECTOR ),
	DEFINE_FUNCTION( ButtonUse ),
	DEFINE_FUNCTION( UseMoveDone ),
	DEFINE_FUNCTION( ReturnMoveDone ),
	DEFINE_FUNCTION( SetPositionMoveDone ),
	DEFINE_FUNCTION( UpdateThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( momentary_rot_button, CMomentaryRotButton );
LINK_ENTITY_TO_CLASS( momentary_rot_door, CMomentaryRotButton );

void CMomentaryRotButton :: Precache( void )
{
	const char *pszSound;

	pszSound = UTIL_ButtonSound( m_sounds );
	pev->noise = UTIL_PrecacheSound( pszSound );
}

void CMomentaryRotButton :: Spawn( void )
{
	// g-cont. just to have two seperate entities
	if( FClassnameIs( pev, "momentary_rot_door" ))
		SetBits( pev->spawnflags, SF_MOMENTARY_ROT_DOOR );

	Precache();

	AxisDir( pev );

	m_bUpdateTarget = true;

	if( pev->speed == 0 )
		pev->speed = 100;

	m_startPosition = bound( 0.0f, m_startPosition, 1.0f );

	if( m_flMoveDistance < 0 )
	{
		pev->movedir = pev->movedir * -1;
		m_flMoveDistance = -m_flMoveDistance;
	}

	m_direction = -1;

	m_start = GetLocalAngles() - pev->movedir * m_flMoveDistance * m_startPosition;
	m_end = GetLocalAngles() + pev->movedir * m_flMoveDistance * (1.0f - m_startPosition);

	pev->ideal_yaw = m_startPosition;
	m_iState = STATE_OFF;

	if( FBitSet( pev->spawnflags, SF_MOMENTARY_ROT_DOOR ))
		pev->solid = SOLID_BSP;
	else pev->solid = SOLID_NOT;

	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL( edict(), GetModel() );
	UTIL_SetOrigin( this, GetLocalOrigin( ));
	m_lastUsed = 0;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
	SetUse( &CMomentaryRotButton::ButtonUse );
}

void CMomentaryRotButton :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "returnspeed" ))
	{
		m_returnSpeed = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flStartPos" ))
	{
		m_startPosition = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "sounds"))
	{
		m_sounds = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CMomentaryRotButton :: PlaySound( void )
{
	EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise ), 1, ATTN_NORM );
}

float CMomentaryRotButton::GetPos( const Vector &vecAngles )
{
	float flScale;

	if(( pev->movedir.x < 0 ) || ( pev->movedir.y < 0 ) || ( pev->movedir.z < 0 ))
		flScale = -1;
	else flScale = 1;

	float flPos = flScale * AxisDelta( pev->spawnflags, vecAngles, m_start ) / m_flMoveDistance;

	return bound( 0.0f, flPos, 1.0f );
}

void CMomentaryRotButton :: SetPosition( float value )
{
	pev->ideal_yaw = bound( 0.0f, value, 1 );

	float flCurPos = GetPos( GetLocalAngles( ));

	if( flCurPos < pev->ideal_yaw )
	{
		// moving forward (from start to end).
		SetLocalAvelocity( pev->speed * pev->movedir );
		m_direction = 1;
	}
	else if( flCurPos > pev->ideal_yaw )
	{
		// moving backward (from end to start).
		SetLocalAvelocity( -pev->speed * pev->movedir );
		m_direction = -1;
	}
	else
	{
		// we're there already; nothing to do.
		SetLocalAvelocity( g_vecZero );
		return;
	}

	// g-cont. to avoid moving by user in back direction
	if( FBitSet( pev->spawnflags, SF_MOMENTARY_ROT_BUTTON_AUTO_RETURN ) && m_returnSpeed > 0 )
		m_lastUsed = 1;

	// play sound on set new pos
	PlaySound();

	SetMoveDone( &CMomentaryRotButton::SetPositionMoveDone );
	SetThink( &CMomentaryRotButton::UpdateThink );
	SetNextThink( 0 );

	// Think again in 0.1 seconds or the time that it will take us to reach our movement goal,
	// whichever is the shorter interval. This prevents us from overshooting and stuttering when we
	// are told to change position in very small increments.
	Vector vecNewAngles = m_start + pev->movedir * ( pev->ideal_yaw * m_flMoveDistance );
	float flAngleDelta = fabs( AxisDelta( pev->spawnflags, vecNewAngles, GetLocalAngles( )));
	float dt = flAngleDelta / pev->speed;

	if( dt < gpGlobals->frametime )
	{
		dt = gpGlobals->frametime;
		float speed = flAngleDelta / gpGlobals->frametime;
		SetLocalAvelocity( speed * pev->movedir * m_direction );
	}

	dt = bound( gpGlobals->frametime, dt, gpGlobals->frametime * 6 );

	SetMoveDoneTime( dt );
}

void CMomentaryRotButton :: ButtonUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator )) return;

	// always update target for direct use
	m_bUpdateTarget = true;

	if( useType == USE_SET )
	{
		if( pActivator->IsPlayer( ))
		{
			CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

			if( pPlayer->m_afPhysicsFlags & PFLAG_USING )
				UpdateAllButtons();
			else SetPosition( value );	// just targetting "from player" indirect
		}
		else SetPosition( value );	// not from player
	}
	else if( useType == USE_RESET )
	{
		// stop moving immediately
		UseMoveDone();
	}
}

void CMomentaryRotButton :: UpdateAllButtons( void )
{
	// NOTE: all the momentary buttons linked with same targetname
	// will be updated only in forward direction
	// in backward direction each button uses private AutoReturn code

	// update all rot buttons attached to my target
	CBaseEntity *pTarget = NULL;

	while( 1 )
	{
		pTarget = UTIL_FindEntityByTarget( pTarget, STRING( pev->target ));

		if( FNullEnt( pTarget ))
			break;

		if( FClassnameIs( pTarget->pev, "momentary_rot_button" ) || FClassnameIs( pTarget->pev, "momentary_rot_door" ))
		{
			CMomentaryRotButton *pEntity = (CMomentaryRotButton *)pTarget;

			if( pEntity != this )
			{
				// indirect use. disable update targets to avoid fire it twice
				// and prevent possible recursion
				pEntity->m_bUpdateTarget = false;
			}
			pEntity->UpdateButton();
		}
	}
}

void CMomentaryRotButton :: UpdateButton( void )
{
	// reverse our direction and play movement sound every time the player
	// pauses between uses.
	bool bPlaySound = false;
	
	if( !m_lastUsed )
	{
		bPlaySound = true;
		m_direction = -m_direction;
	}

	m_lastUsed = 1;

	float flPos = GetPos( GetLocalAngles() );
	UpdateSelf( flPos, bPlaySound );

	// Think every frame while we are moving.
	// HACK: Don't reset the think time if we already have a pending think.
	// This works around an issue with host_thread_mode > 0 when the player's
	// clock runs ahead of the server.
	if( !m_pfnThink )
	{
		SetThink( &CMomentaryRotButton::UpdateThink );
		SetNextThink( 0 );
	}
}

void CMomentaryRotButton :: UpdateThink( void )
{
	float value = GetPos( GetLocalAngles( ));
	UpdateTarget( value );
	SetNextThink( 0 );
}

void CMomentaryRotButton :: UpdateTarget( float value )
{
	if( !FStringNull( pev->target ) && m_bUpdateTarget )
	{
		CBaseEntity *pTarget = NULL;
		while( 1 )
		{
			pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ));

			if( FNullEnt( pTarget ))
				break;

			pTarget->Use( this, this, USE_SET, value );
		}
	}
}

void CMomentaryRotButton :: UpdateSelf( float value, bool bPlaySound )
{
	// set our move clock to 0.1 seconds in the future so we stop spinning unless we are
	// used again before then.
	SetMoveDoneTime( 0.1 );

	if( m_direction > 0 && value >= 1.0 )
	{
		// if we hit the end, zero our avelocity and snap to the end angles.
		SetLocalAvelocity( g_vecZero );
		SetLocalAngles( m_end );
		m_iState = STATE_ON;
		return;
	}
	else if( m_direction < 0 && value <= 0 )
	{
		// if we returned to the start, zero our avelocity and snap to the start angles.
		SetLocalAvelocity( g_vecZero );
		SetLocalAngles( m_start );
		m_iState = STATE_OFF;
		return;
	}

	// i'm "in use" player turn me :-)
	m_iState = STATE_IN_USE;
	
	if( bPlaySound ) PlaySound();

	SetLocalAvelocity(( m_direction * pev->speed ) * pev->movedir );
	SetMoveDone( &CMomentaryRotButton::UseMoveDone );
}

void CMomentaryRotButton :: UseMoveDone( void )
{
	SetLocalAvelocity( g_vecZero );

	// make sure our targets stop where we stopped.
	float flPos = GetPos( GetLocalAngles( ));
	UpdateTarget( flPos );

	m_lastUsed = 0;

	if( FBitSet( pev->spawnflags, SF_MOMENTARY_ROT_BUTTON_AUTO_RETURN ) && m_returnSpeed > 0 )
	{
		SetMoveDone( &CMomentaryRotButton::ReturnMoveDone );
		m_direction = -1;

		if( flPos >= 1.0f )
		{
			// disable use until button is waiting
			SetUse( NULL );

			// delay before autoreturn.
			SetMoveDoneTime( m_flDelay + 0.1f );
		}
		else SetMoveDoneTime( 0.1f );
	}
	else
	{
		SetThink( NULL );
		SetMoveDone( NULL );
	}
}

void CMomentaryRotButton :: ReturnMoveDone( void )
{
	float value = GetPos( GetLocalAngles() );

	SetUse( &CMomentaryRotButton::ButtonUse );

	if( value <= 0 )
	{
		// Got back to the start, stop spinning.
		SetLocalAvelocity( g_vecZero );
		SetLocalAngles( m_start );

		m_iState = STATE_OFF;

		UpdateTarget( 0 );

		SetMoveDoneTime( -1 );
		SetMoveDone( NULL );

		SetNextThink( -1 );
		SetThink( NULL );
	}
	else
	{
		m_iState = STATE_TURN_OFF;

		SetLocalAvelocity( -m_returnSpeed * pev->movedir );
		SetMoveDoneTime( 0.1f );

		SetThink( &CMomentaryRotButton::UpdateThink );
		SetNextThink( 0.01f );
	}
}

void CMomentaryRotButton :: SetPositionMoveDone( void )
{
	float flCurPos = GetPos( GetLocalAngles( ));

	if((( flCurPos >= pev->ideal_yaw ) && ( m_direction == 1 )) || (( flCurPos <= pev->ideal_yaw ) && ( m_direction == -1 )))
	{
		// g-cont. we need auto return after direct set position?
		if( FBitSet( pev->spawnflags, SF_MOMENTARY_ROT_BUTTON_AUTO_RETURN ) && m_returnSpeed > 0 )
		{
			SetMoveDone( &CMomentaryRotButton::ReturnMoveDone );
			m_direction = -1;

			if( flCurPos >= 1.0f )
			{
				// disable use until button is waiting
				SetUse( NULL );

				// delay before autoreturn.
				SetMoveDoneTime( m_flDelay + 0.1f );
			}
			else SetMoveDoneTime( 0.1f );
		}
		else
		{
			m_iState = STATE_OFF;

			// we reached or surpassed our movement goal.
			SetLocalAvelocity( g_vecZero );

			// BUGBUG: Won't this get the player stuck?
			SetLocalAngles( m_start + pev->movedir * ( pev->ideal_yaw * m_flMoveDistance ));
			SetNextThink( -1 );
			SetMoveDoneTime( -1 );
			UpdateTarget( pev->ideal_yaw );
		}

		m_lastUsed = 0;
		return;
	}

	// set right state
	if( flCurPos >= pev->ideal_yaw )
		m_iState = STATE_TURN_OFF;

	if( flCurPos <= pev->ideal_yaw )
		m_iState = STATE_TURN_ON;

	Vector vecNewAngles = m_start + pev->movedir * ( pev->ideal_yaw * m_flMoveDistance );
	float flAngleDelta = fabs( AxisDelta( pev->spawnflags, vecNewAngles, GetLocalAngles( )));
	float dt = flAngleDelta / pev->speed;

	if( dt < gpGlobals->frametime )
	{
		dt = gpGlobals->frametime;
		float speed = flAngleDelta / gpGlobals->frametime;
		SetLocalAvelocity( speed * pev->movedir * m_direction );
	}

	dt = bound( gpGlobals->frametime, dt, gpGlobals->frametime * 6 );

	SetMoveDoneTime( dt );
}

// =================== BUTTON_TARGET ==============================================

#define SF_BUTTON_TARGET_USE		BIT( 0 )
#define SF_BUTTON_TARGET_ON		BIT( 1 )

class CButtonTarget : public CBaseDelay
{
	DECLARE_CLASS( CButtonTarget, CBaseDelay );
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	virtual int ObjectCaps( void );
};

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

// =================== ENV_SPARK ==============================================

#define SF_SPARK_TOGGLE		BIT( 5 )
#define SF_SPARK_START_ON		BIT( 6 )

class CEnvSpark : public CBaseDelay
{
	DECLARE_CLASS( CEnvSpark, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void SparkThink( void );
	void SparkUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_spark, CEnvSpark );

BEGIN_DATADESC( CEnvSpark )
	DEFINE_FUNCTION( SparkThink ),
	DEFINE_FUNCTION( SparkUse ),
END_DATADESC()

void CEnvSpark::Precache(void)
{
	UTIL_PrecacheSound( "buttons/spark1.wav" );
	UTIL_PrecacheSound( "buttons/spark2.wav" );
	UTIL_PrecacheSound( "buttons/spark3.wav" );
	UTIL_PrecacheSound( "buttons/spark4.wav" );
	UTIL_PrecacheSound( "buttons/spark5.wav" );
	UTIL_PrecacheSound( "buttons/spark6.wav" );
}

void CEnvSpark::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "MaxDelay" ))
	{
		m_flDelay = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;	
	}
	else CBaseEntity :: KeyValue( pkvd );
}

void CEnvSpark :: Spawn(void)
{
	Precache();

	SetThink( NULL );
	SetUse( NULL );

	if( FBitSet( pev->spawnflags, SF_SPARK_TOGGLE ))
	{
		if( FBitSet( pev->spawnflags, SF_SPARK_START_ON ))
			SetThink( &CEnvSpark::SparkThink );	// start sparking

		SetUse( &CEnvSpark::SparkUse );
	}
	else
	{
		SetThink( &CEnvSpark::SparkThink );
	}		

	SetNextThink( 0.1f + RANDOM_FLOAT( 0.0f, 1.5f ));

	if( m_flDelay <= 0 )
		m_flDelay = 1.5f;
	m_iState = STATE_OFF;
}

void CEnvSpark :: SparkThink( void )
{
	UTIL_DoSpark( pev, GetAbsOrigin( ));
	SetNextThink( 0.1f + RANDOM_FLOAT( 0.0f, m_flDelay ));
	m_iState = STATE_ON;
}

void CEnvSpark :: SparkUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int active = (GetState() == STATE_ON);

	if ( !ShouldToggle( useType, active ) )
		return;

	if ( active )
	{
		m_iState = STATE_OFF;
		SetThink( NULL );
	}
	else
	{
		SetThink( &CEnvSpark::SparkThink );
		SetNextThink( 0.1f + RANDOM_FLOAT( 0.0f, m_flDelay ));
	}
}
