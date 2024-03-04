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

#include "func_train.h"
#include "scripted_trainsequence.h"

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

	if (pev->target == pTarg->pev->target)
	{
		ALERT(at_error, "Path entity \"%s\" have the target set as itself! Train stopped.\n", pTarg->GetTargetname());
		Stop();
		return;
	}

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
