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

#include "func_light.h"

LINK_ENTITY_TO_CLASS(func_light, CFuncLight);

BEGIN_DATADESC(CFuncLight)
	DEFINE_FIELD(m_iFlickerMode, FIELD_INTEGER),
	DEFINE_FIELD(m_flNextFlickerTime, FIELD_FLOAT),
	DEFINE_FIELD(m_vecLastDmgPoint, FIELD_POSITION_VECTOR),
	DEFINE_FUNCTION(Flicker),
END_DATADESC()

void CFuncLight::Spawn(void)
{
	m_Material = matGlass;

	CBreakable::Spawn();
	SET_MODEL(edict(), GetModel());

	// probably map compiler haven't func_light support
	if (m_iStyle <= 0 || m_iStyle >= 256)
	{
		if (GetTargetname()[0])
			ALERT(at_error, "%s with name %s has bad lightstyle %i. Disabled\n", GetClassname(), GetTargetname(), m_iStyle);
		else ALERT(at_error, "%s [%i] has bad lightstyle %i. Disabled\n", GetClassname(), entindex(), m_iStyle);
		m_iState = STATE_DEAD; // lamp is dead
	}

	if (FBitSet(pev->spawnflags, SF_LIGHT_START_ON))
		Use(this, this, USE_ON, 0);
	else Use(this, this, USE_OFF, 0);

	if (pev->health <= 0)
		pev->takedamage = DAMAGE_NO;
	else pev->takedamage = DAMAGE_YES;
}

void CFuncLight::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	m_hActivator = pActivator;

	if (IsLockedByMaster())
		return;

	if (m_iState == STATE_DEAD)
		return; // lamp is broken

	if (useType == USE_TOGGLE)
	{
		if (m_iState == STATE_OFF)
			useType = USE_ON;
		else useType = USE_OFF;
	}

	if (useType == USE_ON)
	{
		if (m_flDelay)
		{
			// make flickering delay
			m_iState = STATE_TURN_ON;
			LIGHT_STYLE(m_iStyle, "mmamammmmammamamaaamammma");
			pev->frame = 0; // light texture is on
			SetThink(&CFuncLight::Flicker);
			SetNextThink(m_flDelay);
		}
		else
		{         // instant enable
			m_iState = STATE_ON;
			LIGHT_STYLE(m_iStyle, "k");
			pev->frame = 0; // light texture is on
			UTIL_FireTargets(pev->target, this, this, USE_ON);
		}
	}
	else if (useType == USE_OFF)
	{
		LIGHT_STYLE(m_iStyle, "a");
		UTIL_FireTargets(pev->target, this, this, USE_OFF);
		pev->frame = 1;// light texture is off
		m_iState = STATE_OFF;
	}
	else if (useType == USE_SET)
	{
		// a script die (dramatic effect)
		Die();
	}
}

void CFuncLight::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if (pev->takedamage == DAMAGE_NO)
		return;

	if (m_iState == STATE_DEAD)
		return;

	const char *pTextureName = NULL;
	Vector start = pevAttacker->origin + pevAttacker->view_ofs;
	Vector end = start + vecDir * 1024;
	edict_t *pHit = ptr->pHit;

	if (pHit)
		pTextureName = TRACE_TEXTURE(pHit, start, end);

	if (pTextureName != NULL && (!Q_strncmp(pTextureName, "+0~", 3) || *pTextureName == '~'))
	{
		// take damage only at light texture
		UTIL_Sparks(ptr->vecEndPos);
		m_vecLastDmgPoint = ptr->vecEndPos;
	}

	CBreakable::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

int CFuncLight::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (bitsDamageType & DMG_BLAST)
		flDamage *= 3.0f;
	else if (bitsDamageType & DMG_CLUB)
		flDamage *= 2.0f;
	else if (bitsDamageType & DMG_SONIC)
		flDamage *= 1.2f;
	else if (bitsDamageType & DMG_SHOCK)
		flDamage *= 10.0f;//!!!! over voltage
	else if (bitsDamageType & DMG_BULLET)
		flDamage *= 0.5f; // half damage at bullet

	pev->health -= flDamage;

	if (pev->health <= 0.0f)
	{
		Die();
		return 0;
	}

	CBreakable::DamageSound();

	return 1;
}

