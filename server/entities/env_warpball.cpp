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

#include "env_warpball.h"
#include "beam.h"
#include "env_sprite.h"

LINK_ENTITY_TO_CLASS( env_warpball, CEnvWarpBall );

void CEnvWarpBall :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "warp_target"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "damage_delay"))
	{
		pev->frags = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CEnvWarpBall::Precache( void )
{
	PRECACHE_MODEL( "sprites/lgtning.spr" );
	PRECACHE_MODEL( "sprites/Fexplo1.spr" );
	PRECACHE_SOUND( "debris/beamstart2.wav" );
	PRECACHE_SOUND( "debris/beamstart7.wav" );
}

void CEnvWarpBall::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int iTimes = 0;
	int iDrawn = 0;
	TraceResult tr;
	Vector vecDest;
	CBeam *pBeam;
	CBaseEntity *pEntity = UTIL_FindEntityByTargetname ( NULL, STRING(pev->message));
	edict_t *pos;
		
	if( pEntity ) // target found ?
	{
		vecOrigin = pEntity->GetAbsOrigin();
		pos = pEntity->edict();
	}
	else
	{         //use as center
		vecOrigin = GetAbsOrigin();
		pos = edict();
	}	

	EMIT_SOUND( pos, CHAN_BODY, "debris/beamstart2.wav", 1, ATTN_NORM );
	UTIL_ScreenShake( vecOrigin, 6, 160, 1.0, pev->button );
	CSprite *pSpr = CSprite::SpriteCreate( "sprites/Fexplo1.spr", vecOrigin, TRUE );
	pSpr->SetParent( this );
	pSpr->AnimateAndDie( 18 );
	pSpr->SetTransparency(kRenderGlow,  77, 210, 130,  255, kRenderFxNoDissipation);
	EMIT_SOUND( pos, CHAN_ITEM, "debris/beamstart7.wav", 1, ATTN_NORM );
	int iBeams = RANDOM_LONG(20, 40);
	while (iDrawn < iBeams && iTimes < (iBeams * 3))
	{
		vecDest = pev->button * (Vector(RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1)).Normalize());
		UTIL_TraceLine( vecOrigin, vecOrigin + vecDest, ignore_monsters, NULL, &tr);
		if (tr.flFraction != 1.0)
		{
			// we hit something.
			iDrawn++;
			pBeam = CBeam::BeamCreate("sprites/lgtning.spr", 200);
			pBeam->PointsInit( vecOrigin, tr.vecEndPos );
			pBeam->SetColor( 20, 243, 20 );
			pBeam->SetNoise( 65 );
			pBeam->SetBrightness( 220 );
			pBeam->SetWidth( 30 );
			pBeam->SetScrollRate( 35 );
//			pBeam->SetParent( this );
			pBeam->SetThink( &CBeam:: SUB_Remove );
			pBeam->pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.5, 1.6);
		}
		iTimes++;
	}
	pev->nextthink = gpGlobals->time + pev->frags;
}

void CEnvWarpBall::Think( void )
{
	SUB_UseTargets( this, USE_TOGGLE, 0);
 
	if ( pev->spawnflags & SF_KILL_CENTER )
	{
		CBaseEntity *pMonster = NULL;

		while ((pMonster = UTIL_FindEntityInSphere( pMonster, vecOrigin, 72 )) != NULL)
		{
			if ( FBitSet( pMonster->pev->flags, FL_MONSTER ) || FClassnameIs( pMonster->pev, "player"))
				pMonster->TakeDamage ( pev, pev, 100, DMG_GENERIC );
		}
	}
	if ( pev->spawnflags & SF_REMOVE_ON_FIRE ) UTIL_Remove( this );
}
