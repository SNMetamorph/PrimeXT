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

#include "monster_snark.h"
#include "weapons/snark.h"

#define SQUEEK_DETONATE_DELAY		15.0

LINK_ENTITY_TO_CLASS(monster_snark, CSqueakGrenade);

BEGIN_DATADESC(CSqueakGrenade)
	DEFINE_FIELD(m_flDie, FIELD_TIME),
	DEFINE_FIELD(m_vecTarget, FIELD_VECTOR),
	DEFINE_FIELD(m_flNextHunt, FIELD_TIME),
	DEFINE_FIELD(m_flNextHit, FIELD_TIME),
	DEFINE_FIELD(m_posPrev, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(m_hOwner, FIELD_EHANDLE),
	DEFINE_FUNCTION(SuperBounceTouch),
	DEFINE_FUNCTION(HuntThink),
END_DATADESC()

float CSqueakGrenade::m_flNextBounceSoundTime = 0;

int CSqueakGrenade::Classify(void)
{
	if (m_iClass) return m_iClass;

	if (m_iMyClass != 0)
		return m_iMyClass; // protect against recursion

	if (m_hEnemy != NULL)
	{
		m_iMyClass = CLASS_INSECT; // no one cares about it
		switch (m_hEnemy->Classify())
		{
			case CLASS_PLAYER:
			case CLASS_HUMAN_PASSIVE:
			case CLASS_HUMAN_MILITARY:
				m_iMyClass = 0;
				return CLASS_ALIEN_MILITARY; // barney's get mad, grunts get mad at it
		}
		m_iMyClass = 0;
	}

	return CLASS_ALIEN_BIOWEAPON;
}

void CSqueakGrenade::Spawn(void)
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_squeak.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, 0), Vector(4, 4, 8));

	SetTouch(&CSqueakGrenade::SuperBounceTouch);
	SetThink(&CSqueakGrenade::HuntThink);
	pev->nextthink = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time + 1E6;

	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_AIM;
	pev->health = gSkillData.snarkHealth;
	pev->gravity = 0.5;
	pev->friction = 0.5;

	pev->dmg = gSkillData.snarkDmgPop;

	m_flDie = gpGlobals->time + SQUEEK_DETONATE_DELAY;

	m_flFieldOfView = 0; // 180 degrees

	if (pev->owner)
		m_hOwner = Instance(pev->owner);

	m_flNextBounceSoundTime = gpGlobals->time;// reset each time a snark is spawned.

	pev->sequence = WSQUEAK_RUN;
	ResetSequenceInfo();
}

void CSqueakGrenade::Precache(void)
{
	PRECACHE_MODEL("models/w_squeak.mdl");
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
	PRECACHE_SOUND("common/bodysplat.wav");
	PRECACHE_SOUND("squeek/sqk_die1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt1.wav");
	PRECACHE_SOUND("squeek/sqk_hunt2.wav");
	PRECACHE_SOUND("squeek/sqk_hunt3.wav");
	PRECACHE_SOUND("squeek/sqk_deploy1.wav");
}

void CSqueakGrenade::Killed(entvars_t *pevAttacker, int iGib)
{
	pev->model = iStringNull;// make invisible
	SetThink(&CBaseEntity::SUB_Remove);
	SetTouch(NULL);
	pev->nextthink = gpGlobals->time + 0.1;

	// since squeak grenades never leave a body behind, clear out their takedamage now.
	// Squeaks do a bit of radius damage when they pop, and that radius damage will
	// continue to call this function unless we acknowledge the Squeak's death now. (sjb)
	pev->takedamage = DAMAGE_NO;

	// play squeek blast
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "squeek/sqk_blast1.wav", 1, 0.5, 0, PITCH_NORM);

	CSoundEnt::InsertSound(bits_SOUND_COMBAT, GetAbsOrigin(), SMALL_EXPLOSION_VOLUME, 3.0);

	UTIL_BloodDrips(GetAbsOrigin(), g_vecZero, BloodColor(), 80);

	if (m_hOwner != NULL)
		RadiusDamage(pev, m_hOwner->pev, pev->dmg, CLASS_NONE, DMG_BLAST);
	else
		RadiusDamage(pev, pev, pev->dmg, CLASS_NONE, DMG_BLAST);

	// reset owner so death message happens
	if (m_hOwner != NULL)
		pev->owner = m_hOwner->edict();

	CBaseMonster::Killed(pevAttacker, GIB_ALWAYS);
}

void CSqueakGrenade::GibMonster(void)
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/bodysplat.wav", 0.75, ATTN_NORM, 0, 200);
}

