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
//
// ========================== PATH_CORNER ===========================
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trains.h"
#include "saverestore.h"

class CPathCorner : public CBaseDelay
{
	DECLARE_CLASS( CPathCorner, CBaseDelay );
public:
	void Spawn( void ) { ASSERTSZ( !FStringNull( pev->targetname ), "path_corner without a targetname" ); }
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	float GetDelay( void ) { return m_flWait; }
};

LINK_ENTITY_TO_CLASS( path_corner, CPathCorner );

BEGIN_DATADESC( CPathTrack )
	DEFINE_FIELD( m_length, FIELD_FLOAT ),
	DEFINE_FIELD( m_pnext, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_paltpath, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pprevious, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_altName, FIELD_STRING, "altpath" ),
	DEFINE_KEYFIELD( m_iszFireFow, FIELD_STRING, "m_iszFireFow" ),
	DEFINE_KEYFIELD( m_iszFireRev, FIELD_STRING, "m_iszFireRev" ),
#ifdef PATH_SPARKLE_DEBUG
	DEFINE_FUNCTION( Sparkle ),
#endif
END_DATADESC()

LINK_ENTITY_TO_CLASS( path_track, CPathTrack );

//
// Cache user-entity-field values until spawn is called.
//
void CPathTrack :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "altpath" ))
	{
		m_altName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszFireFow" ))
	{
		m_iszFireFow = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_iszFireRev" ))
	{
		m_iszFireRev = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CPathTrack :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int on;

	// Use toggles between two paths
	if( m_paltpath )
	{
		on = !FBitSet( pev->spawnflags, SF_PATH_ALTERNATE );
		if( ShouldToggle( useType, on ))
		{
			if( on ) SetBits( pev->spawnflags, SF_PATH_ALTERNATE );
			else ClearBits( pev->spawnflags, SF_PATH_ALTERNATE );
		}
	}
	else	
	{
		// Use toggles between enabled/disabled
		on = !FBitSet( pev->spawnflags, SF_PATH_DISABLED );

		if( ShouldToggle( useType, on ))
		{
			if( on ) SetBits( pev->spawnflags, SF_PATH_DISABLED );
			else ClearBits( pev->spawnflags, SF_PATH_DISABLED );
		}
	}
}

void CPathTrack :: Link( void  )
{
	CBaseEntity *pTarget;

	if( !FStringNull( pev->target ))
	{
		pTarget = UTIL_FindEntityByTargetname( NULL, GetTarget( ));

		if( pTarget == this )
		{
			ALERT( at_error, "path_track (%s) refers to itself as a target!\n", GetTargetname( ));
		}
		else if( pTarget )
		{
			m_pnext = CPathTrack :: Instance( pTarget->edict() );

			// if no next pointer, this is the end of a path
			if( m_pnext )
				m_pnext->SetPrevious( this );
		}
		else
		{
			ALERT( at_console, "Dead end link %s\n", GetTarget());
		}
	}

	// Find "alternate" path
	if( m_altName )
	{
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_altName ));
		m_paltpath = CPathTrack :: Instance( pTarget->edict() );

		if( m_paltpath )
		{
			// if no next pointer, this is the end of a path
			m_paltpath->SetPrevious( this );
		}
	}
}

void CPathTrack :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize( pev, Vector( -8, -8, -8 ), Vector( 8, 8, 8 ));

	m_pnext = NULL;
	m_pprevious = NULL;

// DEBUGGING CODE
#ifdef PATH_SPARKLE_DEBUG
	SetThink( Sparkle );
	pev->nextthink = gpGlobals->time + 0.5;
#endif
}

void CPathTrack :: Activate( void )
{
	// link to next, and back-link
	if( !FStringNull( pev->targetname ))
		Link();
}

CPathTrack *CPathTrack :: ValidPath( CPathTrack *ppath, int testFlag )
{
	if( !ppath )
		return NULL;

	if( testFlag && FBitSet( ppath->pev->spawnflags, SF_PATH_DISABLED ))
		return NULL;

	return ppath;
}

void CPathTrack :: Project( CPathTrack *pstart, CPathTrack *pend, Vector *origin, float dist )
{
	if( pstart && pend )
	{
		Vector dir = (pend->GetLocalOrigin() - pstart->GetLocalOrigin());
		dir = dir.Normalize();
		*origin = pend->GetLocalOrigin() + dir * dist;
	}
}

CPathTrack *CPathTrack::GetNext( void )
{
	if( m_paltpath && FBitSet( pev->spawnflags, SF_PATH_ALTERNATE ) && !FBitSet( pev->spawnflags, SF_PATH_ALTREVERSE ))
		return m_paltpath;
	
	return m_pnext;
}

CPathTrack *CPathTrack::GetPrevious( void )
{
	if( m_paltpath && FBitSet( pev->spawnflags, SF_PATH_ALTERNATE ) && FBitSet( pev->spawnflags, SF_PATH_ALTREVERSE ))
		return m_paltpath;
	
	return m_pprevious;
}

