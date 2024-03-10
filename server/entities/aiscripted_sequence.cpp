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

#include "aiscripted_sequence.h"

// when a monster finishes an AI scripted sequence, we can choose
// a schedule to place them in. These defines are the aliases to
// resolve worldcraft input to real schedules (sjb)
#define SCRIPT_FINISHSCHED_DEFAULT	0
#define SCRIPT_FINISHSCHED_AMBUSH	1

LINK_ENTITY_TO_CLASS( aiscripted_sequence, CCineAI );

//=========================================================
// FCanOverrideState - returns true because scripted AI can
// possess entities regardless of their state.
//=========================================================
BOOL CCineAI :: FCanOverrideState( void )
{
	return TRUE;
}

// make the entity carry out the scripted sequence instructions, but without 
// destroying the monster's state.
void CCineAI :: PossessEntity( void )
{
	Schedule_t *pNewSchedule;

	CBaseEntity		*pEntity = m_hTargetEnt;
	CBaseMonster	*pTarget = NULL;
	if ( pEntity )
		pTarget = pEntity->MyMonsterPointer();

	if ( pTarget )
	{
		if ( !pTarget->CanPlaySequence( FCanOverrideState(), SS_INTERRUPT_AI ) )
		{
			ALERT( at_aiconsole, "(AI)Can't possess entity %s\n", STRING(pTarget->pev->classname) );
			return;
		}

		pTarget->m_pGoalEnt = this;
		pTarget->m_pCine = this;
		pTarget->m_hTargetEnt = this;

		m_saved_movetype = pTarget->pev->movetype;
		m_saved_solid = pTarget->pev->solid;
		m_saved_effects = pTarget->pev->effects;
		pTarget->pev->effects |= pev->effects;
		Vector vecTargetAngles;

		switch (m_fMoveTo)
		{
		case 0: 
		case 5:
			pTarget->m_scriptState = SCRIPT_WAIT; 
			break;

		case 1: 
			pTarget->m_scriptState = SCRIPT_WALK_TO_MARK; 
			break;

		case 2: 
			pTarget->m_scriptState = SCRIPT_RUN_TO_MARK; 
			break;

		case 4: 
			// zap the monster instantly to the site of the script entity.
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
			pTarget->pev->flags &= ~FL_ONGROUND;
			break;
		default:
			ALERT ( at_aiconsole, "aiscript:  invalid Move To Position value!" );
			break;
		}
		
		ALERT( at_aiconsole, "\"%s\" found and used\n", STRING( pTarget->pev->targetname ) );

		pTarget->m_IdealMonsterState = MONSTERSTATE_SCRIPT;

/*
		if (m_iszIdle)
		{
			StartSequence( pTarget, m_iszIdle, FALSE );
			if (FStrEq( STRING(m_iszIdle), STRING(m_iszPlay)))
			{
				pTarget->pev->framerate = 0;
			}
		}
*/
		// Already in a scripted state?
		if ( pTarget->m_MonsterState == MONSTERSTATE_SCRIPT )
		{
			pNewSchedule = pTarget->GetScheduleOfType( SCHED_AISCRIPT );
			pTarget->ChangeSchedule( pNewSchedule );
		}
	}
}



// lookup a sequence name and setup the target monster to play it
// overridden for CCineAI because it's ok for them to not have an animation sequence
// for the monster to play. For a regular Scripted Sequence, that situation is an error.
BOOL CCineAI :: StartSequence( CBaseMonster *pTarget, int iszSeq, BOOL completeOnEmpty )
{
	if ( iszSeq == 0 && completeOnEmpty )
	{
		// no sequence was provided. Just let the monster proceed, however, we still have to fire any Sequence target
		// and remove any non-repeatable CineAI entities here ( because there is code elsewhere that handles those tasks, but
		// not until the animation sequence is finished. We have to manually take care of these things where there is no sequence.

		SequenceDone ( pTarget );

		return TRUE;
	}

	pTarget->pev->sequence = pTarget->LookupSequence( STRING( iszSeq ) );

	if (pTarget->pev->sequence == -1)
	{
		ALERT( at_error, "%s: unknown aiscripted sequence \"%s\"\n", STRING( pTarget->pev->targetname ), STRING( iszSeq) );
		pTarget->pev->sequence = 0;
		// return FALSE;
	}

	pTarget->pev->frame = 0;
	pTarget->ResetSequenceInfo( );
	return TRUE;
}

//=========================================================
// When a monster finishes a scripted sequence, we have to 
// fix up its state and schedule for it to return to a 
// normal AI monster. 
//
// AI Scripted sequences will, depending on what the level
// designer selects:
//
// -Dirty the monster's schedule and drop out of the 
//  sequence in their current state.
//
// -Select a specific AMBUSH schedule, regardless of state.
//=========================================================
void CCineAI :: FixScriptMonsterSchedule( CBaseMonster *pMonster )
{
	switch ( m_iFinishSchedule )
	{
	case SCRIPT_FINISHSCHED_DEFAULT:
		pMonster->ClearSchedule();
		break;
	case SCRIPT_FINISHSCHED_AMBUSH:
		pMonster->ChangeSchedule( pMonster->GetScheduleOfType( SCHED_AMBUSH ) );
		break;
	default:
		ALERT ( at_aiconsole, "FixScriptMonsterSchedule - no case!\n" );
		pMonster->ClearSchedule();
		break;
	}
}
