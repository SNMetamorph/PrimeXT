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

#include "path_track.h"
#include "trains.h"

BEGIN_DATADESC(CPathTrack)
	DEFINE_FIELD(m_length, FIELD_FLOAT),
	DEFINE_FIELD(m_pnext, FIELD_CLASSPTR),
	DEFINE_FIELD(m_paltpath, FIELD_CLASSPTR),
	DEFINE_FIELD(m_pprevious, FIELD_CLASSPTR),
	DEFINE_KEYFIELD(m_altName, FIELD_STRING, "altpath"),
	DEFINE_KEYFIELD(m_eOrientationType, FIELD_INTEGER, "type"),
	DEFINE_KEYFIELD(m_iszFireFow, FIELD_STRING, "m_iszFireFow"),
	DEFINE_KEYFIELD(m_iszFireRev, FIELD_STRING, "m_iszFireRev"),
#ifdef PATH_SPARKLE_DEBUG
	DEFINE_FUNCTION(Sparkle),
#endif
END_DATADESC()

LINK_ENTITY_TO_CLASS(path_track, CPathTrack);

CPathTrack::CPathTrack()
{
	m_eOrientationType = TrackOrientation_FacePath;
}

//
// Cache user-entity-field values until spawn is called.
//
void CPathTrack::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "altpath"))
	{
		m_altName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszFireFow"))
	{
		m_iszFireFow = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszFireRev"))
	{
		m_iszFireRev = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "type"))
	{
		m_eOrientationType = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue(pkvd);
}

STATE CPathTrack::GetState(void)
{
	// Use toggles between two paths
	if (m_paltpath)
	{
		if (FBitSet(pev->spawnflags, SF_PATH_ALTERNATE))
			return STATE_ON;
	}
	else
	{
		if (FBitSet(pev->spawnflags, SF_PATH_DISABLED))
			return STATE_ON; // yes, there is no error, disabled path is STATE_ON
	}

	return STATE_OFF;
}

void CPathTrack::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	int on;

	// Use toggles between two paths
	if (m_paltpath)
	{
		on = !FBitSet(pev->spawnflags, SF_PATH_ALTERNATE);
		if (ShouldToggle(useType, on))
		{
			if (on) SetBits(pev->spawnflags, SF_PATH_ALTERNATE);
			else ClearBits(pev->spawnflags, SF_PATH_ALTERNATE);
		}
	}
	else
	{
		// Use toggles between enabled/disabled
		on = !FBitSet(pev->spawnflags, SF_PATH_DISABLED);

		if (ShouldToggle(useType, on))
		{
			if (on) SetBits(pev->spawnflags, SF_PATH_DISABLED);
			else ClearBits(pev->spawnflags, SF_PATH_DISABLED);
		}
	}
}

void CPathTrack::Link(void)
{
	CBaseEntity *pTarget;

	if (!FStringNull(pev->target))
	{
		pTarget = UTIL_FindEntityByTargetname(NULL, GetTarget());

		if (pTarget == this)
		{
			ALERT(at_error, "path_track (%s) refers to itself as a target!\n", GetDebugName());
		}
		else if (pTarget)
		{
			m_pnext = CPathTrack::Instance(pTarget->edict());

			// if no next pointer, this is the end of a path
			if (m_pnext)
				m_pnext->SetPrevious(this);
		}
		else
		{
			ALERT(at_warning, "Dead end link %s\n", GetTarget());
		}
	}

	// Find "alternate" path
	if (m_altName)
	{
		pTarget = UTIL_FindEntityByTargetname(NULL, STRING(m_altName));

		if (pTarget)
		{
			m_paltpath = CPathTrack::Instance(pTarget->edict());

			// if no next pointer, this is the end of a path
			if (m_paltpath)
				m_paltpath->SetPrevious(this);
		}
	}
}

void CPathTrack::Spawn(void)
{
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));

	m_pnext = NULL;
	m_pprevious = NULL;

	// DEBUGGING CODE
#ifdef PATH_SPARKLE_DEBUG
	SetThink(&Sparkle);
	pev->nextthink = gpGlobals->time + 0.5;
#endif
}

void CPathTrack::Activate(void)
{
	BaseClass::Activate();

	// link to next, and back-link
	if (!FStringNull(pev->targetname))
		Link();
}

CPathTrack *CPathTrack::ValidPath(CPathTrack *ppath, int testFlag)
{
	if (!ppath)
		return NULL;

	if (testFlag && FBitSet(ppath->pev->spawnflags, SF_PATH_DISABLED))
		return NULL;

	return ppath;
}

void CPathTrack::Project(CPathTrack *pstart, CPathTrack *pend, Vector &origin, float dist)
{
	if (pstart && pend)
	{
		Vector dir = (pend->GetLocalOrigin() - pstart->GetLocalOrigin());
		dir = dir.Normalize();
		origin = pend->GetLocalOrigin() + dir * dist;
	}
}

