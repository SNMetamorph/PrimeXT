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

#include "scripted_sequence.h"
#include "animation.h"

LINK_ENTITY_TO_CLASS( scripted_sequence, CCineMonster );

BEGIN_DATADESC( CCineMonster )
	DEFINE_KEYFIELD( m_iszIdle, FIELD_STRING, "m_iszIdle" ),
	DEFINE_KEYFIELD( m_iszPlay, FIELD_STRING, "m_iszPlay" ),
	DEFINE_KEYFIELD( m_iszEntity, FIELD_STRING, "m_iszEntity" ),
	DEFINE_KEYFIELD( m_fMoveTo, FIELD_INTEGER, "m_fMoveTo" ),
	DEFINE_KEYFIELD( m_flRepeat, FIELD_FLOAT, "m_flRepeat" ),
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "m_flRadius" ),
	DEFINE_KEYFIELD( m_iszFireOnBegin, FIELD_STRING, "m_iszFireOnBegin" ),
	DEFINE_FIELD( m_iDelay, FIELD_INTEGER ),
	DEFINE_FIELD( m_startTime, FIELD_TIME ),

	DEFINE_FIELD( m_saved_movetype, FIELD_INTEGER ),
	DEFINE_FIELD( m_saved_solid, FIELD_INTEGER ),
	DEFINE_FIELD( m_saved_effects, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iFinishSchedule, FIELD_INTEGER, "m_iFinishSchedule" ),
	DEFINE_FIELD( m_interruptable, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( CineThink )
END_DATADESC()

/*
classname "scripted_sequence"
targetname "me" - there can be more than one with the same name, and they act in concert
target "the_entity_I_want_to_start_playing" or "class entity_classname" will pick the closest inactive scientist
play "name_of_sequence"
idle "name of idle sequence to play before starting"
donetrigger "whatever" - can be any other triggerable entity such as another sequence, train, door, or a special case like "die" or "remove"
moveto - if set the monster first moves to this nodes position
range # - only search this far to find the target
spawnflags - (stop if blocked, stop if player seen)
*/

//
// Cache user-entity-field values until spawn is called.
//
void CCineMonster :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iszIdle"))
	{
		m_iszIdle = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszPlay"))
	{
		m_iszPlay = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszEntity"))
	{
		m_iszEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fMoveTo"))
	{
		m_fMoveTo = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flRepeat"))
	{
		m_flRepeat = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flRadius"))
	{
		m_flRadius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iFinishSchedule"))
	{
		m_iFinishSchedule = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszFireOnBegin"))
	{
		m_iszFireOnBegin = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue( pkvd );
	}
}

void CCineMonster :: Spawn( void )
{
	// pev->solid = SOLID_TRIGGER;
	// UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
	pev->solid = SOLID_NOT;

	m_iState = STATE_OFF;

	// if no targetname, start now
	if ( FStringNull(pev->targetname) || !FStringNull( m_iszIdle ) )
	{
		SetThink( &CCineMonster::CineThink );
		pev->nextthink = gpGlobals->time + 1.0;
		// Wait to be used?
		if ( pev->targetname )
			m_startTime = gpGlobals->time + 1E6;
	}
	if ( pev->spawnflags & SF_SCRIPT_NOINTERRUPT )
		m_interruptable = FALSE;
	else
		m_interruptable = TRUE;
}

void CCineMonster :: OnRemove( void )
{
	// entity will be removed, cancel script playing
	CancelScript();
}

//=========================================================
// FCanOverrideState - returns FALSE, scripted sequences 
// cannot possess entities regardless of state.
//=========================================================
BOOL CCineMonster :: FCanOverrideState( void )
{
	if ( pev->spawnflags & SF_SCRIPT_OVERRIDESTATE )
		return TRUE;
	return FALSE;
}

//
// CineStart
//
void CCineMonster :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	// do I already know who I should use
	CBaseEntity		*pEntity = m_hTargetEnt;
	CBaseMonster	*pTarget = NULL;

	if ( pEntity )
		pTarget = pEntity->MyMonsterPointer();

	if ( pTarget )
	{
		// am I already playing the script?
		if ( pTarget->m_scriptState == SCRIPT_PLAYING )
			return;

		m_startTime = gpGlobals->time + 0.05;
	}
	else
	{
		// if not, try finding them
		SetThink( &CCineMonster::CineThink );
		pev->nextthink = gpGlobals->time;
	}
}


// This doesn't really make sense since only MOVETYPE_PUSH get 'Blocked' events
void CCineMonster :: Blocked( CBaseEntity *pOther )
{

}

void CCineMonster :: Touch( CBaseEntity *pOther )
{
}

//
// ********** Cinematic DIE **********
//
void CCineMonster :: Die( void )
{
	SetThink( &CBaseEntity::SUB_Remove );
}

//
// ********** Cinematic PAIN **********
//
void CCineMonster :: Pain( void )
{

}

//
// ********** Cinematic Think **********
//

// find a viable entity
int CCineMonster :: FindEntity( void )
{
	edict_t *pentTarget;

	pentTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(m_iszEntity));
	m_hTargetEnt = NULL;
	CBaseMonster	*pTarget = NULL;

	while (!FNullEnt(pentTarget))
	{
		if ( FBitSet( VARS(pentTarget)->flags, FL_MONSTER ))
		{
			pTarget = GetMonsterPointer( pentTarget );
			if ( pTarget && pTarget->CanPlaySequence( FCanOverrideState(), SS_INTERRUPT_BY_NAME ) )
			{
				m_hTargetEnt = pTarget;
				return TRUE;
			}
			ALERT( at_aiconsole, "Found %s, but can't play!\n", STRING(m_iszEntity) );
		}
		pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, STRING(m_iszEntity));
		pTarget = NULL;
	}
	
	if ( !pTarget )
	{
		CBaseEntity *pEntity = NULL;
		while ((pEntity = UTIL_FindEntityInSphere( pEntity, GetAbsOrigin(), m_flRadius )) != NULL)
		{
			if (FClassnameIs( pEntity->pev, STRING(m_iszEntity)))
			{
				if ( FBitSet( pEntity->pev->flags, FL_MONSTER ))
				{
					pTarget = pEntity->MyMonsterPointer( );
					if ( pTarget && pTarget->CanPlaySequence( FCanOverrideState(), SS_INTERRUPT_IDLE ) )
					{
						m_hTargetEnt = pTarget;
						return TRUE;
					}
				}
			}
		}
	}
	pTarget = NULL;
	m_hTargetEnt = NULL;
	return FALSE;
}

// make the entity enter a scripted sequence
void CCineMonster :: PossessEntity( void )
{
	CBaseEntity		*pEntity = m_hTargetEnt;
	CBaseMonster	*pTarget = NULL;
	if ( pEntity )
		pTarget = pEntity->MyMonsterPointer();

	if ( pTarget )
	{

	// FindEntity() just checked this!
#if 0
		if ( !pTarget->CanPlaySequence(  FCanOverrideState() ) )
		{
			ALERT( at_aiconsole, "Can't possess entity %s\n", STRING(pTarget->pev->classname) );
			return;
		}
#endif

		pTarget->m_pGoalEnt = this;
		pTarget->m_pCine = this;
		pTarget->m_hTargetEnt = this;

		m_saved_movetype = pTarget->pev->movetype;
		m_saved_solid = pTarget->pev->solid;
		m_saved_effects = pTarget->pev->effects;
		pTarget->pev->effects |= pev->effects;
		Vector vecTargetAngles;

		m_iState = STATE_ON; // LRC: assume we'll set it to 'on', unless proven otherwise...

		switch (m_fMoveTo)
		{
		case 0: 
			pTarget->m_scriptState = SCRIPT_WAIT; 
			break;

		case 1: 
			pTarget->m_scriptState = SCRIPT_WALK_TO_MARK; 
			m_iState = STATE_TURN_ON;
			DelayStart( 1 ); 
			break;

		case 2: 
			pTarget->m_scriptState = SCRIPT_RUN_TO_MARK; 
			m_iState = STATE_TURN_ON;
			DelayStart( 1 ); 
			break;

		case 4: 
			UTIL_SetOrigin( pTarget, GetAbsOrigin() );
			pTarget->pev->ideal_yaw = GetAbsAngles().y;
			pTarget->SetLocalAvelocity( g_vecZero );
			pTarget->SetLocalVelocity( g_vecZero );
			pTarget->pev->effects |= EF_NOINTERP;
			vecTargetAngles = pTarget->GetLocalAngles();
			vecTargetAngles.y = GetAbsAngles().y;
			pTarget->SetLocalAngles( vecTargetAngles );
			pTarget->m_scriptState = SCRIPT_WAIT;
			m_startTime = gpGlobals->time + 1E6;
			// UNDONE: Add a flag to do this so people can fixup physics after teleporting monsters
			// pTarget->pev->flags &= ~FL_ONGROUND;
			break;
		}
//		ALERT( at_aiconsole, "\"%s\" found and used (INT: %s)\n", STRING( pTarget->pev->targetname ), FBitSet(pev->spawnflags, SF_SCRIPT_NOINTERRUPT)?"No":"Yes" );

		pTarget->m_IdealMonsterState = MONSTERSTATE_SCRIPT;
		if (m_iszIdle)
		{
			StartSequence( pTarget, m_iszIdle, FALSE );
			if (FStrEq( STRING(m_iszIdle), STRING(m_iszPlay)))
			{
				pTarget->pev->framerate = 0;
			}
		}
	}
}

void CCineMonster::AllowInterrupt( BOOL fAllow )
{
	if ( pev->spawnflags & SF_SCRIPT_NOINTERRUPT )
		return;
	m_interruptable = fAllow;
}

BOOL CCineMonster::CanInterrupt( void )
{
	if ( !m_interruptable )
		return FALSE;

	CBaseEntity *pTarget = m_hTargetEnt;

	if ( pTarget != NULL && pTarget->pev->deadflag == DEAD_NO )
		return TRUE;

	return FALSE;
}

int	CCineMonster::IgnoreConditions( void )
{
	if ( CanInterrupt() )
		return 0;
	return SCRIPT_BREAK_CONDITIONS;
}

void ScriptEntityCancel( edict_t *pentCine )
{
	// make sure they are a scripted_sequence
	if (FClassnameIs( pentCine, "scripted_sequence"))
	{
		CCineMonster *pCineTarget = GetClassPtr((CCineMonster *)VARS(pentCine));
		// make sure they have a monster in mind for the script
		CBaseEntity		*pEntity = pCineTarget->m_hTargetEnt;
		CBaseMonster	*pTarget = NULL;
		if ( pEntity )
			pTarget = pEntity->MyMonsterPointer();

		pCineTarget->m_iState = STATE_OFF;
		
		if (pTarget)
		{
			// make sure their monster is actually playing a script
			if ( pTarget->m_MonsterState == MONSTERSTATE_SCRIPT )
			{
				// tell them do die
				pTarget->m_scriptState = CCineMonster::SCRIPT_CLEANUP;
				// do it now
				pTarget->CineCleanup( );
			}
		}
	}
}

// find all the cinematic entities with my targetname and stop them from playing
void CCineMonster :: CancelScript( void )
{
	ALERT( at_aiconsole, "Cancelling script: %s\n", STRING(m_iszPlay) );
	
	if ( !pev->targetname )
	{
		ScriptEntityCancel( edict() );
		return;
	}

	edict_t *pentCineTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pev->targetname));

	while (!FNullEnt(pentCineTarget))
	{
		ScriptEntityCancel( pentCineTarget );
		pentCineTarget = FIND_ENTITY_BY_TARGETNAME(pentCineTarget, STRING(pev->targetname));
	}
}

// find all the cinematic entities with my targetname and tell them to wait before starting
void CCineMonster :: DelayStart( int state )
{
	edict_t *pentCine = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pev->targetname));

	while (!FNullEnt(pentCine))
	{
		if (FClassnameIs( pentCine, "scripted_sequence" ))
		{
			CCineMonster *pTarget = GetClassPtr((CCineMonster *)VARS(pentCine));
			if (state)
			{
				pTarget->m_iDelay++;
			}
			else
			{
				pTarget->m_iDelay--;
				if (pTarget->m_iDelay <= 0)
				{
					pTarget->m_iState = STATE_ON;
					UTIL_FireTargets( m_iszFireOnBegin, this, this, USE_TOGGLE, 0);
					pTarget->m_startTime = gpGlobals->time + 0.05;
				}
			}
		}
		pentCine = FIND_ENTITY_BY_TARGETNAME(pentCine, STRING(pev->targetname));
	}
}

