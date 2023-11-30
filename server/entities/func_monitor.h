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

#define SF_MONITOR_START_ON		BIT( 0 )
#define SF_MONITOR_PASSABLE		BIT( 1 )
#define SF_MONITOR_USEABLE		BIT( 2 )
#define SF_MONITOR_HIDEHUD		BIT( 3 )
#define SF_MONITOR_MONOCRHOME		BIT( 4 )	// black & white

class CFuncMonitor : public CBaseDelay
{
	DECLARE_CLASS( CFuncMonitor, CBaseDelay );
public:
	void Spawn( void );
	void ChangeCamera( string_t newcamera );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual BOOL IsFuncScreen( void ) { return TRUE; }
	bool AllowToFindClientInPVS( CBaseEntity *pTarget );
	void StartMessage( CBasePlayer *pPlayer );
	virtual STATE GetState( void ) { return pev->body ? STATE_ON:STATE_OFF; };
	virtual int ObjectCaps( void );
	BOOL OnControls( CBaseEntity *pTest );
	void Activate( void );
	void VisThink( void );
	void SetCameraVisibility( bool fEnable );

	DECLARE_DATADESC();

	CBasePlayer *m_pController;	// player pointer
	Vector m_vecControllerUsePos; // where was the player standing when he used me?
	float m_flDisableTime;	// portal disable time
};