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

#include "beam.h"
#include "weapons.h"
#include "customentity.h"

LINK_ENTITY_TO_CLASS( beam, CBeam );

BEGIN_DATADESC( CBeam )
	DEFINE_FUNCTION( TriggerTouch ),
END_DATADESC()

void CBeam::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT; // Remove model & collisions
}

void CBeam::Precache( void )
{
	if ( pev->owner )
		SetStartEntity( ENTINDEX( pev->owner ) );
	if ( pev->aiment )
		SetEndEntity( ENTINDEX( pev->aiment ) );
}

void CBeam::SetStartEntity( int entityIndex ) 
{ 
	pev->sequence = (entityIndex & 0x0FFF) | ((pev->sequence&0xF000)<<12); 
	pev->owner = INDEXENT( entityIndex );
}

void CBeam::SetEndEntity( int entityIndex ) 
{ 
	pev->skin = (entityIndex & 0x0FFF) | ((pev->skin&0xF000)<<12); 
	pev->aiment = INDEXENT( entityIndex );
}

// These don't take attachments into account
const Vector &CBeam::GetStartPos( void )
{
	if ( GetType() == BEAM_ENTS )
	{
		CBaseEntity *pEntity;
		pEntity = CBaseEntity :: Instance( INDEXENT( GetStartEntity() ));
		return pEntity->GetLocalOrigin();
	}
	return GetLocalOrigin();
}

const Vector &CBeam::GetEndPos( void )
{
	int type = GetType();

	if( type == BEAM_POINTS || type == BEAM_HOSE )
		return m_vecEndPos;

	CBaseEntity *pEntity;
	pEntity = CBaseEntity :: Instance( INDEXENT( GetStartEntity() ));
	if( pEntity ) return pEntity->GetLocalOrigin();

	return m_vecEndPos;
}

void CBeam::SetAbsStartPos( const Vector &pos )
{
	if( m_hParent == NULL )
	{
		SetStartPos( pos );
		return;
	}

	matrix4x4	worldToBeam = EntityToWorldTransform();
	Vector vecLocalPos = worldToBeam.VectorITransform( pos );

	SetStartPos( vecLocalPos );
}

void CBeam::SetAbsEndPos( const Vector &pos )
{
	if( m_hParent == NULL )
	{
		SetEndPos( pos );
		return;
	}

	matrix4x4	worldToBeam = EntityToWorldTransform();
	Vector vecLocalPos = worldToBeam.VectorITransform( pos );

	SetEndPos( vecLocalPos );
}

// These don't take attachments into account
const Vector &CBeam::GetAbsStartPos( void ) const
{
	if( GetType() == BEAM_ENTS && GetStartEntity( ))
	{
		CBaseEntity *pEntity;
		pEntity = CBaseEntity :: Instance( INDEXENT( GetStartEntity() ));

		if( pEntity )
			return pEntity->GetAbsOrigin();
		return GetAbsOrigin();
	}
	return GetAbsOrigin();
}

const Vector &CBeam::GetAbsEndPos( void ) const
{
	if( GetType() != BEAM_POINTS && GetType() != BEAM_HOSE && GetEndEntity() ) 
	{
		CBaseEntity *pEntity;
		pEntity = CBaseEntity :: Instance( INDEXENT( GetEndEntity() ));

		if( pEntity )
			return pEntity->GetAbsOrigin();
	}

	if( const_cast<CBeam*>(this)->m_hParent == NULL )
		return m_vecEndPos;

	matrix4x4	beamToWorld = EntityToWorldTransform();
	const_cast<CBeam*>(this)->m_vecAbsEndPos = beamToWorld.VectorTransform( m_vecEndPos );

	return m_vecAbsEndPos;
}

CBeam *CBeam::BeamCreate( const char *pSpriteName, int width )
{
	// Create a new entity with CBeam private data
	CBeam *pBeam = GetClassPtr( (CBeam *)NULL );
	pBeam->pev->classname = MAKE_STRING( "beam" );
	pBeam->BeamInit( pSpriteName, width );

	return pBeam;
}

void CBeam::BeamInit( const char *pSpriteName, int width )
{
	pev->flags |= FL_CUSTOMENTITY;
	SetColor( 255, 255, 255 );
	SetBrightness( 255 );
	SetNoise( 0 );
	SetFrame( 0 );
	SetScrollRate( 0 );
	pev->model = MAKE_STRING( pSpriteName );
	SetTexture( PRECACHE_MODEL( (char *)pSpriteName ) );
	SetWidth( width );
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
}

