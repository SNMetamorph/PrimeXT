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
#include "monster_tripmine.h"
#include "weapons/tripmine.h"

LINK_ENTITY_TO_CLASS(monster_tripmine, CTripmineGrenade);

BEGIN_DATADESC(CTripmineGrenade)
	DEFINE_FIELD(m_flPowerUp, FIELD_TIME),
	DEFINE_FIELD(m_hOwner, FIELD_EHANDLE),
	DEFINE_FIELD(m_pBeam, FIELD_CLASSPTR),
	DEFINE_FIELD(m_pRealOwner, FIELD_EDICT),
	DEFINE_FUNCTION(PowerupThink),
	DEFINE_FUNCTION(BeamBreakThink),
	DEFINE_FUNCTION(DelayDeathThink),
END_DATADESC()

void CTripmineGrenade::Spawn(void)
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	SET_MODEL(edict(), "models/v_tripmine.mdl");
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = TRIPMINE_WORLD;
	ResetSequenceInfo();
	pev->framerate = 0;
	SetBits(pev->effects, EF_NOINTERP);

	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));

	if (!m_fSetAngles)
	{
		SetLocalAngles(g_vecZero);
	}

	if (pev->spawnflags & 1)
	{
		// power up quickly
		m_flPowerUp = gpGlobals->time + 1.0;
	}
	else
	{
		// power up in 2.5 seconds
		m_flPowerUp = gpGlobals->time + 2.5;
	}

	SetThink(&CTripmineGrenade::PowerupThink);
	SetNextThink(0.2);

	pev->takedamage = DAMAGE_YES;
	pev->dmg = gSkillData.plrDmgTripmine;
	pev->health = 1; // don't let die normally

	if (pev->owner != NULL)
	{
		// play deploy sound
		EMIT_SOUND(edict(), CHAN_VOICE, "weapons/mine_deploy.wav", 1.0, ATTN_NORM);
		EMIT_SOUND(edict(), CHAN_BODY, "weapons/mine_charge.wav", 0.2, ATTN_NORM); // chargeup

		m_pRealOwner = pev->owner;// see CTripmineGrenade for why.
	}
}

void CTripmineGrenade::Precache(void)
{
	PRECACHE_MODEL("models/v_tripmine.mdl");
	PRECACHE_SOUND("weapons/mine_deploy.wav");
	PRECACHE_SOUND("weapons/mine_activate.wav");
	PRECACHE_SOUND("weapons/mine_charge.wav");
}

void CTripmineGrenade::OnChangeLevel(void)
{
	// NOTE: beam for moveable entities can't stay here
	// because entity will moved on a next level but beam is not
	KillBeam();

	// NOTE: clear parent. We can't moving it properly for non-global entities
	SetParent(0);

	m_hOwner = NULL;	// i lost my star in Krasnodar :-)
}

void CTripmineGrenade::TransferReset(void)
{
	// NOTE: i'm need to do it here because my parent already has
	// landmark offset but i'm is not
	Vector origin = GetAbsOrigin();
	origin += gpGlobals->vecLandmarkOffset;
	SetAbsOrigin(origin);

	// changelevel issues
	if (m_hParent == NULL && m_hOwner == NULL)
	{
		TraceResult tr;

		UTIL_MakeVectors(GetAbsAngles());
		Vector vecSrc = GetAbsOrigin() + gpGlobals->v_forward * -32;
		Vector vecDst = GetAbsOrigin() + gpGlobals->v_forward * 32;

		UTIL_TraceHull(vecSrc, vecDst, ignore_monsters, head_hull, edict(), &tr);

		if (tr.pHit && ENTINDEX(tr.pHit) != 0)
		{
			CBaseEntity *pNewParent = CBaseEntity::Instance(tr.pHit);

			if (pNewParent && pNewParent->IsBSPModel())
			{
				ALERT(at_aiconsole, "SetNewParent: %s\n", pNewParent->GetClassname());
				SetParent(pNewParent);
				m_hOwner = pNewParent;
			}
			else
			{
				m_hOwner = g_pWorld;
			}
		}
		else
		{
			m_hOwner = g_pWorld;
		}
	}

	SetBits(pev->effects, EF_NOINTERP);

	// create new beam on a next level
	if (!m_pBeam)
	{
		MakeBeam();
	}
}

void CTripmineGrenade::PowerupThink(void)
{
	TraceResult tr;

	if (m_hOwner == NULL)
	{
		// find an owner
		edict_t *oldowner = pev->owner;
		pev->owner = NULL;

		if (m_hParent != NULL)
		{
			// g-cont. my owner is parent!
			m_hOwner = m_hParent;
		}
		else
		{
			m_hOwner = g_pWorld;
		}
	}

	if (gpGlobals->time > m_flPowerUp)
	{
		// make solid
		pev->solid = SOLID_BBOX;
		RelinkEntity(TRUE);

		MakeBeam();

		// play enabled sound
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/mine_activate.wav", 0.5, ATTN_NORM, 1.0, 75);
	}

	SetNextThink(0.1);
}

