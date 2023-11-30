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

#include "func_button.h"

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

int CBaseButton::ObjectCaps(void)
{
	int flags = FCAP_SET_MOVEDIR;
	if (pev->takedamage == DAMAGE_NO)
		flags |= FCAP_IMPULSE_USE;
	if (FBitSet(pev->spawnflags, SF_BUTTON_ONLYDIRECT))
		flags |= FCAP_ONLYDIRECT_USE;
	return ((BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)) | flags);
}