// Find an entity that I'm interested in and precache the sounds he'll need in the sequence.
void CCineMonster :: Activate( void )
{
	edict_t			*pentTarget;
	CBaseMonster	*pTarget;

	// The entity name could be a target name or a classname
	// Check the targetname
	pentTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(m_iszEntity));
	pTarget = NULL;

	while (!pTarget && !FNullEnt(pentTarget))
	{
		if ( FBitSet( VARS(pentTarget)->flags, FL_MONSTER ))
		{
			pTarget = GetMonsterPointer( pentTarget );
		}
		pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, STRING(m_iszEntity));
	}
	
	// If no entity with that targetname, check the classname
	if ( !pTarget )
	{
		pentTarget = FIND_ENTITY_BY_CLASSNAME(NULL, STRING(m_iszEntity));
		while (!pTarget && !FNullEnt(pentTarget))
		{
			pTarget = GetMonsterPointer( pentTarget );
			pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, STRING(m_iszEntity));
		}
	}
	// Found a compatible entity
	if ( pTarget )
	{
		void *pmodel;
		pmodel = GET_MODEL_PTR( pTarget->edict() );
		if ( pmodel )
		{
			// Look through the event list for stuff to precache
			SequencePrecache( pmodel, STRING( m_iszIdle ) );
			SequencePrecache( pmodel, STRING( m_iszPlay ) );
		}
	}
}

