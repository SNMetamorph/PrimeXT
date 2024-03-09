/*
env_customize.h - entity for changing a variety of parameters for NPCs
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

//***********************************************************
//
// EnvCustomize
//
// Changes various properties of an entity (some properties only apply to monsters.)
//

#define SF_CUSTOM_AFFECTDEAD	1
#define SF_CUSTOM_ONCE			2
#define SF_CUSTOM_DEBUG			4

#define CUSTOM_FLAG_NOCHANGE	0
#define CUSTOM_FLAG_ON			1
#define CUSTOM_FLAG_OFF			2
#define CUSTOM_FLAG_TOGGLE		3
#define CUSTOM_FLAG_USETYPE		4
#define CUSTOM_FLAG_INVUSETYPE	5

class CEnvCustomize : public CBaseEntity
{
	DECLARE_CLASS( CEnvCustomize, CBaseEntity );
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void Affect (CBaseEntity *pTarget, USE_TYPE useType);
	int GetActionFor( int iField, int iActive, USE_TYPE useType, char *szDebug );
	void SetBoneController (float fController, int cnum, CBaseEntity *pTarget);

	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();

	int	m_iszModel;
	int	m_iClass;
	int	m_iPlayerReact;
	int	m_iPrisoner;
	int	m_iMonsterClip;
	int	m_iVisible;
	int	m_iSolid;
	int	m_iProvoked;
	int	m_voicePitch;
	int	m_iBloodColor;
	float	m_fFramerate;
	float	m_fController0;
	float	m_fController1;
	float	m_fController2;
	float	m_fController3;
	int	m_iReflection;	// reflection style
};
