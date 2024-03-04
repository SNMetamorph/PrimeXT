/*
env_rain.h - rain VFX implementation from Paranoia 1
Copyright (C) 2004 BUzer

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "env_rain.h"

LINK_ENTITY_TO_CLASS( env_rain, CEnvRain );

void CEnvRain::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_flDistance" ))
	{
		pev->frags = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iMode" ))
	{
		pev->impulse = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CPointEntity::KeyValue( pkvd );
	}
}
