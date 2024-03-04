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

#include "env_sprite.h"
#include "trains.h"

LINK_ENTITY_TO_CLASS( env_sprite, CSprite );
LINK_ENTITY_TO_CLASS( env_spritetrain, CSprite );

BEGIN_DATADESC( CSprite )
	DEFINE_FIELD( m_lastTime, FIELD_TIME ),
	DEFINE_FIELD( m_maxFrame, FIELD_FLOAT ),
	DEFINE_FIELD( m_activated, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( AnimateThink ),
	DEFINE_FUNCTION( ExpandThink ),
	DEFINE_FUNCTION( AnimateUntilDead ),
	DEFINE_FUNCTION( SpriteMove ),
END_DATADESC()

void CSprite::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->frame = 0;

	if( FClassnameIs( pev, "env_spritetrain" ))
		pev->movetype = MOVETYPE_NOCLIP;
	else
		pev->movetype = MOVETYPE_NONE;

	Precache();
	SET_MODEL( ENT(pev), STRING(pev->model) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	if ( pev->targetname && !(pev->spawnflags & SF_SPRITE_STARTON) )
		TurnOff();
	else
		TurnOn();
	
	// Worldcraft only sets y rotation, copy to Z
	if ( UTIL_GetSpriteType( pev->modelindex ) != SPR_ORIENTED && GetLocalAngles().y != 0 && GetLocalAngles().z == 0 )
	{
		Vector angles = GetLocalAngles();

		angles.z = angles.y;
		angles.y = 0;

		SetLocalAngles( angles );
	}
}

void CSprite::Activate( void )
{
	// Not yet active, so teleport to first target
	if( !m_activated && FClassnameIs( pev, "env_spritetrain" ))
	{
		m_activated = TRUE;

		m_pGoalEnt = GetNextTarget();
		if( m_pGoalEnt )
			UTIL_SetOrigin( this, m_pGoalEnt->GetLocalOrigin() );
	}
}

void CSprite::Precache( void )
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );

	// Reset attachment after save/restore
	if ( pev->aiment )
		SetAttachment( pev->aiment, pev->body );
	else
	{
		// Clear attachment
		pev->skin = 0;
		pev->body = 0;
	}
}

void CSprite::SpriteInit( const char *pSpriteName, const Vector &origin )
{
	pev->model = MAKE_STRING(pSpriteName);
	SetLocalOrigin( origin );
	Spawn();
}

CSprite *CSprite::SpriteCreate( const char *pSpriteName, const Vector &origin, BOOL animate )
{
	CSprite *pSprite = GetClassPtr( (CSprite *)NULL );
	pSprite->SpriteInit( pSpriteName, origin );
	pSprite->pev->classname = MAKE_STRING("env_sprite");
	pSprite->pev->solid = SOLID_NOT;
	pSprite->pev->movetype = MOVETYPE_NOCLIP;
	if ( animate )
		pSprite->TurnOn();

	return pSprite;
}

void CSprite::AnimateThink( void )
{
	Animate( pev->framerate * (gpGlobals->time - m_lastTime) );

	pev->nextthink = gpGlobals->time + 0.1;
	m_lastTime = gpGlobals->time;
}

void CSprite::AnimateUntilDead( void )
{
	if ( gpGlobals->time > pev->dmgtime )
	{
		UTIL_Remove(this);
	}
	else
	{
		AnimateThink();
		pev->nextthink = gpGlobals->time;
	}
}

void CSprite::Expand( float scaleSpeed, float fadeSpeed )
{
	pev->speed = scaleSpeed;
	pev->health = fadeSpeed;
	SetThink( &CSprite::ExpandThink );

	pev->nextthink	= gpGlobals->time;
	m_lastTime	= gpGlobals->time;
}


void CSprite::ExpandThink( void )
{
	float frametime = gpGlobals->time - m_lastTime;
	pev->scale += pev->speed * frametime;
	pev->renderamt -= pev->health * frametime;
	if ( pev->renderamt <= 0 )
	{
		pev->renderamt = 0;
		UTIL_Remove( this );
	}
	else
	{
		pev->nextthink		= gpGlobals->time + 0.1;
		m_lastTime			= gpGlobals->time;
	}
}


void CSprite::Animate( float frames )
{ 
	pev->frame += frames;
	if ( pev->frame > m_maxFrame )
	{
		if( pev->spawnflags & SF_SPRITE_ONCE && !FClassnameIs( pev, "env_spritetrain" ))
		{
			TurnOff();
		}
		else
		{
			if ( m_maxFrame > 0 )
				pev->frame = fmod( pev->frame, m_maxFrame );
		}
	}
}