CPathTrack *CPathTrack::GetNext(void)
{
	if (m_paltpath && FBitSet(pev->spawnflags, SF_PATH_ALTERNATE) && !FBitSet(pev->spawnflags, SF_PATH_ALTREVERSE))
		return m_paltpath;

	return m_pnext;
}

CPathTrack *CPathTrack::GetPrevious(void)
{
	if (m_paltpath && FBitSet(pev->spawnflags, SF_PATH_ALTERNATE) && FBitSet(pev->spawnflags, SF_PATH_ALTREVERSE))
		return m_paltpath;

	return m_pprevious;
}

void CPathTrack::SetPrevious(CPathTrack *pprev)
{
	// Only set previous if this isn't my alternate path
	if (pprev && !FStrEq(pprev->GetTargetname(), STRING(m_altName)))
		m_pprevious = pprev;
}

CPathTrack *CPathTrack::GetNextInDir(bool bForward)
{
	if (bForward)
		return GetNext();

	return GetPrevious();
}

// Assumes this is ALWAYS enabled
CPathTrack *CPathTrack::LookAhead(Vector &origin, float dist, int move, CPathTrack **pNextNext)
{
	CPathTrack *pcurrent = this;
	float originalDist = dist;
	Vector currentPos = origin;

	bool bForward = true;
	if (dist < 0)
	{
		// Travelling backwards along the path.
		dist = -dist;
		bForward = false;
	}

	// Move along the path, until we've gone 'dist' units or run out of path.
	while (dist > 0)
	{
		// If there is no next path track, or it's disabled, we're done.
		if (!ValidPath(pcurrent->GetNextInDir(bForward), move))
		{
			if (!move)
			{
				Project(pcurrent->GetNextInDir(!bForward), pcurrent, origin, dist);
			}

			return NULL;
		}

		// The next path track is valid. How far are we from it?
		Vector dir = pcurrent->GetNextInDir(bForward)->GetLocalOrigin() - currentPos;
		float length = dir.Length();

		// If we are at the next node and there isn't one beyond it, return the next node.
		if (!length && !ValidPath(pcurrent->GetNextInDir(bForward)->GetNextInDir(bForward), move))
		{
			if (pNextNext)
			{
				*pNextNext = NULL;
			}

			if (dist == originalDist)
			{
				// Didn't move at all, must be in a dead end.
				return NULL;
			}

			return pcurrent->GetNextInDir(bForward);
		}

		// If we don't hit the next path track within the distance remaining, we're done.
		if (length > dist)
		{
			origin = currentPos + (dir * (dist / length));
			if (pNextNext)
			{
				*pNextNext = pcurrent->GetNextInDir(bForward);
			}

			return pcurrent;
		}

		// We hit the next path track, advance to it.
		dist -= length;
		currentPos = pcurrent->GetNextInDir(bForward)->GetLocalOrigin();
		pcurrent = pcurrent->GetNextInDir(bForward);
		origin = currentPos;
	}

	// We consumed all of the distance, and exactly landed on a path track.
	if (pNextNext)
	{
		*pNextNext = pcurrent->GetNextInDir(bForward);
	}

	return pcurrent;
}

// Assumes this is ALWAYS enabled
CPathTrack *CPathTrack::Nearest(const Vector &origin)
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

	while (ppath && ppath != this)
	{
		deadCount++;

		if (deadCount > 9999)
		{
			ALERT(at_error, "Bad sequence of path_tracks from %s", GetDebugName());
			return NULL;
		}

		delta = origin - ppath->GetLocalOrigin();
		delta.z = 0;
		dist = delta.Length();

		if (dist < minDist)
		{
			minDist = dist;
			pnearest = ppath;
		}

		ppath = ppath->GetNext();
	}

	return pnearest;
}

Vector CPathTrack::GetOrientation(bool bForwardDir)
{
	if (m_eOrientationType == TrackOrientation_FacePathAngles)
	{
		return GetLocalAngles();
	}

	CPathTrack *pPrev = this;
	CPathTrack *pNext = GetNextInDir(bForwardDir);

	if (!pNext)
	{
		pPrev = GetNextInDir(!bForwardDir);
		pNext = this;
	}

	Vector vecDir = pNext->GetLocalOrigin() - pPrev->GetLocalOrigin();

	Vector angDir = UTIL_VecToAngles(vecDir);
	// The train actually points west
	angDir.y += 180;

	return angDir;
}

CPathTrack *CPathTrack::Instance(edict_t *pent)
{
	if (FClassnameIs(pent, "path_track"))
		return (CPathTrack *)GET_PRIVATE(pent);

	return NULL;
}

// DEBUGGING CODE
#ifdef PATH_SPARKLE_DEBUG
void CPathTrack::Sparkle(void)
{
	SetNextThink(0.2);

	if (FBitSet(pev->spawnflags, SF_PATH_DISABLED))
		UTIL_ParticleEffect(GetLocalOrigin(), Vector(0, 0, 100), 210, 10);
	else UTIL_ParticleEffect(GetLocalOrigin(), Vector(0, 0, 100), 84, 10);
}
#endif
