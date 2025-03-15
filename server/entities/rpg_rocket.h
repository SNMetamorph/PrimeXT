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
#include "monsters.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "user_messages.h"
#include "ggrenade.h"
#include "weapon_rpg.h"

class CRpgRocket : public CGrenade
{
	DECLARE_CLASS(CRpgRocket, CGrenade);
public:
	void Spawn(void);
	void Precache(void);
	void FollowThink(void);
	void IgniteThink(void);
	void RocketTouch(CBaseEntity *pOther);
	static CRpgRocket *CreateRpgRocket(Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CRpg *pLauncher);
	void CreateTrail(void);
	virtual void OnTeleport(void);

	DECLARE_DATADESC();

	int m_iTrail;
	float m_flIgniteTime;
	CRpg *m_pLauncher;// pointer back to the launcher that fired me. 
};