void CSprite::UpdateTrainPath( void )
{
	if( !m_pGoalEnt ) return;

	Vector delta = m_pGoalEnt->GetLocalOrigin() - GetLocalOrigin();
	pev->frags = delta.Length();
	pev->movedir = delta.Normalize();
}

void CSprite::SpriteMove( void )
{
	// Not moving on a path, return
	if( !m_pGoalEnt )
	{
		TurnOff();
		return;
	}

	if( m_lastTime < gpGlobals->time )
	{
		Animate( pev->framerate * (gpGlobals->time - m_lastTime) );
		m_lastTime = gpGlobals->time + 0.1f;
	}

	if( pev->dmgtime > gpGlobals->time )
	{
		SetNextThink( 0 );
		return; // wait for path_corner delay
	}

	// Subtract movement from the previous frame
	pev->frags -= pev->speed * gpGlobals->frametime;

	// Have we moved enough to reach the target?
	if( pev->frags <= 0 )
	{
teleport_sprite:
		if( m_pGoalEnt->pev->speed != 0 )
			pev->speed = m_pGoalEnt->pev->speed;

		// Fire the passtarget if there is one
		if ( m_pGoalEnt->pev->message )
		{
			UTIL_FireTargets( STRING( m_pGoalEnt->pev->message ), this, this, USE_TOGGLE, 0 );
			if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_FIREONCE ) )
				m_pGoalEnt->pev->message = NULL_STRING;
		}

		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_WAITFORTRIG ) )
		{
			m_pGoalEnt = m_pGoalEnt->GetNextTarget();
			UpdateTrainPath();

			SetThink( &CSprite::AnimateThink );
			m_lastTime = gpGlobals->time;
			SetLocalVelocity( g_vecZero );
			SetNextThink( 0 );
			return;
		}

		float flDelay = m_pGoalEnt->GetDelay();

		if( flDelay != 0.0f )
		{
			m_pGoalEnt = m_pGoalEnt->GetNextTarget();
			UpdateTrainPath();

			SetLocalVelocity( g_vecZero );
			pev->dmgtime = gpGlobals->time + flDelay;
			SetNextThink( 0 );
			return;
		}

		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_TELEPORT ) )
		{
			m_pGoalEnt = m_pGoalEnt->GetNextTarget();

			if( m_pGoalEnt )
				UTIL_SetOrigin( this, m_pGoalEnt->GetLocalOrigin( )); 

			goto teleport_sprite;
		}
		
		// Time to go to the next target
		m_pGoalEnt = m_pGoalEnt->GetNextTarget();

		// Set up next corner
		if ( !m_pGoalEnt )
		{
			SetLocalVelocity( g_vecZero );
		}
		else
		{
			UpdateTrainPath();
		}
	}

	SetLocalVelocity( pev->movedir * pev->speed );
	SetNextThink( 0 );
}

void CSprite::TurnOff( void )
{
	if( !FClassnameIs( pev, "env_spritetrain" ))
		SUB_UseTargets( this, USE_OFF, 0 ); //LRC

	SetLocalVelocity( g_vecZero );
	pev->effects |= EF_NODRAW;
	pev->nextthink = 0;
}

void CSprite::TurnOn( void )
{
	if( !FClassnameIs( pev, "env_spritetrain" ))
		SUB_UseTargets( this, USE_ON, 0 ); //LRC

	pev->effects &= ~EF_NODRAW;

	if( FClassnameIs( pev, "env_spritetrain" ))
	{
		SetThink( &CSprite::SpriteMove );
		SetNextThink( 0 );
	}
	else if ( (pev->framerate && m_maxFrame > 1.0) || (pev->spawnflags & SF_SPRITE_ONCE) )
	{
		SetThink( &CSprite::AnimateThink );
		pev->nextthink = gpGlobals->time;
		m_lastTime = gpGlobals->time;
	}
	pev->frame = 0;
}


void CSprite::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int on;

	if( FClassnameIs( pev, "env_spritetrain" ))
		on = (GetLocalVelocity() != g_vecZero);
	else
		on = !(pev->effects & EF_NODRAW);

	if ( ShouldToggle( useType, on ) )
	{
		if ( on )
		{
			TurnOff();
		}
		else
		{
			TurnOn();
		}
	}
}
