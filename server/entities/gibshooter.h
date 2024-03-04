/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"

class CGibShooter : public CBaseDelay
{
	DECLARE_CLASS( CGibShooter, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void ShootThink( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)) | FCAP_SET_MOVEDIR; };

	virtual CBaseEntity *CreateGib( void );

	DECLARE_DATADESC();

	int	m_iGibs;
	int	m_iGibCapacity;
	int	m_iGibMaterial;
	int	m_iGibModelIndex;
	float	m_flGibVelocity;
	float	m_flVariance;
	float	m_flGibLife;
	string_t	m_iszTargetname;
	string_t	m_iszSpawnTarget;
	int	m_iBloodColor;
};
