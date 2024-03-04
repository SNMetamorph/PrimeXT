/*
env_sky.cpp - 3D skybox effect camera
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

#include "env_sky.h"

#define SF_ENVSKY_START_OFF	BIT( 0 )

LINK_ENTITY_TO_CLASS( env_sky, CEnvSky );

void CEnvSky :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "fov" ))
	{
		SetFovAngle(Q_atof( pkvd->szValue ));
		pkvd->fHandled = TRUE;
	}
	else CPointEntity::KeyValue( pkvd );
}

void CEnvSky::Spawn( void )
{
	if( !FBitSet( pev->spawnflags, SF_ENVSKY_START_OFF ))
		pev->effects |= (EF_MERGE_VISIBILITY|EF_SKYCAMERA);

	SET_MODEL( edict(), "sprites/null.spr" );
	SetBits( m_iFlags, MF_POINTENTITY );
	RelinkEntity( FALSE );
}

void CEnvSky::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int m_active = FBitSet( pev->effects, EF_SKYCAMERA );

	if ( !ShouldToggle( useType, m_active ))
		return;

	if ( m_active )
	{
		pev->effects &= ~(EF_MERGE_VISIBILITY|EF_SKYCAMERA);
	}
	else
	{
		pev->effects |= (EF_MERGE_VISIBILITY|EF_SKYCAMERA);
	}
}
