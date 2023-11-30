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
/*

===== explode.cpp ========================================================

  Explosion-related code

*/
#include "env_explosion.h"

BEGIN_DATADESC( CEnvExplosion )
	DEFINE_FIELD( m_iMagnitude, FIELD_INTEGER ),
	DEFINE_FIELD( m_spriteScale, FIELD_INTEGER ),
	DEFINE_FUNCTION( Smoke ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_explosion, CEnvExplosion );

void CEnvExplosion::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "iMagnitude" ))
	{
		m_iMagnitude = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CEnvExplosion::Spawn( void )
{ 
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;

	float flSpriteScale;
	flSpriteScale = ( m_iMagnitude - 50) * 0.6;
	
	if ( flSpriteScale < 10 )
	{
		flSpriteScale = 10;
	}

	m_spriteScale = (int)flSpriteScale;
}

void CEnvExplosion::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	TraceResult tr;

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	Vector		vecSpot;// trace starts here!

	vecSpot = GetAbsOrigin() + Vector( 0, 0, 8 );
	
	UTIL_TraceLine( vecSpot, vecSpot + Vector ( 0, 0, -40 ),  ignore_monsters, edict(), &tr );
	
	// Pull out of the wall a bit
	if ( tr.flFraction != 1.0 )
	{
		SetAbsOrigin( tr.vecEndPos + (tr.vecPlaneNormal * (m_iMagnitude - 24) * 0.6f ));
	}

	// draw decal
	if (!(pev->spawnflags & SF_ENVEXPLOSION_NODECAL))
	{
		if (RANDOM_FLOAT(0, 1) < 0.5)
		{
			UTIL_TraceCustomDecal(&tr, "scorch1", RANDOM_FLOAT(0.0f, 360.0f));
		}
		else
		{
			UTIL_TraceCustomDecal(&tr, "scorch2", RANDOM_FLOAT(0.0f, 360.0f));
		}
	}

	Vector absOrigin = GetAbsOrigin();

	// draw fireball
	if ( !( pev->spawnflags & SF_ENVEXPLOSION_NOFIREBALL ) )
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, absOrigin );
			WRITE_BYTE( TE_EXPLOSION);
			WRITE_COORD( absOrigin.x );
			WRITE_COORD( absOrigin.y );
			WRITE_COORD( absOrigin.z );
			WRITE_SHORT( g_sModelIndexFireball );
			WRITE_BYTE( (BYTE)m_spriteScale ); // scale * 10
			WRITE_BYTE( 15  ); // framerate
			WRITE_BYTE( TE_EXPLFLAG_NONE );
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, absOrigin );
			WRITE_BYTE( TE_EXPLOSION);
			WRITE_COORD( absOrigin.x );
			WRITE_COORD( absOrigin.y );
			WRITE_COORD( absOrigin.z );
			WRITE_SHORT( g_sModelIndexFireball );
			WRITE_BYTE( 0 ); // no sprite
			WRITE_BYTE( 15  ); // framerate
			WRITE_BYTE( TE_EXPLFLAG_NONE );
		MESSAGE_END();
	}

	// do damage
	if ( !( pev->spawnflags & SF_ENVEXPLOSION_NODAMAGE ) )
	{
		RadiusDamage ( pev, pev, m_iMagnitude, CLASS_NONE, DMG_BLAST );
	}

	SetThink( &CEnvExplosion::Smoke );
	pev->nextthink = gpGlobals->time + 0.3;

	// draw sparks
	if ( !( pev->spawnflags & SF_ENVEXPLOSION_NOSPARKS ) )
	{
		int sparkCount = RANDOM_LONG(0,3);

		for ( int i = 0; i < sparkCount; i++ )
		{
			Create( "spark_shower", absOrigin, tr.vecPlaneNormal, NULL );
		}
	}
}

void CEnvExplosion::Smoke( void )
{
	Vector absOrigin = GetAbsOrigin();

	if ( !( pev->spawnflags & SF_ENVEXPLOSION_NOSMOKE ) )
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, absOrigin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( absOrigin.x );
			WRITE_COORD( absOrigin.y );
			WRITE_COORD( absOrigin.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( (BYTE)m_spriteScale ); // scale * 10
			WRITE_BYTE( 12  ); // framerate
		MESSAGE_END();
	}
	
	if ( !(pev->spawnflags & SF_ENVEXPLOSION_REPEATABLE) )
	{
		UTIL_Remove( this );
	}
}


// HACKHACK -- create one of these and fake a keyvalue to get the right explosion setup
void ExplosionCreate( const Vector &center, const Vector &angles, edict_t *pOwner, int magnitude, BOOL doDamage )
{
	KeyValueData	kvd;
	char			buf[128];

	CBaseEntity *pExplosion = CBaseEntity::Create( "env_explosion", center, angles, pOwner );
	sprintf( buf, "%3d", magnitude );
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = buf;
	pExplosion->KeyValue( &kvd );
	if ( !doDamage )
		pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;

	pExplosion->Spawn();
	pExplosion->Use( NULL, NULL, USE_TOGGLE, 0 );
}