void CBeam::PointsInit( const Vector &start, const Vector &end )
{
	SetType( BEAM_POINTS );
	SetStartPos( start );
	SetEndPos( end );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::HoseInit( const Vector &start, const Vector &direction )
{
	SetType( BEAM_HOSE );
	SetStartPos( start );
	SetEndPos( direction );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::PointEntInit( const Vector &start, int endIndex )
{
	SetType( BEAM_ENTPOINT );
	SetStartPos( start );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::EntsInit( int startIndex, int endIndex )
{
	SetType( BEAM_ENTS );
	SetStartEntity( startIndex );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::RelinkBeam( void )
{
	const Vector &startPos = GetAbsStartPos();
	const Vector &endPos = GetAbsEndPos();

	pev->mins.x = Q_min( startPos.x, endPos.x );
	pev->mins.y = Q_min( startPos.y, endPos.y );
	pev->mins.z = Q_min( startPos.z, endPos.z );
	pev->maxs.x = Q_max( startPos.x, endPos.x );
	pev->maxs.y = Q_max( startPos.y, endPos.y );
	pev->maxs.z = Q_max( startPos.z, endPos.z );
	pev->mins = pev->mins - GetAbsOrigin();
	pev->maxs = pev->maxs - GetAbsOrigin();

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	RelinkEntity( TRUE );
}

void CBeam::TriggerTouch( CBaseEntity *pOther )
{
	if( FBitSet( pOther->pev->flags, ( FL_CLIENT|FL_MONSTER ) ))
	{
		if( pev->owner )
		{
			CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
			pOwner->Use( pOther, this, USE_TOGGLE, 0 );
		}
		ALERT( at_console, "Firing targets!!!\n" );
	}
}

CBaseEntity *CBeam::RandomTargetname( const char *szName )
{
	int total = 0;

	if( !szName || !*szName )
		return NULL;

	CBaseEntity *pEntity = NULL;
	CBaseEntity *pNewEntity = NULL;

	while(( pNewEntity = UTIL_FindEntityByTargetname( pNewEntity, szName )) != NULL )
	{
		total++;
		if( RANDOM_LONG( 0, total - 1 ) < 1 )
			pEntity = pNewEntity;
	}
	return pEntity;
}

void CBeam::DoSparks( const Vector &start, const Vector &end )
{
	if( FBitSet( pev->spawnflags, ( SF_BEAM_SPARKSTART|SF_BEAM_SPARKEND )))
	{
		if( FBitSet( pev->spawnflags, SF_BEAM_SPARKSTART ))
		{
			UTIL_Sparks( start );
		}
		if( FBitSet( pev->spawnflags, SF_BEAM_SPARKEND ))
		{
			UTIL_Sparks( end );
		}
	}
}

CBaseEntity *CBeam::GetTripEntity( TraceResult *ptr )
{
	CBaseEntity *pTrip;

	if( ptr->flFraction == 1.0 || ptr->pHit == NULL )
		return NULL;

	pTrip = CBaseEntity::Instance( ptr->pHit );
	if( pTrip == NULL )
		return NULL;

	if( FStringNull( pev->netname ))
	{
		if( pTrip->pev->flags & (FL_CLIENT|FL_MONSTER))
			return pTrip;
		else
			return NULL;
	}
	else if( FClassnameIs( pTrip->pev, STRING( pev->netname )))
		return pTrip;
	else if( FStrEq( pTrip->GetTargetname(), STRING( pev->netname )))
		return pTrip;

	return NULL;
}

void CBeam::BeamDamage( TraceResult *ptr )
{
	RelinkBeam();

	if( ptr->flFraction != 1.0 && ptr->pHit != NULL )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( ptr->pHit );
		if ( pHit )
		{
			if( pev->dmg > 0 )
			{  
				ClearMultiDamage();
				pHit->TraceAttack( pev, pev->dmg * (gpGlobals->time - pev->dmgtime), (ptr->vecEndPos - GetAbsOrigin()).Normalize(), ptr, DMG_ENERGYBEAM );
				ApplyMultiDamage( pev, pev );
				if ( pev->spawnflags & SF_BEAM_DECALS )
				{
					if ( pHit->IsBSPModel() )
						UTIL_DecalTrace( ptr, "{bigshot1" );
				}
			}
			else
			{
				//LRC - beams that heal people
				pHit->TakeHealth( -(pev->dmg * (gpGlobals->time - pev->dmgtime)), DMG_GENERIC );
			}
		}
	}

	pev->dmgtime = gpGlobals->time;
}
