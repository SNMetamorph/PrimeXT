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

#include "func_monitor.h"

#define SF_PORTAL_START_OFF		BIT( 0 )
#define SF_PORTAL_CLIENTONLYFIRE	BIT( 1 )

class CFuncPortal : public CFuncMonitor
{
	DECLARE_CLASS( CFuncPortal, CFuncMonitor );
public:
	void Spawn( void );
	void Touch ( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void PortalSleep( float seconds ) { m_flDisableTime = gpGlobals->time + seconds; }
	virtual STATE GetState( void ) { return m_iState; };
	virtual BOOL IsPortal( void ) { return TRUE; }
	void StartMessage( CBasePlayer *pPlayer ) {};
	void Activate( void );
};

class CInfoPortalDest : public CPointEntity
{
	DECLARE_CLASS( CInfoPortalDest, CPointEntity );
public:
	void Spawn( void );
};