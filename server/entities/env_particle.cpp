/*
env_particle.cpp - entity that represents particle system
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "env_particle.h"

#define SF_PARTICLE_START_ON		BIT( 0 )

LINK_ENTITY_TO_CLASS( env_particle, CBaseParticle );

void CBaseParticle::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "aurora" ))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "attachment" ))
	{
		pev->impulse = Q_atoi( pkvd->szValue );
		pev->impulse = bound( 1, pev->impulse, 4 );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CBaseParticle::StartMessage( CBasePlayer *pPlayer )
{
	UTIL_CreateAuroraSystem(pPlayer, this, STRING(pev->message), pev->impulse);
}

void CBaseParticle::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->renderamt = 128;
	pev->rendermode = kRenderTransTexture;
	pev->body = (pev->spawnflags & SF_PARTICLE_START_ON) != 0; // 'body' determines whether the effect is active or not

	SET_MODEL( edict(), "sprites/null.spr" );
	UTIL_SetSize( pev, Vector( -8, -8, -8 ), Vector( 8, 8, 8 ));
	RelinkEntity();
}

void CBaseParticle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_TOGGLE )
	{
		if( GetState() == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}

	if( useType == USE_ON )
	{
		pev->body = 1;
	}
	else if( useType == USE_OFF )
	{
		pev->body = 0;
	}
}
