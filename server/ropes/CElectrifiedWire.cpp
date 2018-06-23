/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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

#include "CRopeSegment.h"

#include "CElectrifiedWire.h"

BEGIN_DATADESC( CElectrifiedWire )
	DEFINE_FIELD( m_bIsActive, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_iTipSparkFrequency, FIELD_INTEGER ),
	DEFINE_FIELD( m_iBodySparkFrequency, FIELD_INTEGER ),
	DEFINE_FIELD( m_iLightningFrequency, FIELD_INTEGER ),
	
	DEFINE_FIELD( m_iXJoltForce, FIELD_INTEGER ),
	DEFINE_FIELD( m_iYJoltForce, FIELD_INTEGER ),
	DEFINE_FIELD( m_iZJoltForce, FIELD_INTEGER ),
	
	DEFINE_FIELD( m_iNumUninsulatedSegments, FIELD_INTEGER ),
	DEFINE_ARRAY( m_iUninsulatedSegments, FIELD_INTEGER, MAX_SEGMENTS ),
	
	//DEFINE_FIELD( m_iLightningSprite, FIELD_INTEGER ), //Not restored, reset in Precache. - Solokiller
	
	DEFINE_FIELD( m_flLastSparkTime, FIELD_TIME ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_electrified_wire, CElectrifiedWire );

CElectrifiedWire::CElectrifiedWire()
	: m_bIsActive( true )
	, m_iTipSparkFrequency( 3 )
	, m_iBodySparkFrequency( 100 )
	, m_iLightningFrequency( 150 )
	, m_iXJoltForce( 0 )
	, m_iYJoltForce( 0 )
	, m_iZJoltForce( 0 )
	, m_iNumUninsulatedSegments( 0 )
{
}

