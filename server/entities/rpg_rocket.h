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
	void Spawn();
	void Precache();
	void FollowThink();
	void IgniteThink();
	static CRpgRocket *CreateRpgRocket(Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CRpg *pLauncher);
	void CreateTrail();
	void OnTeleport() override;
	void Explode( TraceResult *pTrace, int bitsDamageType ) override;
	CRpg *GetLauncher();

	DECLARE_DATADESC();

	int m_iTrail;
	float m_flIgniteTime;
	EHANDLE m_hLauncher;
};