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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "beam.h"
#include "env_beam.h"

// UNDONE: Jay -- This is only a test
#if _DEBUG
class CTripBeam : public CLightning
{
	DECLARE_CLASS( CTripBeam, CLightning );
public:
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( trip_beam, CTripBeam );

void CTripBeam::Spawn( void )
{
	BaseClass::Spawn();
	SetTouch( &CBeam::TriggerTouch );
	pev->solid = SOLID_TRIGGER;
	RelinkBeam();
}
#endif

class CTestEffect : public CBaseDelay
{
	DECLARE_CLASS( CTestEffect, CBaseDelay );
public:
	void	Spawn( void );
	void	Precache( void );

	void	TestThink( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();

	int	m_iLoop;
	int	m_iBeam;
	CBeam	*m_pBeam[24];
	float	m_flBeamTime[24];
	float	m_flStartTime;
};

LINK_ENTITY_TO_CLASS( test_effect, CTestEffect );

BEGIN_DATADESC( CTestEffect )
	DEFINE_FUNCTION( TestThink ),
END_DATADESC()

void CTestEffect::Spawn( void )
{
	Precache( );
}

void CTestEffect::Precache( void )
{
	PRECACHE_MODEL( "sprites/lgtning.spr" );
}

void CTestEffect::TestThink( void )
{
	int i;
	float t = (gpGlobals->time - m_flStartTime);

	if (m_iBeam < 24)
	{
		CBeam *pbeam = CBeam::BeamCreate( "sprites/lgtning.spr", 100 );

		TraceResult		tr;

		Vector vecSrc = GetAbsOrigin();
		Vector vecDir = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir = vecDir.Normalize();
		UTIL_TraceLine( vecSrc, vecSrc + vecDir * 128, ignore_monsters, ENT(pev), &tr);

		pbeam->PointsInit( vecSrc, tr.vecEndPos );
		// pbeam->SetColor( 80, 100, 255 );
		pbeam->SetColor( 255, 180, 100 );
		pbeam->SetWidth( 100 );
		pbeam->SetScrollRate( 12 );
		
		m_flBeamTime[m_iBeam] = gpGlobals->time;
		m_pBeam[m_iBeam] = pbeam;
		m_iBeam++;
	}

	if (t < 3.0)
	{
		for (i = 0; i < m_iBeam; i++)
		{
			t = (gpGlobals->time - m_flBeamTime[i]) / ( 3 + m_flStartTime - m_flBeamTime[i]);
			m_pBeam[i]->SetBrightness( 255 * t );
			// m_pBeam[i]->SetScrollRate( 20 * t );
		}
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		for (i = 0; i < m_iBeam; i++)
		{
			UTIL_Remove( m_pBeam[i] );
		}
		m_flStartTime = gpGlobals->time;
		m_iBeam = 0;
		// pev->nextthink = gpGlobals->time;
		SetThink( NULL );
	}
}

void CTestEffect::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CTestEffect::TestThink );
	pev->nextthink = gpGlobals->time + 0.1;
	m_flStartTime = gpGlobals->time;
}
