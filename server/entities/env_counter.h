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

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"

// =================== ENV_COUNTER ==============================================

class CDecLED : public CBaseDelay
{
	DECLARE_CLASS( CDecLED, CBaseDelay );
public:
	void	Spawn( void );
	void	Precache( void );
	void	CheckState( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	float 	Frames( void ) { return MODEL_FRAMES( pev->modelindex ) - 1; }
	float	curframe( void ) { return Q_rint( pev->frame ); }
private:
	bool	keyframeset;
	float	flashtime;
	bool	recursive; // to prevent infinite loop with USE_RESET
};
