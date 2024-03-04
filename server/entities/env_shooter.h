/*
env_shooter.h - improved version of gibshooter with more features
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
#include "gibshooter.h"

class CEnvShooter : public CGibShooter
{
	DECLARE_CLASS( CEnvShooter, CGibShooter );

	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();

	virtual CBaseEntity *CreateGib( void );

	string_t m_iszTouch;
	string_t m_iszTouchOther;
	int m_iPhysics;
	float m_fFriction;
	Vector m_vecSize;
};