void CFuncLight::Die(void)
{
	// lamp is random choose die style
	if (m_iState == STATE_OFF)
	{
		pev->frame = 1; // light texture is off
		LIGHT_STYLE(m_iStyle, "a");
		DontThink();
	}
	else
	{         // simple randomization
		m_iFlickerMode = RANDOM_LONG(1, 2);
		SetThink(&CFuncLight::Flicker);
		SetNextThink(0.1f + RANDOM_LONG(0.1f, 0.2f));
	}

	m_iState = STATE_DEAD;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;
	UTIL_FireTargets(pev->target, this, this, USE_OFF);

	switch (RANDOM_LONG(0, 1))
	{
		case 0:
			EMIT_SOUND(edict(), CHAN_VOICE, "debris/bustglass1.wav", 0.7, ATTN_IDLE);
			break;
		case 1:
			EMIT_SOUND(edict(), CHAN_VOICE, "debris/bustglass2.wav", 0.8, ATTN_IDLE);
			break;
	}

	Vector vecSpot = GetAbsOrigin() + (pev->mins + pev->maxs) * 0.5f;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, vecSpot);
	WRITE_BYTE(TE_BREAKMODEL);
	WRITE_COORD(vecSpot.x);
	WRITE_COORD(vecSpot.y);
	WRITE_COORD(vecSpot.z);
	WRITE_COORD(pev->size.x);
	WRITE_COORD(pev->size.y);
	WRITE_COORD(pev->size.z);
	WRITE_COORD(0);
	WRITE_COORD(0);
	WRITE_COORD(0);
	WRITE_BYTE(10);
	WRITE_SHORT(m_idShard);
	WRITE_BYTE(0);
	WRITE_BYTE(25);
	WRITE_BYTE(BREAK_GLASS);
	MESSAGE_END();
}

void CFuncLight::Flicker(void)
{
	if (m_iState == STATE_TURN_ON)
	{
		LIGHT_STYLE(m_iStyle, "k");
		UTIL_FireTargets(pev->target, this, this, USE_ON);
		m_iState = STATE_ON;
		DontThink();
		return;
	}

	if (m_iFlickerMode == 1)
	{
		pev->frame = 1;
		LIGHT_STYLE(m_iStyle, "a");
		SetThink(NULL);
		return;
	}

	if (m_iFlickerMode == 2)
	{
		switch (RANDOM_LONG(0, 3))
		{
			case 0:
				LIGHT_STYLE(m_iStyle, "abcaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
				break;
			case 1:
				LIGHT_STYLE(m_iStyle, "acaaabaaaaaaaaaaaaaaaaaaaaaaaaaaa");
				break;
			case 2:
				LIGHT_STYLE(m_iStyle, "aaafbaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
				break;
			case 3:
				LIGHT_STYLE(m_iStyle, "aaaaaaaaaaaaagaaaaaaaaacaaaacaaaa");
				break;
		}

		m_flNextFlickerTime = RANDOM_FLOAT(0.5f, 10.0f);
		UTIL_Sparks(m_vecLastDmgPoint);

		switch (RANDOM_LONG(0, 2))
		{
			case 0:
				EMIT_SOUND(edict(), CHAN_VOICE, "buttons/spark1.wav", 0.4, ATTN_IDLE);
				break;
			case 1:
				EMIT_SOUND(edict(), CHAN_VOICE, "buttons/spark2.wav", 0.3, ATTN_IDLE);
				break;
			case 2:
				EMIT_SOUND(edict(), CHAN_VOICE, "buttons/spark3.wav", 0.35, ATTN_IDLE);
				break;
		}

		if (m_flNextFlickerTime > 6.5f)
			m_iFlickerMode = 1; // stop sparking
	}

	SetNextThink(m_flNextFlickerTime);
}