void CSqueakGrenade::HuntThink(void)
{
	// ALERT( at_console, "think\n" );

	if (!IsInWorld())
	{
		SetTouch(NULL);
		UTIL_Remove(this);
		return;
	}

	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	// explode when ready
	if (gpGlobals->time >= m_flDie)
	{
		g_vecAttackDir = GetAbsVelocity().Normalize();
		pev->health = -1;
		Killed(pev, 0);
		return;
	}

	// float
	if (pev->waterlevel != 0)
	{
		if (pev->movetype == MOVETYPE_BOUNCE)
		{
			pev->movetype = MOVETYPE_FLY;
		}
		Vector vecVelocity = GetAbsVelocity() * 0.9f;
		vecVelocity.z += 8.0f;
		SetAbsVelocity(vecVelocity);
	}
	else if (pev->movetype == MOVETYPE_FLY)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}

	// return if not time to hunt
	if (m_flNextHunt > gpGlobals->time)
		return;

	m_flNextHunt = gpGlobals->time + 2.0;

	CBaseEntity *pOther = NULL;
	Vector vecDir;
	TraceResult tr;

	Vector vecFlat = GetAbsVelocity();
	vecFlat.z = 0;
	vecFlat = vecFlat.Normalize();

	UTIL_MakeVectors(GetAbsAngles());

	if (m_hEnemy == NULL || !m_hEnemy->IsAlive())
	{
		// find target, bounce a bit towards it.
		Look(512);
		m_hEnemy = BestVisibleEnemy();
	}

	// squeek if it's about time blow up
	if ((m_flDie - gpGlobals->time <= 0.5) && (m_flDie - gpGlobals->time >= 0.3))
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_die1.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG(0, 0x3F));
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, GetAbsOrigin(), 256, 0.25);
	}

	// higher pitch as squeeker gets closer to detonation time
	float flpitch = 155.0 - 60.0 * ((m_flDie - gpGlobals->time) / SQUEEK_DETONATE_DELAY);
	if (flpitch < 80)
		flpitch = 80;

	if (m_hEnemy != NULL)
	{
		if (FVisible(m_hEnemy))
		{
			vecDir = m_hEnemy->EyePosition() - GetAbsOrigin();
			m_vecTarget = vecDir.Normalize();
		}

		float flVel = GetAbsVelocity().Length();
		float flAdj = 50.0 / (flVel + 10.0);

		if (flAdj > 1.2)
			flAdj = 1.2;

		// ALERT( at_console, "think : enemy\n");

		// ALERT( at_console, "%.0f %.2f %.2f %.2f\n", flVel, m_vecTarget.x, m_vecTarget.y, m_vecTarget.z );
		Vector vecVelocity = GetAbsVelocity();

		SetAbsVelocity(vecVelocity * flAdj + m_vecTarget * 300);
	}

	if (pev->flags & FL_ONGROUND)
	{
		SetLocalAvelocity(g_vecZero);
	}
	else
	{
		if (GetLocalAvelocity() == g_vecZero)
		{
			Vector vecAvelocity;
			vecAvelocity.x = RANDOM_FLOAT(-100, 100);
			vecAvelocity.z = RANDOM_FLOAT(-100, 100);
			vecAvelocity.y = 0;
			SetLocalAvelocity(vecAvelocity);
		}
	}

	if ((GetAbsOrigin() - m_posPrev).Length() < 1.0)
	{
		Vector vecVelocity;
		vecVelocity.x = RANDOM_FLOAT(-100, 100);
		vecVelocity.y = RANDOM_FLOAT(-100, 100);
		vecVelocity.z = 0;
		SetAbsVelocity(vecVelocity);
	}
	m_posPrev = GetAbsOrigin();

	Vector vecAngles = UTIL_VecToAngles(GetAbsVelocity());
	vecAngles.z = 0;
	vecAngles.x = 0;
	SetAbsAngles(vecAngles);
}

void CSqueakGrenade::SuperBounceTouch(CBaseEntity *pOther)
{
	float	flpitch;

	TraceResult tr = UTIL_GetGlobalTrace();

	// don't hit the guy that launched this grenade
	if (pev->owner && pOther->edict() == pev->owner)
		return;

	// at least until we've bounced once
	pev->owner = NULL;

	Vector vecAngles = GetAbsAngles();

	vecAngles.x = 0;
	vecAngles.z = 0;

	SetAbsAngles(vecAngles);

	// avoid bouncing too much
	if (m_flNextHit > gpGlobals->time)
		return;

	// higher pitch as squeeker gets closer to detonation time
	flpitch = 155.0 - 60.0 * ((m_flDie - gpGlobals->time) / SQUEEK_DETONATE_DELAY);

	if (pOther->pev->takedamage && m_flNextAttack < gpGlobals->time)
	{
		// attack!

		// make sure it's me who has touched them
		if (tr.pHit == pOther->edict())
		{
			// and it's not another squeakgrenade
			if (tr.pHit->v.modelindex != pev->modelindex)
			{
				// ALERT( at_console, "hit enemy\n");
				ClearMultiDamage();
				pOther->TraceAttack(pev, gSkillData.snarkDmgBite, gpGlobals->v_forward, &tr, DMG_SLASH);
				if (m_hOwner != NULL)
					ApplyMultiDamage(pev, m_hOwner->pev);
				else
					ApplyMultiDamage(pev, pev);

				pev->dmg += gSkillData.snarkDmgPop; // add more explosion damage
				// m_flDie += 2.0; // add more life

				// make bite sound
				EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "squeek/sqk_deploy1.wav", 1.0, ATTN_NORM, 0, (int)flpitch);
				m_flNextAttack = gpGlobals->time + 0.5;
			}
		}
		else
		{
			// ALERT( at_console, "been hit\n");
		}
	}

	m_flNextHit = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time;

	if (g_pGameRules->IsMultiplayer())
	{
		// in multiplayer, we limit how often snarks can make their bounce sounds to prevent overflows.
		if (gpGlobals->time < m_flNextBounceSoundTime)
		{
			// too soon!
			return;
		}
	}

	if (!(pev->flags & FL_ONGROUND))
	{
		// play bounce sound
		float flRndSound = RANDOM_FLOAT(0, 1);

		if (flRndSound <= 0.33)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt1.wav", 1, ATTN_NORM, 0, (int)flpitch);
		else if (flRndSound <= 0.66)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, (int)flpitch);
		else
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, (int)flpitch);
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, GetAbsOrigin(), 256, 0.25);
	}
	else
	{
		// skittering sound
		CSoundEnt::InsertSound(bits_SOUND_COMBAT, GetAbsOrigin(), 100, 0.1);
	}

	m_flNextBounceSoundTime = gpGlobals->time + 0.5;// half second.
}