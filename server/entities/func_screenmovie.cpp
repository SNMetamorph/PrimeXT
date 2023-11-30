/*
func_monitor.cpp - realtime monitors and portals
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "func_screenmovie.h"

LINK_ENTITY_TO_CLASS( func_screenmovie, CFuncScreenMovie );

BEGIN_DATADESC( CFuncScreenMovie )
	DEFINE_FUNCTION( CineThink ),
END_DATADESC()

void CFuncScreenMovie::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "movie" ))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue( pkvd );
}

void CFuncScreenMovie :: Precache( void )
{
	// store movie name as event index
	pev->sequence = UTIL_PrecacheMovie( pev->message, FBitSet( pev->spawnflags, SF_SCREENMOVIE_SOUND ));
}

void CFuncScreenMovie :: Spawn( void )
{
	Precache();

	pev->movetype = MOVETYPE_PUSH;

	if( FBitSet( pev->spawnflags, SF_SCREENMOVIE_PASSABLE ))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;

	SET_MODEL( edict(), GetModel() );
	pev->effects |= EF_SCREENMOVIE;
	m_iState = STATE_OFF;

	if( FBitSet( pev->spawnflags, SF_SCREENMOVIE_LOOPED ))
		pev->iuser1 |= CF_LOOPED_MOVIE;

	if( FBitSet( pev->spawnflags, SF_SCREENMOVIE_MONOCRHOME ))
		pev->iuser1 |= CF_MONOCHROME;

	if( FBitSet( pev->spawnflags, SF_SCREENMOVIE_SOUND ))
		pev->iuser1 |= CF_MOVIE_SOUND;

	// enable monitor
	if( FBitSet( pev->spawnflags, SF_SCREENMOVIE_START_ON ))
	{
		SetThink( &CBaseEntity::SUB_CallUseToggle );
		SetNextThink( 0.1 );
	}
}

void CFuncScreenMovie :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( IsLockedByMaster( pActivator ))
		return;

	if( !pev->sequence )
	{
		ALERT( at_error, "%s with name %s can't loaded video %s\n", GetClassname(), GetTargetname(), STRING( pev->message ));
		return;
	}

	SetThink( &CFuncScreenMovie::CineThink );

	if( ShouldToggle( useType ))
	{
		if( useType == USE_SET )
		{
			if( value )
			{
				pev->fuser2 = value;
			}
			else if( pev->body )
			{
				if( pev->nextthink != -1 )
					DontThink();
				else SetNextThink( CIN_FRAMETIME );
			}
			return;
		}
		else if( useType == USE_RESET )
		{
			pev->fuser2 = 0;
			return;
		}

		pev->body = !pev->body;
		m_iState = (pev->body) ? STATE_ON : STATE_OFF;

		if( pev->body )
			SetNextThink( CIN_FRAMETIME );
		else DontThink();
	}
}

void CFuncScreenMovie :: CineThink( void )
{
	m_iState = STATE_ON;

	// update as 30 frames per second
	pev->fuser2 += CIN_FRAMETIME;
	SetNextThink( CIN_FRAMETIME );
}