void CCineMonster :: CineThink( void )
{
	if (FindEntity())
	{
		PossessEntity( );
		ALERT( at_aiconsole, "script \"%s\" using monster \"%s\"\n", STRING( pev->targetname ), STRING( m_iszEntity ) );
	}
	else
	{
		CancelScript( );
		ALERT( at_aiconsole, "script \"%s\" can't find monster \"%s\"\n", STRING( pev->targetname ), STRING( m_iszEntity ) );
		pev->nextthink = gpGlobals->time + 1.0;
	}
}

// lookup a sequence name and setup the target monster to play it
BOOL CCineMonster :: StartSequence( CBaseMonster *pTarget, int iszSeq, BOOL completeOnEmpty )
{
	if ( !iszSeq && completeOnEmpty )
	{
		SequenceDone( pTarget );
		return FALSE;
	}

	pTarget->pev->sequence = pTarget->LookupSequence( STRING( iszSeq ) );
	if (pTarget->pev->sequence == -1)
	{
		ALERT( at_error, "%s: unknown scripted sequence \"%s\"\n", STRING( pTarget->pev->targetname ), STRING( iszSeq) );
		pTarget->pev->sequence = 0;
		// return FALSE;
	}

#if 0
	char *s;
	if ( pev->spawnflags & SF_SCRIPT_NOINTERRUPT ) 
		s = "No";
	else
		s = "Yes";

	ALERT( at_console, "%s (%s): started \"%s\":INT:%s\n", STRING( pTarget->pev->targetname ), STRING( pTarget->pev->classname ), STRING( iszSeq), s );
#endif

	pTarget->pev->frame = 0;
	pTarget->ResetSequenceInfo( );
	return TRUE;
}

