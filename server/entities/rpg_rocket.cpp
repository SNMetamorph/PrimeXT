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

#include "rpg_rocket.h"

LINK_ENTITY_TO_CLASS(rpg_rocket, CRpgRocket);

BEGIN_DATADESC(CRpgRocket)
	DEFINE_FIELD(m_flIgniteTime, FIELD_TIME),
	DEFINE_FIELD(m_pLauncher, FIELD_CLASSPTR),
	DEFINE_FUNCTION(FollowThink),
	DEFINE_FUNCTION(IgniteThink),
	DEFINE_FUNCTION(RocketTouch),
END_DATADESC()


//=========================================================
//=========================================================
CRpgRocket *CRpgRocket::CreateRpgRocket(Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CRpg *pLauncher)
{
	CRpgRocket *pRocket = GetClassPtr((CRpgRocket *)NULL);

	UTIL_SetOrigin(pRocket, vecOrigin);
	pRocket->SetLocalAngles(vecAngles);
	pRocket->Spawn();
	pRocket->SetTouch(&CRpgRocket::RocketTouch);
	pRocket->m_pLauncher = pLauncher;// remember what RPG fired me. 
	pRocket->m_pLauncher->AddActiveRocket();// register this missile as active for the launcher
	pRocket->pev->owner = pOwner->edict();

	return pRocket;
}

//=========================================================
//=========================================================
void CRpgRocket::Spawn(void)
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	RelinkEntity(TRUE);

	pev->classname = MAKE_STRING("rpg_rocket");

	SetThink(&CRpgRocket::IgniteThink);
	SetTouch(&CGrenade::ExplodeTouch);

	Vector angles = GetLocalAngles();

	angles.x -= 30;
	UTIL_MakeVectors(angles);

	SetLocalVelocity(gpGlobals->v_forward * 250);
	pev->gravity = 0.5;

	SetNextThink(0.4);

	pev->dmg = gSkillData.plrDmgRPG;
}

//=========================================================
//=========================================================
void CRpgRocket::RocketTouch(CBaseEntity *pOther)
{
	if (m_pLauncher)
	{
		// my launcher is still around, tell it I'm dead.
		m_pLauncher->RemoveActiveRocket();
	}

	STOP_SOUND(edict(), CHAN_VOICE, "weapons/rocket1.wav");
	ExplodeTouch(pOther);
}

//=========================================================
//=========================================================
void CRpgRocket::Precache(void)
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND("weapons/rocket1.wav");
}

void CRpgRocket::CreateTrail(void)
{
	// rocket trail
	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);

	WRITE_BYTE(TE_BEAMFOLLOW);
	WRITE_SHORT(entindex());	// entity
	WRITE_SHORT(m_iTrail);	// model
	WRITE_BYTE(40); // life
	WRITE_BYTE(5);  // width
	WRITE_BYTE(224);   // r, g, b
	WRITE_BYTE(224);   // r, g, b
	WRITE_BYTE(255);   // r, g, b
	WRITE_BYTE(255);	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
}

void CRpgRocket::IgniteThink(void)
{
	// pev->movetype = MOVETYPE_TOSS;

	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND(edict(), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5);

	CreateTrail();

	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink(&CRpgRocket::FollowThink);
	SetNextThink(0.1);
}

void CRpgRocket::OnTeleport(void)
{
	// re-aiming the projectile
	SetLocalAngles(UTIL_VecToAngles(GetLocalVelocity()));

	MESSAGE_BEGIN(MSG_ALL, SVC_TEMPENTITY);
	WRITE_BYTE(TE_KILLBEAM);
	WRITE_ENTITY(entindex());
	MESSAGE_END();

	CreateTrail();
}

void CRpgRocket::FollowThink(void)
{
	CBaseEntity *pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;

	Vector angles = GetLocalAngles();
	UTIL_MakeVectors(angles);

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;

	// Examine all entities within a reasonable radius
	while ((pOther = UTIL_FindEntityByClassname(pOther, "laser_spot")) != NULL)
	{
		UTIL_TraceLine(GetAbsOrigin(), pOther->GetAbsOrigin(), dont_ignore_monsters, ENT(pev), &tr);
		// ALERT( at_console, "%f\n", tr.flFraction );
		if (tr.flFraction >= 0.90)
		{
			vecDir = pOther->GetAbsOrigin() - GetAbsOrigin();
			flDist = vecDir.Length();
			vecDir = vecDir.Normalize();
			flDot = DotProduct(gpGlobals->v_forward, vecDir);
			if ((flDot > 0) && (flDist * (1 - flDot) < flMax))
			{
				flMax = flDist * (1 - flDot);
				vecTarget = vecDir;
			}
		}
	}

	SetLocalAngles(UTIL_VecToAngles(vecTarget));

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = GetLocalVelocity().Length();

	if (gpGlobals->time - m_flIgniteTime < 1.0)
	{
		SetLocalVelocity(GetLocalVelocity() * 0.2 + vecTarget * (flSpeed * 0.8 + 400));
		if (pev->waterlevel == 3)
		{
			// go slow underwater
			if (GetLocalVelocity().Length() > 300)
			{
				SetLocalVelocity(GetLocalVelocity().Normalize() * 300);
			}
			UTIL_BubbleTrail(GetAbsOrigin() - GetAbsVelocity() * 0.1, GetAbsOrigin(), 4);
		}
		else
		{
			if (GetLocalVelocity().Length() > 2000)
			{
				SetLocalVelocity(GetLocalVelocity().Normalize() * 2000);
			}
		}
	}
	else
	{
		if (pev->effects & EF_LIGHT)
		{
			pev->effects = 0;
			STOP_SOUND(ENT(pev), CHAN_VOICE, "weapons/rocket1.wav");
		}

		SetLocalVelocity(GetLocalVelocity() * 0.2 + vecTarget * flSpeed * 0.798);

		if (pev->waterlevel == 0 && GetLocalVelocity().Length() < 1500)
		{
			Detonate();
		}
	}
	// ALERT( at_console, "%.0f\n", flSpeed );

	SetNextThink(0.1);
}