void CElectrifiedWire::KeyValue( KeyValueData* pkvd )
{
	if( FStrEq( pkvd->szKeyName, "sparkfrequency" ))
	{
		m_iTipSparkFrequency = atoi( pkvd->szValue );

		pkvd->fHandled = true;
	}
	else if( FStrEq( pkvd->szKeyName, "bodysparkfrequency" ))
	{
		m_iBodySparkFrequency = atoi( pkvd->szValue );

		pkvd->fHandled = true;
	}
	else if( FStrEq( pkvd->szKeyName, "lightningfrequency" ))
	{
		m_iLightningFrequency = atoi( pkvd->szValue );

		pkvd->fHandled = true;
	}
	else if( FStrEq( pkvd->szKeyName, "xforce" ))
	{
		m_iXJoltForce = atoi( pkvd->szValue );

		pkvd->fHandled = true;
	}
	else if( FStrEq( pkvd->szKeyName, "yforce" ))
	{
		m_iYJoltForce = atoi( pkvd->szValue );

		pkvd->fHandled = true;
	}
	else if( FStrEq( pkvd->szKeyName, "zforce" ))
	{
		m_iZJoltForce = atoi( pkvd->szValue );

		pkvd->fHandled = true;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CElectrifiedWire :: Precache( void )
{
	BaseClass::Precache();

	m_iLightningSprite = PRECACHE_MODEL( "sprites/lgtning.spr" );
}

void CElectrifiedWire :: Spawn( void )
{
	if( !pev->dmg )
		pev->dmg = 1;

	BaseClass::Spawn();

	m_iNumUninsulatedSegments = 0;
	m_bIsActive = true;

	if( m_iBodySparkFrequency > 0 )
	{
		for( int i = 0; i < GetNumSegments(); i++ )
		{
			if( IsValidSegmentIndex( i ))
			{
				m_iUninsulatedSegments[m_iNumUninsulatedSegments++] = i;
			}
		}
	}

	if( m_iNumUninsulatedSegments > 0 )
	{
		for( int i = 0; i < m_iNumUninsulatedSegments; i++ )
		{
			GetSegments()[i]->SetCauseDamageOnTouch( m_bIsActive );
		}
	}

	if( m_iTipSparkFrequency > 0 )
	{
		GetSegments()[GetNumSegments() - 1]->SetCauseDamageOnTouch( m_bIsActive );
	}

	m_flLastSparkTime = gpGlobals->time;

	SetSoundAllowed( false );
}

void CElectrifiedWire :: Think( void )
{
	if( gpGlobals->time - m_flLastSparkTime > 0.1 )
	{
		m_flLastSparkTime = gpGlobals->time;

		if( m_iNumUninsulatedSegments > 0 )
		{
			for( int i = 0; i < m_iNumUninsulatedSegments; i++ )
			{
				if( ShouldDoEffect( m_iBodySparkFrequency ))
				{
					DoSpark( m_iUninsulatedSegments[i], false );
				}
			}
		}

		if( ShouldDoEffect( m_iTipSparkFrequency ) )
		{
			DoSpark( GetNumSegments() - 1, true );
		}

		if( ShouldDoEffect( m_iLightningFrequency ) )
			DoLightning();
	}

	BaseClass::Think();
}

void CElectrifiedWire::Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float flValue )
{
	m_bIsActive = !m_bIsActive;

	if( m_iNumUninsulatedSegments > 0 )
	{
		for( int i = 0; i < m_iNumUninsulatedSegments; i++ )
		{
			GetSegments()[m_iUninsulatedSegments[i]]->SetCauseDamageOnTouch( m_bIsActive );
		}
	}

	if( m_iTipSparkFrequency > 0 )
	{
		GetSegments()[GetNumSegments() - 1]->SetCauseDamageOnTouch( m_bIsActive );
	}
}

bool CElectrifiedWire::ShouldDoEffect( const int iFrequency )
{
	if( iFrequency <= 0 )
		return false;

	if( !IsActive() )
		return false;

	return RANDOM_LONG( 1, iFrequency ) == 1;
}

void CElectrifiedWire::DoSpark( const int iSegment, const bool bExertForce )
{
	const Vector vecOrigin = GetSegmentAttachmentPoint( iSegment );

	UTIL_Sparks( vecOrigin );

	if( bExertForce )
	{
		const Vector vecSparkForce(
			RANDOM_FLOAT( -m_iXJoltForce, m_iXJoltForce ),
			RANDOM_FLOAT( -m_iYJoltForce, m_iYJoltForce ),
			RANDOM_FLOAT( -m_iZJoltForce, m_iZJoltForce )
		);

		ApplyForceToSegment( vecSparkForce, iSegment );
	}
}

void CElectrifiedWire :: DoLightning( void )
{
	const int iSegment1 = RANDOM_LONG( 0, GetNumSegments() - 1 );

	int iSegment2, i;

	// Try to get a random segment.
	for( i = 0; i < 10; i++ )
	{
		iSegment2 = RANDOM_LONG( 0, GetNumSegments() - 1 );

		if( iSegment2 != iSegment1 )
			break;
	}

	if( i >= 10 ) return;

	CRopeSegment* pSegment1 = GetSegments()[iSegment1];
	CRopeSegment* pSegment2 = GetSegments()[iSegment2];

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

		if( IsSimulateBones( ))
		{
			WRITE_BYTE( TE_BEAMPOINTS);
			WRITE_COORD( pSegment1->GetAbsOrigin().x);
			WRITE_COORD( pSegment1->GetAbsOrigin().y);
			WRITE_COORD( pSegment1->GetAbsOrigin().z);
			WRITE_COORD( pSegment2->GetAbsOrigin().x);
			WRITE_COORD( pSegment2->GetAbsOrigin().y);
			WRITE_COORD( pSegment2->GetAbsOrigin().z);
		}
		else
		{
			WRITE_BYTE( TE_BEAMENTS );
			WRITE_SHORT( pSegment1->entindex() );
			WRITE_SHORT( pSegment2->entindex() );
		}
		WRITE_SHORT( m_iLightningSprite );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 1 );
		WRITE_BYTE( 10 );
		WRITE_BYTE( 80 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 255 );
	MESSAGE_END();
}
