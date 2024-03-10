/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
/*


===== scripted.cpp ========================================================

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "scripted_sequence.h"

BOOL CBaseMonster :: ExitScriptedSequence( )
{
	if ( pev->deadflag == DEAD_DYING )
	{
		// is this legal?
		// BUGBUG -- This doesn't call Killed()
		m_IdealMonsterState = MONSTERSTATE_DEAD;
		return FALSE;
	}

	if (m_pCine)
	{
		m_pCine->CancelScript( );
	}

	return TRUE;
}

BOOL CBaseMonster :: CineCleanup( void )
{
	CCineMonster *pOldCine = m_pCine;

	// am I linked to a cinematic?
	if (m_pCine)
	{
		// okay, reset me to what it thought I was before
		m_pCine->m_hTargetEnt = NULL;
		pev->movetype = m_pCine->m_saved_movetype;
		pev->solid = m_pCine->m_saved_solid;
		pev->effects = m_pCine->m_saved_effects;
	}
	else
	{
		// arg, punt
		pev->movetype = MOVETYPE_STEP;// this is evil
		pev->solid = SOLID_SLIDEBOX;
	}
	m_pCine = NULL;
	m_hTargetEnt = NULL;
	m_pGoalEnt = NULL;
	if (pev->deadflag == DEAD_DYING)
	{
		// last frame of death animation?
		pev->health			= 0;
		pev->framerate		= 0.0;
		pev->solid			= SOLID_NOT;
		SetState( MONSTERSTATE_DEAD );
		pev->deadflag = DEAD_DEAD;
		UTIL_SetSize( pev, pev->mins, Vector(pev->maxs.x, pev->maxs.y, pev->mins.z + 2) );

		if ( pOldCine && FBitSet( pOldCine->pev->spawnflags, SF_SCRIPT_LEAVECORPSE ) )
		{
			SetUse( NULL );		// BUGBUG -- This doesn't call Killed()
			SetThink( NULL );	// This will probably break some stuff
			SetTouch( NULL );
		}
		else
			SUB_StartFadeOut(); // SetThink( &SUB_DoNothing );
		// This turns off animation & physics in case their origin ends up stuck in the world or something
		StopAnimation();
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NOINTERP;	// Don't interpolate either, assume the corpse is positioned in its final resting place
		return FALSE;
	}

	// If we actually played a sequence
	if ( pOldCine && pOldCine->m_iszPlay )
	{
		if ( !(pOldCine->pev->spawnflags & SF_SCRIPT_NOSCRIPTMOVEMENT) )
		{
			// reset position
			Vector new_origin, new_angle;
			GetBonePosition( 0, new_origin, new_angle );

			// Figure out how far they have moved
			// We can't really solve this problem because we can't query the movement of the origin relative
			// to the sequence.  We can get the root bone's position as we do here, but there are
			// cases where the root bone is in a different relative position to the entity's origin
			// before/after the sequence plays.  So we are stuck doing this:

			// !!!HACKHACK: Float the origin up and drop to floor because some sequences have
			// irregular motion that can't be properly accounted for.

			// UNDONE: THIS SHOULD ONLY HAPPEN IF WE ACTUALLY PLAYED THE SEQUENCE.
			Vector oldOrigin = pev->origin;

			// UNDONE: ugly hack.  Don't move monster if they don't "seem" to move
			// this really needs to be done with the AX,AY,etc. flags, but that aren't consistantly
			// being set, so animations that really do move won't be caught.
			if ((oldOrigin - new_origin).Length2D() < 8.0)
				new_origin = oldOrigin;

			pev->origin.x = new_origin.x;
			pev->origin.y = new_origin.y;
			pev->origin.z += 1;

			pev->flags |= FL_ONGROUND;
			int drop = UTIL_DropToFloor( this );
			
			// Origin in solid?  Set to org at the end of the sequence
			if ( drop < 0 )
				pev->origin = oldOrigin;
			else if ( drop == 0 ) // Hanging in air?
			{
				pev->origin.z = new_origin.z;
				pev->flags &= ~FL_ONGROUND;
			}
			// else entity hit floor, leave there

			// pEntity->pev->origin.z = new_origin.z + 5.0; // damn, got to fix this

			UTIL_SetOrigin( this, pev->origin );
			pev->effects |= EF_NOINTERP;
		}

		if (!FBitSet(pOldCine->pev->spawnflags, SF_SCRIPT_FIXANIMTRANSITIONS))
		{
			// We should have some animation to put these guys in, but for now it's idle.
			// Due to NOINTERP above, there won't be any blending between this anim & the sequence
			m_Activity = ACT_RESET;
		}
	}
	// set them back into a normal state
	pev->enemy = NULL;
	if ( pev->health > 0 )
		m_IdealMonsterState = MONSTERSTATE_IDLE; // m_previousState;
	else
	{
		// Dropping out because he got killed
		// Can't call killed() no attacker and weirdness (late gibbing) may result
		m_IdealMonsterState = MONSTERSTATE_DEAD;
		SetConditions( bits_COND_LIGHT_DAMAGE );
		pev->deadflag = DEAD_DYING;
		FCheckAITrigger();
		pev->deadflag = DEAD_NO;
	}


	//	SetAnimation( m_MonsterState );
	ClearBits(pev->spawnflags, SF_MONSTER_WAIT_FOR_SCRIPT );

	return TRUE;
}