void CTripmineGrenade::KillBeam(void)
{
	if (m_pBeam)
	{
		UTIL_Remove(m_pBeam);
		m_pBeam = NULL;
	}
}

void CTripmineGrenade::MakeBeam(void)
{
	TraceResult tr;

	// g-cont. moved here from spawn
	UTIL_MakeVectors(GetAbsAngles());

	Vector vecSrc = GetAbsOrigin();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 2048;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr);

	// set to follow laser spot
	SetThink(&CTripmineGrenade::BeamBreakThink);
	SetNextThink(0.1);

	m_pBeam = CBeam::BeamCreate(g_pModelNameLaser, 10);
	m_pBeam->PointEntInit(tr.vecEndPos, entindex());
	m_pBeam->SetColor(0, 214, 198);
	m_pBeam->SetScrollRate(255);
	m_pBeam->SetBrightness(64);
	m_pBeam->SetParent(this);

#if 0	// for testing parent system
	CBaseEntity *pEnt = CBaseEntity::Create("info_target", tr.endpos + tr.plane.normal * 8, g_vecZero, NULL);
	pEnt->SetModel("sprites/laserdot.spr");
	pEnt->SetParent(this);
	pEnt->pev->rendermode = kRenderGlow;
	pEnt->pev->renderfx = kRenderFxNoDissipation;
	pEnt->pev->renderamt = 255;
#endif
}

void CTripmineGrenade::BeamBreakThink(void)
{
	BOOL bBlowup = 0;

	// respawn detect. 
	if (!m_pBeam)
	{
		MakeBeam();
		if (m_hParent)
			m_hOwner = m_hParent;// reset owner too
		else if (m_hOwner == NULL)
			m_hOwner = g_pWorld;// world-placed mine
	}

	TraceResult tr, tr2;

	// HACKHACK Set simple box using this really nice global!
	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;

	UTIL_TraceLine(m_pBeam->GetAbsEndPos(), m_pBeam->GetAbsStartPos(), dont_ignore_monsters, edict(), &tr);

	UTIL_MakeVectors(GetAbsAngles());
	Vector vecTest = m_pBeam->GetAbsStartPos() + gpGlobals->v_forward * 1;

	// make sure what beam end is in solid
	UTIL_TraceLine(vecTest, vecTest, dont_ignore_monsters, edict(), &tr2);

	if (tr.flFraction < 1.0f || !tr2.fAllSolid)
	{
		bBlowup = 1;
	}
	else
	{
		if (m_hOwner == NULL)
			bBlowup = 1;
	}

	if (bBlowup)
	{
		// a bit of a hack, but all CGrenade code passes pev->owner along to make sure the proper player gets credit for the kill
		// so we have to restore pev->owner from pRealOwner, because an entity's tracelines don't strike it's pev->owner which meant
		// that a player couldn't trigger his own tripmine. Now that the mine is exploding, it's safe the restore the owner so the 
		// CGrenade code knows who the explosive really belongs to.
		pev->owner = m_pRealOwner;
		pev->health = 0;
		Killed(VARS(pev->owner), GIB_NORMAL);
		return;
	}

	SetNextThink(0.1);
}

int CTripmineGrenade::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	if (gpGlobals->time < m_flPowerUp && flDamage < pev->health)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		KillBeam();
		return FALSE;
	}
	return CGrenade::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

void CTripmineGrenade::Killed(entvars_t *pevAttacker, int iGib)
{
	pev->takedamage = DAMAGE_NO;

	if (pevAttacker && (pevAttacker->flags & FL_CLIENT))
	{
		// some client has destroyed this mine, he'll get credit for any kills
		pev->owner = ENT(pevAttacker);
	}

	SetThink(&CTripmineGrenade::DelayDeathThink);
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.1, 0.3);

	EMIT_SOUND(ENT(pev), CHAN_BODY, "common/null.wav", 0.5, ATTN_NORM); // shut off chargeup
}

void CTripmineGrenade::DelayDeathThink(void)
{
	KillBeam();
	TraceResult tr;

	UTIL_MakeVectors(GetAbsAngles());
	Vector vecDir = gpGlobals->v_forward;

	UTIL_TraceLine(GetAbsOrigin() + vecDir * 8, GetAbsOrigin() - vecDir * 64, dont_ignore_monsters, edict(), &tr);
	Explode(&tr, DMG_BLAST);
}