void CPathTrack::SetPrevious( CPathTrack *pprev )
{
	// Only set previous if this isn't my alternate path
	if( pprev && !FStrEq( pprev->GetTargetname(), STRING( m_altName )))
		m_pprevious = pprev;
}

// Assumes this is ALWAYS enabled
CPathTrack *CPathTrack :: LookAhead( Vector *origin, float dist, int move )
{
	CPathTrack *pcurrent;
	float originalDist = dist;
	
	pcurrent = this;
	Vector currentPos = *origin;

	if( dist < 0 )	// Travelling backwards through path
	{
		dist = -dist;

		while( dist > 0 )
		{
			Vector dir = pcurrent->GetLocalOrigin() - currentPos;
			float length = dir.Length();

			if( !length )
			{
			 	// If there is no previous node, or it's disabled, return now.
				if ( !ValidPath( pcurrent->GetPrevious(), move ))
				{
					if( !move )
						Project( pcurrent->GetNext(), pcurrent, origin, dist );
					return NULL;
				}
				pcurrent = pcurrent->GetPrevious();
			}
			else if( length > dist )	// enough left in this path to move
			{
				*origin = currentPos + (dir * (dist / length));
				return pcurrent;
			}
			else
			{
				dist -= length;
				currentPos = pcurrent->GetLocalOrigin();
				*origin = currentPos;
				if( !ValidPath( pcurrent->GetPrevious(), move ))	// If there is no previous node, or it's disabled, return now.
					return NULL;

				pcurrent = pcurrent->GetPrevious();
			}
		}

		*origin = currentPos;

		return pcurrent;
	}
	else 
	{
		while( dist > 0 )
		{
			if( !ValidPath( pcurrent->GetNext(), move ))	// If there is no next node, or it's disabled, return now.
			{
				if( !move )
					Project( pcurrent->GetPrevious(), pcurrent, origin, dist );
				return NULL;
			}

			Vector dir = pcurrent->GetNext()->GetLocalOrigin() - currentPos;
			float length = dir.Length();

			if( !length && !ValidPath( pcurrent->GetNext()->GetNext(), move ))
			{
				if( dist == originalDist ) // HACK -- up against a dead end
					return NULL;
				return pcurrent;
			}

			if( length > dist )	// enough left in this path to move
			{
				*origin = currentPos + (dir * (dist / length));
				return pcurrent;
			}
			else
			{
				dist -= length;
				currentPos = pcurrent->GetNext()->GetLocalOrigin();
				pcurrent = pcurrent->GetNext();
				*origin = currentPos;
			}
		}
		*origin = currentPos;
	}

	return pcurrent;
}
	
// Assumes this is ALWAYS enabled
CPathTrack *CPathTrack :: Nearest( Vector origin )
{
	int		deadCount;
	float		minDist, dist;
	Vector		delta;
	CPathTrack	*ppath, *pnearest;

	delta = origin - GetLocalOrigin();
	delta.z = 0;
	minDist = delta.Length();
	pnearest = this;
	ppath = GetNext();

	// Hey, I could use the old 2 racing pointers solution to this, but I'm lazy :)
	deadCount = 0;

	while( ppath && ppath != this )
	{
		deadCount++;

		if( deadCount > 9999 )
		{
			ALERT( at_error, "Bad sequence of path_tracks from %s", GetTargetname( ));
			return NULL;
		}

		delta = origin - ppath->GetLocalOrigin();
		delta.z = 0;
		dist = delta.Length();

		if( dist < minDist )
		{
			minDist = dist;
			pnearest = ppath;
		}

		ppath = ppath->GetNext();
	}

	return pnearest;
}

CPathTrack *CPathTrack::GetNextInDir( bool bForward )
{
	if( bForward )
		return GetNext();
	
	return GetPrevious();
}

Vector CPathTrack :: GetOrientation( bool bForwardDir )
{
	CPathTrack *pPrev = this;
	CPathTrack *pNext = GetNextInDir( bForwardDir );

	if( !pNext )
	{	
		pPrev = GetNextInDir( !bForwardDir );
		pNext = this;
	}

	Vector vecDir = pNext->GetLocalOrigin() - pPrev->GetLocalOrigin();

	Vector angDir = UTIL_VecToAngles( vecDir );
	// The train actually points west
	angDir.y += 180;

	return angDir;
}

CPathTrack *CPathTrack::Instance( edict_t *pent )
{ 
	if( FClassnameIs( pent, "path_track" ))
		return (CPathTrack *)GET_PRIVATE( pent );

	return NULL;
}

// DEBUGGING CODE
#ifdef PATH_SPARKLE_DEBUG
void CPathTrack :: Sparkle( void )
{
	SetNextThink( 0.2 );

	if( FBitSet( pev->spawnflags, SF_PATH_DISABLED ))
		UTIL_ParticleEffect(GetLocalOrigin(), Vector( 0, 0, 100 ), 210, 10 );
	else UTIL_ParticleEffect(GetLocalOrigin(), Vector( 0, 0, 100 ), 84, 10 );
}
#endif
