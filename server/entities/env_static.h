/*
env_static.cpp - entity that represents static studiomodel geometry
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

// =================== ENV_STATIC ==============================================
#define SF_STATIC_SOLID			BIT(0)
#define SF_STATIC_DROPTOFLOOR	BIT(1)
#define SF_STATIC_NOSHADOW		BIT(2)	// reserved for pxrad, disables shadows on lightmap
#define SF_STATIC_NOVLIGHT		BIT(4)	// reserved for pxrad, disables vertex lighting
#define SF_STATIC_NODYNSHADOWS	BIT(5)

class CEnvStatic : public CBaseEntity
{
	DECLARE_CLASS( CEnvStatic, CBaseEntity );
public:
	void Spawn( void );
	void Precache( void );
	void AutoSetSize( void );
	virtual int ObjectCaps( void ) { return FCAP_IGNORE_PARENT; }
	void SetObjectCollisionBox( void );
	void KeyValue( KeyValueData *pkvd );

	int m_iVertexLightCache;
	int m_iSurfaceLightCache;
};
