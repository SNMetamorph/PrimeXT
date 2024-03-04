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
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "env_beam.h"
#include "gamerules.h"
#include "ggrenade.h"

class CTripmineGrenade : public CGrenade
{
	DECLARE_CLASS(CTripmineGrenade, CGrenade);

	void Spawn(void);
	void Precache(void);

	DECLARE_DATADESC();

	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);

	void PowerupThink(void);
	void BeamBreakThink(void);
	void DelayDeathThink(void);
	void Killed(entvars_t *pevAttacker, int iGib);
	void TransferReset(void);
	void OnChangeLevel(void);

	void MakeBeam(void);
	void KillBeam(void);

	float	m_flPowerUp;

	EHANDLE	m_hOwner;
	CBeam	*m_pBeam;
	edict_t	*m_pRealOwner; // tracelines don't hit pev->owner, which means a player couldn't detonate his own trip mine, so we store the owner here.
};