//=========================================================
// SequenceDone - called when a scripted sequence animation
// sequence is done playing ( or when an AI Scripted Sequence
// doesn't supply an animation sequence to play ). Expects
// the CBaseMonster pointer to the monster that the sequence
// possesses. 
//=========================================================
void CCineMonster :: SequenceDone ( CBaseMonster *pMonster )
{
	//ALERT( at_aiconsole, "Sequence %s finished\n", STRING( m_pCine->m_iszPlay ) );
	m_iState = STATE_OFF; // we've finished.

	if ( !( pev->spawnflags & SF_SCRIPT_REPEATABLE ) )
	{
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	
	// This is done so that another sequence can take over the monster when triggered by the first
	
	pMonster->CineCleanup();

	FixScriptMonsterSchedule( pMonster );
	
	// This may cause a sequence to attempt to grab this guy NOW, so we have to clear him out
	// of the existing sequence
	SUB_UseTargets( NULL, USE_TOGGLE, 0 );
}

//=========================================================
// When a monster finishes a scripted sequence, we have to 
// fix up its state and schedule for it to return to a 
// normal AI monster. 
//
// Scripted sequences just dirty the Schedule and drop the
// monster in Idle State.
//=========================================================
void CCineMonster :: FixScriptMonsterSchedule( CBaseMonster *pMonster )
{
	if ( pMonster->m_IdealMonsterState != MONSTERSTATE_DEAD )
		pMonster->m_IdealMonsterState = MONSTERSTATE_IDLE;
	pMonster->ClearSchedule();
}
