/*
env_counter.h - range-aware counter that can assume special “key” states
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

#include "env_counter.h"

LINK_ENTITY_TO_CLASS( env_counter, CDecLED );

void CDecLED :: Precache( void )
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("sprites/decimal.spr");
}

void CDecLED :: Spawn( void )
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "sprites/decimal.spr");
         
	// Smart Field System ©
    if( !pev->renderamt ) pev->renderamt = 200; // light transparency
	if( !pev->rendermode ) pev->rendermode = kRenderTransAdd;
	if( !pev->frags ) pev->frags = Frames();
	if( !keyframeset ) pev->impulse = -1;

	CheckState();

	SetBits( m_iFlags, MF_POINTENTITY );
	pev->effects |= EF_NOINTERP;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	Vector angles = GetLocalAngles();

	angles.x = -angles.x;
	angles.y = 180 - angles.y;
	angles.y = -angles.y;

	SetLocalAngles( angles );	
	RelinkEntity( FALSE );

	// allow to switch frames for studio models
	pev->framerate = -1.0f;
	pev->button = 1;
}

void CDecLED :: CheckState( void )
{
	switch( (int)pev->dmg )
	{
	case 1:
		if( pev->impulse >= curframe() )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		break;
	case 2:
		if( pev->impulse <= curframe() )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		break;
	default:
		if( pev->impulse == curframe() )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		break;
	}
}

void CDecLED :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "maxframe" ))
	{
		pev->frags = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "keyframe" ))
	{
		pev->impulse = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
		keyframeset = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "type" ))
	{
		pev->dmg = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CDecLED :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_ON || useType == USE_TOGGLE )
	{
		if( pev->frame >= pev->frags ) 
		{
			UTIL_FireTargets( pev->target, pActivator, this, useType, value );
			pev->frame = 0;
		}
		else pev->frame++;
		
		pev->button = 1;
		ClearBits( pev->effects, EF_NODRAW );
		flashtime = gpGlobals->time + 0.5f;
	}
	else if( useType == USE_OFF )
	{
		if( pev->frame <= 0 ) 
		{
         	UTIL_FireTargets( pev->target, pActivator, this, useType, value );
			pev->frame = pev->frags;
		}
		else pev->frame--;

		pev->button = 0;
		ClearBits( pev->effects, EF_NODRAW );
		flashtime = gpGlobals->time + 0.5f;
	}
	else if( useType == USE_SET ) // set custom frame
	{ 
		if( value != 0.0f )
		{
			if( value == -1 ) value = 0; // reset frame
			pev->frame = fmod( value, pev->frags + 1 );
            float next = value / ( pev->frags + 1 );
            if( next ) UTIL_FireTargets( pev->target, pActivator, this, useType, next );
			ClearBits( pev->effects, EF_NODRAW );
			flashtime = gpGlobals->time + 0.5f;
			pev->button = 1;
		}
		else if( gpGlobals->time > flashtime )
		{
			if( pev->button )
			{
				SetBits( pev->effects, EF_NODRAW );
				pev->button = 0;
			}
			else
			{
				ClearBits( pev->effects, EF_NODRAW );
				pev->button = 1;
			}
		}
	}
	else if( useType == USE_RESET ) // immediately reset
	{ 
		pev->frame = 0;
		pev->button = 1;
		pev->effects &= ~EF_NODRAW;

		if( !recursive )
		{
			recursive = TRUE;
			UTIL_FireTargets( pev->target, pActivator, this, useType, 0 );
			recursive = FALSE;
		}
		else ALERT( at_error, "%s has infinite loop in chain\n", STRING( pev->classname ));
	}

	// check state after changing
	CheckState();
}
