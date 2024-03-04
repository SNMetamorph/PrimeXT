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

#include "gibshooter.h"
#include "monsters.h"

#define SF_GIBSHOOTER_REPEATABLE	1 // allows a gibshooter to be refired

BEGIN_DATADESC( CGibShooter )
	DEFINE_KEYFIELD( m_iGibs, FIELD_INTEGER, "m_iGibs" ),
	DEFINE_KEYFIELD( m_iGibCapacity, FIELD_INTEGER, "m_iGibs" ),
	DEFINE_FIELD( m_iGibMaterial, FIELD_INTEGER ),
	DEFINE_FIELD( m_iGibModelIndex, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_flGibVelocity, FIELD_FLOAT, "m_flVelocity" ),
	DEFINE_KEYFIELD( m_flVariance, FIELD_FLOAT, "m_flVariance" ),
	DEFINE_KEYFIELD( m_flGibLife, FIELD_FLOAT, "m_flGibLife" ),
	DEFINE_KEYFIELD( m_iszTargetname, FIELD_STRING, "m_iszTargetName" ),
	DEFINE_KEYFIELD( m_iszSpawnTarget, FIELD_STRING, "m_iszSpawnTarget" ),
	DEFINE_KEYFIELD( m_iBloodColor, FIELD_INTEGER, "m_iBloodColor" ),
	DEFINE_FUNCTION( ShootThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( gibshooter, CGibShooter );

void CGibShooter :: Precache ( void )
{
	if ( g_Language == LANGUAGE_GERMAN )
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/germanygibs.mdl");
	}
	else if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/agibs.mdl");
	}
	else
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/hgibs.mdl");
	}
}

void CGibShooter::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iGibs" ))
	{
		m_iGibs = m_iGibCapacity = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flVelocity" ))
	{
		m_flGibVelocity = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flVariance" ))
	{
		m_flVariance = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flGibLife" ))
	{
		m_flGibLife = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTargetName"))
	{
		m_iszTargetname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSpawnTarget"))
	{
		m_iszSpawnTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iBloodColor"))
	{
		m_iBloodColor = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseDelay::KeyValue( pkvd );
	}
}

void CGibShooter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	SetThink( &CGibShooter::ShootThink );
	SetNextThink( 0 );
}

void CGibShooter::Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;

	// is mapper forgot set angles?
	if( pev->movedir == g_vecZero )
		pev->movedir = Vector( 1, 0, 0 );

	if ( m_flDelay == 0 )
	{
		m_flDelay = 0.1;
	}

	if ( m_flGibLife == 0 )
	{
		m_flGibLife = 25;
	}

	pev->body = MODEL_FRAMES( m_iGibModelIndex );
}


CBaseEntity *CGibShooter :: CreateGib( void )
{
	if ( CVAR_GET_FLOAT("violence_hgibs") == 0 )
		return NULL;

	CGib *pGib = GetClassPtr( (CGib *)NULL );

	if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		pGib->Spawn( "models/agibs.mdl" );
		pGib->m_bloodColor = BLOOD_COLOR_YELLOW;
	}
	else if (m_iBloodColor)
	{
		pGib->Spawn( "models/hgibs.mdl" );
		pGib->m_bloodColor = m_iBloodColor;
	}
	else
	{
		pGib->Spawn( "models/hgibs.mdl" );
		pGib->m_bloodColor = BLOOD_COLOR_RED;
	}

	if ( pev->body <= 1 )
	{
		ALERT ( at_aiconsole, "GibShooter Body is <= 1!\n" );
	}

	pGib->pev->body = RANDOM_LONG ( 1, pev->body - 1 );// avoid throwing random amounts of the 0th gib. (skull).

	return pGib;
}


void CGibShooter :: ShootThink ( void )
{
	SetNextThink( m_flDelay );

	Vector vecShootDir = EntityToWorldTransform().VectorRotate( pev->movedir );
	UTIL_MakeVectors( GetAbsAngles( )); // g-cont. was missed into original game

	vecShootDir = vecShootDir + gpGlobals->v_right * RANDOM_FLOAT( -1, 1) * m_flVariance;
	vecShootDir = vecShootDir + gpGlobals->v_forward * RANDOM_FLOAT( -1, 1) * m_flVariance;
	vecShootDir = vecShootDir + gpGlobals->v_up * RANDOM_FLOAT( -1, 1) * m_flVariance;

	vecShootDir = vecShootDir.Normalize();
	CBaseEntity *pShot = CreateGib();
	
	if( pShot )
	{
		pShot->SetAbsOrigin( GetAbsOrigin( ));
		pShot->SetAbsVelocity( vecShootDir * m_flGibVelocity );

		if( pShot->m_iActorType == ACTOR_DYNAMIC )
			WorldPhysic->AddForce( pShot, vecShootDir * m_flGibVelocity * ( 1.0f / gpGlobals->frametime ) * 0.1f );

		// custom particles already set this
		if( FClassnameIs( pShot->pev, "gib" ))
		{
			CGib *pGib = (CGib *)pShot;

			Vector vecAvelocity = pGib->GetLocalAvelocity();	
			vecAvelocity.x = RANDOM_FLOAT( 100, 200 );
			vecAvelocity.y = RANDOM_FLOAT( 100, 300 );
			pGib->SetLocalAvelocity( vecAvelocity );

			float thinkTime = pGib->pev->nextthink - gpGlobals->time;
			pGib->m_lifeTime = (m_flGibLife * RANDOM_FLOAT( 0.95, 1.05 )); // +/- 5%

			if( pGib->m_lifeTime < thinkTime )
			{
				pGib->SetNextThink( pGib->m_lifeTime );
				pGib->m_lifeTime = 0;
			}
                    }

		pShot->pev->targetname = m_iszTargetname;

		if( m_iszSpawnTarget )
			UTIL_FireTargets( m_iszSpawnTarget, pShot, this, USE_TOGGLE, 0 );		
	}

	if( --m_iGibs <= 0 )
	{
		if( FBitSet( pev->spawnflags, SF_GIBSHOOTER_REPEATABLE ))
		{
			m_iGibs = m_iGibCapacity;
			SetThink( NULL );
			SetNextThink( 0 );
		}
		else
		{
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( 0 );
		}
	}
}
