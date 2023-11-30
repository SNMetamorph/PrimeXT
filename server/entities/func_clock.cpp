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

#include "func_clock.h"

#define SECONDS_PER_MINUTE	60
#define SECONDS_PER_HOUR	3600
#define SECODNS_PER_DAY	43200

LINK_ENTITY_TO_CLASS(func_clock, CFuncClock);

BEGIN_DATADESC(CFuncClock)
	DEFINE_FIELD(m_vecCurtime, FIELD_VECTOR),
	DEFINE_FIELD(m_vecFinaltime, FIELD_VECTOR),
	DEFINE_FIELD(m_flCurTime, FIELD_FLOAT),
	DEFINE_FIELD(m_iClockType, FIELD_INTEGER),
	DEFINE_FIELD(m_iHoursCount, FIELD_INTEGER),
	DEFINE_FIELD(m_fInit, FIELD_BOOLEAN),
END_DATADESC()

void CFuncClock::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		switch (Q_atoi(pkvd->szValue))
		{
			case 1:
				m_iClockType = SECONDS_PER_HOUR;
				break;
			case 2:
				m_iClockType = SECODNS_PER_DAY;
				break;
			default:
				m_iClockType = SECONDS_PER_MINUTE;
				break;
		}
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "curtime"))
	{
		m_vecCurtime = Q_atov(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "event"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue(pkvd);
}

void CFuncClock::Spawn(void)
{
	CBaseToggle::AxisDir(pev);

	m_iState = STATE_ON;
	pev->solid = SOLID_NOT;

	SET_MODEL(edict(), GetModel());

	if (m_iClockType == SECODNS_PER_DAY)
	{
		// normalize our time
		if (m_vecCurtime.x > 11) m_vecCurtime.x = 0;
		if (m_vecCurtime.y > 59) m_vecCurtime.y = 0;
		if (m_vecCurtime.z > 59) m_vecCurtime.z = 0;

		// member full hours
		m_iHoursCount = m_vecCurtime.x;

		// calculate seconds
		m_vecFinaltime.z = m_vecCurtime.z * (SECONDS_PER_MINUTE / 60);
		m_vecFinaltime.y = m_vecCurtime.y * (SECONDS_PER_HOUR / 60) + m_vecFinaltime.z;
		m_vecFinaltime.x = m_vecCurtime.x * (SECODNS_PER_DAY / 12) + m_vecFinaltime.y;
	}
}

void CFuncClock::Activate(void)
{
	if (m_fInit) return;

	if (m_iClockType == SECODNS_PER_DAY && m_vecCurtime != g_vecZero)
	{
		// try to find minutes and seconds entity
		CBaseEntity *pEntity = NULL;
		while ((pEntity = UTIL_FindEntityInSphere(pEntity, GetLocalOrigin(), pev->size.z)))
		{
			if (FClassnameIs(pEntity, "func_clock"))
			{
				CFuncClock *pClock = (CFuncClock *)pEntity;
				// write start hours, minutes and seconds
				switch (pClock->m_iClockType)
				{
					case SECODNS_PER_DAY:
						// NOTE: here we set time for himself through FindEntityInSphere
						pClock->m_flCurTime = m_vecFinaltime.x;
						break;
					case SECONDS_PER_HOUR:
						pClock->m_flCurTime = m_vecFinaltime.y;
						break;
					default:
						pClock->m_flCurTime = m_vecFinaltime.z;
						break;
				}
			}
		}
	}

	// clock start	
	SetNextThink(0);
	m_fInit = 1;
}

void CFuncClock::Think(void)
{
	float seconds, ang, pos;

	seconds = gpGlobals->time + m_flCurTime;
	pos = seconds / m_iClockType;
	pos = pos - floor(pos);
	ang = 360 * pos;

	SetLocalAngles(pev->movedir * ang);

	if (m_iClockType == SECODNS_PER_DAY)
	{
		int hours = GetLocalAngles().Length() / 30;
		if (m_iHoursCount != hours)
		{
			// member new hour
			m_iHoursCount = hours;
			if (hours == 0) hours = 12;	// merge for 0.00.00

			// send hours info
			UTIL_FireTargets(pev->netname, this, this, USE_SET, hours);
			UTIL_FireTargets(pev->netname, this, this, USE_ON);
		}
	}

	RelinkEntity(FALSE);

	// set clock resolution
	SetNextThink(1.0f);
}