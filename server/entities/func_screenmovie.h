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

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "client.h"
#include "player.h"
#include "gamerules.h"

#define SF_SCREENMOVIE_START_ON	BIT( 0 )
#define SF_SCREENMOVIE_PASSABLE	BIT( 1 )
#define SF_SCREENMOVIE_LOOPED		BIT( 2 )
#define SF_SCREENMOVIE_MONOCRHOME	BIT( 3 )	// black & white
#define SF_SCREENMOVIE_SOUND		BIT( 4 )	// allow sound

class CFuncScreenMovie : public CBaseDelay
{
	DECLARE_CLASS( CFuncScreenMovie, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void CineThink( void );

	DECLARE_DATADESC();
};