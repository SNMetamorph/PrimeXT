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
#include "saverestore.h"

//#define PATH_SPARKLE_DEBUG		// This makes a particle effect around path_track entities for debugging
class CPathTrack : public CPointEntity
{
	DECLARE_CLASS(CPathTrack, CPointEntity);
public:
	CPathTrack();

	void		Spawn(void);
	void		Activate(void);
	void		KeyValue(KeyValueData* pkvd);
	STATE		GetState(void);

	void		SetPrevious(CPathTrack *pprevious);
	void		Link(void);
	void		Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);

	CPathTrack	*ValidPath(CPathTrack *ppath, int testFlag = true); // Returns ppath if enabled, NULL otherwise
	void		Project(CPathTrack *pstart, CPathTrack *pend, Vector &origin, float dist);
	int		GetOrientationType(void) { return m_eOrientationType; }

	Vector		GetOrientation(bool bForwardDir);
	CPathTrack	*GetNextInDir(bool bForward);

	static CPathTrack *Instance(edict_t *pent);

	CPathTrack	*LookAhead(Vector &origin, float dist, int move, CPathTrack **pNextNext = NULL);
	CPathTrack	*Nearest(const Vector &origin);

	CPathTrack	*GetNext(void);
	CPathTrack	*GetPrevious(void);

	DECLARE_DATADESC();

#ifdef PATH_SPARKLE_DEBUG
	void 		Sparkle(void);
#endif
	float		m_length;
	string_t		m_altName;
	CPathTrack	*m_pnext;
	CPathTrack	*m_pprevious;
	CPathTrack	*m_paltpath;
	int		m_eOrientationType;

	string_t		m_iszFireFow;
	string_t		m_iszFireRev;
};