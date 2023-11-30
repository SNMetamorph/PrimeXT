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

#include "func_pendulum.h"

LINK_ENTITY_TO_CLASS(func_pendulum, CPendulum);

BEGIN_DATADESC(CPendulum)
	DEFINE_FIELD(m_accel, FIELD_FLOAT),
	DEFINE_KEYFIELD(m_distance, FIELD_FLOAT, "distance"),
	DEFINE_FIELD(m_time, FIELD_TIME),
	DEFINE_FIELD(m_damp, FIELD_FLOAT),
	DEFINE_FIELD(m_maxSpeed, FIELD_FLOAT),
	DEFINE_FIELD(m_dampSpeed, FIELD_FLOAT),
	DEFINE_FIELD(m_center, FIELD_VECTOR),
	DEFINE_FIELD(m_start, FIELD_VECTOR),
	DEFINE_FUNCTION(Swing),
	DEFINE_FUNCTION(Stop),
	DEFINE_FUNCTION(RopeTouch),
END_DATADESC()

void CPendulum::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_distance = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damp"))
	{
		m_damp = Q_atof(pkvd->szValue) * 0.001;
		pkvd->fHandled = TRUE;
	}
	else CBaseDelay::KeyValue(pkvd);
}

void CPendulum::Spawn(void)
{
	// set the axis of rotation
	CBaseToggle::AxisDir(pev);

	if (FBitSet(pev->spawnflags, SF_PENDULUM_PASSABLE))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;

	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL(edict(), GetModel());

	if (m_distance == 0)
	{
		ALERT(at_error, "%s [%i] has invalid distance\n", GetClassname(), entindex());
		return;
	}

	if (pev->speed == 0)
		pev->speed = 100;

	SetLocalAngles(m_vecTempAngles);

	// calculate constant acceleration from speed and distance
	m_accel = (pev->speed * pev->speed) / (2 * fabs(m_distance));
	m_maxSpeed = pev->speed;
	m_start = GetLocalAngles();
	m_center = GetLocalAngles() + (m_distance * 0.5f) * pev->movedir;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity(this);

	if (FBitSet(pev->spawnflags, SF_PENDULUM_START_ON))
	{
		SetThink(&CBaseEntity::SUB_CallUseToggle);
		SetNextThink(0.1);
	}
	pev->speed = 0;

	if (FBitSet(pev->spawnflags, SF_PENDULUM_SWING))
	{
		SetTouch(&CPendulum::RopeTouch);
	}
}

void CPendulum::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator) || m_distance == 0)
		return;

	if (pev->speed)
	{
		// pendulum is moving, stop it and auto-return if necessary
		if (FBitSet(pev->spawnflags, SF_PENDULUM_AUTO_RETURN))
		{
			float delta = CBaseToggle::AxisDelta(pev->spawnflags, GetLocalAngles(), m_start);

			SetLocalAvelocity(m_maxSpeed * pev->movedir);
			SetNextThink(delta / m_maxSpeed);
			SetThink(&CPendulum::Stop);
		}
		else
		{
			pev->speed = 0; // dead stop
			SetThink(NULL);
			SetLocalAvelocity(g_vecZero);
			m_iState = STATE_OFF;
		}
	}
	else
	{
		SetNextThink(0.1f);		// start the pendulum moving
		SetThink(&CPendulum::Swing);
		m_time = gpGlobals->time;		// save time to calculate dt
		m_dampSpeed = m_maxSpeed;
	}
}

void CPendulum::Stop(void)
{
	SetLocalAngles(m_start);
	pev->speed = 0; // dead stop
	SetThink(NULL);
	SetLocalAvelocity(g_vecZero);
	m_iState = STATE_OFF;
}

void CPendulum::Blocked(CBaseEntity *pOther)
{
	m_time = gpGlobals->time;
}

void CPendulum::Swing(void)
{
	float delta, dt;

	delta = CBaseToggle::AxisDelta(pev->spawnflags, GetLocalAngles(), m_center);

	dt = gpGlobals->time - m_time;	// how much time has passed?
	m_time = gpGlobals->time;		// remember the last time called

	// integrate velocity
	if (delta > 0 && m_accel > 0)
		pev->speed -= m_accel * dt;
	else pev->speed += m_accel * dt;

	pev->speed = bound(-m_maxSpeed, pev->speed, m_maxSpeed);

	m_iState = STATE_ON;

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalAvelocity(pev->speed * pev->movedir);

	// call this again
	SetNextThink(0.1f);
	SetMoveDoneTime(0.1f);

	if (m_damp)
	{
		m_dampSpeed -= m_damp * m_dampSpeed * dt;

		if (m_dampSpeed < 30.0)
		{
			SetLocalAngles(m_center);
			pev->speed = 0;
			SetThink(NULL);
			SetLocalAvelocity(g_vecZero);
			m_iState = STATE_OFF;
		}
		else if (pev->speed > m_dampSpeed)
		{
			pev->speed = m_dampSpeed;
		}
		else if (pev->speed < -m_dampSpeed)
		{
			pev->speed = -m_dampSpeed;
		}
	}
}

void CPendulum::Touch(CBaseEntity *pOther)
{
	if (pev->dmg <= 0)
		return;

	// we can't hurt this thing, so we're not concerned with it
	if (!pOther->pev->takedamage)
		return;

	// calculate damage based on rotation speed
	float damage = pev->dmg * pev->speed * 0.01;

	if (damage < 0) damage = -damage;

	pOther->TakeDamage(pev, pev, damage, DMG_CRUSH);

	Vector vNewVel = (pOther->GetAbsOrigin() - GetAbsOrigin()).Normalize();
	pOther->SetAbsVelocity(vNewVel * damage);
}

void CPendulum::RopeTouch(CBaseEntity *pOther)
{
	if (!pOther->IsPlayer())
	{
		// not a player!
		ALERT(at_console, "Not a client\n");
		return;
	}

	if (pOther->edict() == pev->enemy)
	{
		// this player already on the rope.
		return;
	}

	pev->enemy = pOther->edict();
	pOther->SetAbsVelocity(g_vecZero);
	pOther->pev->movetype = MOVETYPE_NONE;
}
