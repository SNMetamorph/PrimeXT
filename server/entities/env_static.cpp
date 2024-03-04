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

#include "env_static.h"

LINK_ENTITY_TO_CLASS( env_static, CEnvStatic );

void CEnvStatic :: Precache( void )
{
	PRECACHE_MODEL( GetModel() );
}

void CEnvStatic :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq(pkvd->szKeyName, "xform"))
	{
		UTIL_StringToVector( (float*)pev->startpos, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "vlight_cache"))
	{
		m_iVertexLightCache = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "flight_cache"))
	{
		m_iSurfaceLightCache = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CEnvStatic :: Spawn( void )
{
	if( pev->startpos == g_vecZero )
		pev->startpos = Vector( pev->scale, pev->scale, pev->scale );

	if (m_iSurfaceLightCache)
	{
		SetBits(pev->iuser1, CF_STATIC_LIGHTMAPPED);
		pev->colormap = m_iSurfaceLightCache;
	}
	else
	{
		pev->colormap = m_iVertexLightCache;
	}

	// check xform values
	if( pev->startpos.x < 0.01f ) pev->startpos.x = 1.0f;
	if( pev->startpos.y < 0.01f ) pev->startpos.y = 1.0f;
	if( pev->startpos.z < 0.01f ) pev->startpos.z = 1.0f;
	if( pev->startpos.x > 16.0f ) pev->startpos.x = 16.0f;
	if( pev->startpos.y > 16.0f ) pev->startpos.y = 16.0f;
	if( pev->startpos.z > 16.0f ) pev->startpos.z = 16.0f;

	Precache();
	SET_MODEL( edict(), GetModel() );

	// tell the client about static entity
	SetBits( pev->iuser1, CF_STATIC_ENTITY );

	if( FBitSet( pev->spawnflags, SF_STATIC_SOLID ))
	{
		if( WorldPhysic->Initialized( ))
			pev->solid = SOLID_CUSTOM;
		pev->movetype = MOVETYPE_NONE;
		AutoSetSize();
	}

	if( FBitSet( pev->spawnflags, SF_STATIC_DROPTOFLOOR ))
	{
		Vector origin = GetLocalOrigin();
		origin.z += 1;
		SetLocalOrigin( origin );
		UTIL_DropToFloor( this );
	}
	else
	{
		UTIL_SetOrigin( this, GetLocalOrigin( ));
	}

	if (FBitSet(pev->spawnflags, SF_STATIC_NODYNSHADOWS)) {
		SetBits(pev->effects, EF_NOSHADOW);
	}

	if( FBitSet( pev->spawnflags, SF_STATIC_SOLID ))
	{
		m_pUserData = WorldPhysic->CreateStaticBodyFromEntity( this );
		RelinkEntity( TRUE );
	}
	else
	{
		// remove from server
		MAKE_STATIC( edict() );
	}
}

void CEnvStatic :: SetObjectCollisionBox( void )
{
	// expand for rotation
	TransformAABB( EntityToWorldTransform(), pev->mins, pev->maxs, pev->absmin, pev->absmax );

	pev->absmin.x -= 1;
	pev->absmin.y -= 1;
	pev->absmin.z -= 1;
	pev->absmax.x += 1;
	pev->absmax.y += 1;
	pev->absmax.z += 1;
}

// automatically set collision box
void CEnvStatic :: AutoSetSize( void )
{
	studiohdr_t *pstudiohdr;
	pstudiohdr = (studiohdr_t *)GET_MODEL_PTR( edict() );

	if( pstudiohdr == NULL )
	{
		UTIL_SetSize( pev, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ));
		ALERT( at_error, "env_model: unable to fetch model pointer!\n" );
		return;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	UTIL_SetSize( pev, pseqdesc[pev->sequence].bbmin * pev->startpos, pseqdesc[pev->sequence].bbmax * pev->startpos );
}
