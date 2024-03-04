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
#include "beam.h"

class CLightning : public CBeam
{
	DECLARE_CLASS( CLightning, CBeam );
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Activate( void );

	void	StrikeThink( void );
	void	DamageThink( void );
	void	RandomArea( void );
	void	RandomPoint( const Vector &vecSrc );
	void	Zap( const Vector &vecSrc, const Vector &vecDest );
	void	StrikeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	inline BOOL ServerSide( void )
	{
		if ( m_life == 0 && !( pev->spawnflags & SF_BEAM_RING ))
			return TRUE;
		return FALSE;
	}

	DECLARE_DATADESC();

	void	BeamUpdateVars( void );

	int	m_active;
	int	m_iszStartEntity;
	int	m_iszEndEntity;
	float	m_life;
	int	m_boltWidth;
	int	m_noiseAmplitude;
	int	m_speed;
	float	m_restrike;
	int	m_spriteTexture;
	int	m_iszSpriteName;
	int	m_frameStart;

	float	m_radius;
};
