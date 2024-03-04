/*
env_model.cpp - entity that represents animated studiomodel
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "env_model.h"
#include "scriptevent.h"

LINK_ENTITY_TO_CLASS(env_model, CEnvModel);

BEGIN_DATADESC(CEnvModel)
	DEFINE_KEYFIELD(m_iszSequence_On, FIELD_STRING, "m_iszSequence_On"),
	DEFINE_KEYFIELD(m_iszSequence_Off, FIELD_STRING, "m_iszSequence_Off"),
	DEFINE_KEYFIELD(m_iAction_On, FIELD_INTEGER, "m_iAction_On"),
	DEFINE_KEYFIELD(m_iAction_Off, FIELD_INTEGER, "m_iAction_Off"),
END_DATADESC()

void CEnvModel::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszSequence_On"))
	{
		m_iszSequence_On = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSequence_Off"))
	{
		m_iszSequence_Off = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_On"))
	{
		m_iAction_On = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_Off"))
	{
		m_iAction_Off = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue(pkvd);
	}
}

void CEnvModel::Spawn(void)
{
	Precache();
	SET_MODEL(edict(), STRING(pev->model));
	RelinkEntity(true);

	SetBoneController(0, 0);
	SetBoneController(1, 0);
	SetSequence();

	if (FBitSet(pev->spawnflags, SF_ENVMODEL_SOLID))
	{
		if (UTIL_AllowHitboxTrace(this))
			pev->solid = SOLID_BBOX;
		else 
			pev->solid = SOLID_SLIDEBOX;
		AutoSetSize();
	}

	if (FBitSet(pev->spawnflags, SF_ENVMODEL_DROPTOFLOOR))
	{
		Vector origin = GetLocalOrigin();
		origin.z += 1;
		SetLocalOrigin(origin);
		UTIL_DropToFloor(this);
	}

	if (FBitSet(pev->spawnflags, SF_ENVMODEL_NEWLIGHTING) && !m_hParent.Get())
	{
		// tell the client about static entity
		SetBits(pev->iuser1, CF_STATIC_ENTITY);
	}

	if (FBitSet(pev->spawnflags, SF_ENVMODEL_NODYNSHADOWS))
	{
		SetBits(pev->effects, EF_NOSHADOW);
	}

	SetNextThink(0.1);
}

void CEnvModel::Precache(void)
{
	PRECACHE_MODEL(GetModel());
}

void CEnvModel::HandleAnimEvent(MonsterEvent_t *pEvent) 
{
	switch (pEvent->event)
	{
		case SCRIPT_EVENT_SOUND:			// Play a named wave file
			EMIT_SOUND(edict(), CHAN_BODY, pEvent->options, 1.0, ATTN_IDLE);
			break;
		case SCRIPT_EVENT_SOUND_VOICE:
			EMIT_SOUND(edict(), CHAN_VOICE, pEvent->options, 1.0, ATTN_IDLE);
			break;
		case SCRIPT_EVENT_SENTENCE_RND1:		// Play a named sentence group 33% of the time
			if (RANDOM_LONG(0, 2) == 0)
				break;
			// fall through...
		case SCRIPT_EVENT_SENTENCE:			// Play a named sentence group
			SENTENCEG_PlayRndSz(edict(), pEvent->options, 1.0, ATTN_IDLE, 0, 100);
			break;
		case SCRIPT_EVENT_FIREEVENT:		// Fire a trigger
			UTIL_FireTargets(pEvent->options, this, this, USE_TOGGLE, 0);
			break;
		case MONSTER_EVENT_AURORA_PARTICLE:
			UTIL_CreateAuroraSystem(NULL, this, pEvent->options, 0);
			break;
		default:
			ALERT(at_aiconsole, "Unhandled animation event %d for %s\n", pEvent->event, STRING(pev->classname));
			break;
	}
}

STATE CEnvModel::GetState(void)
{
	if (pev->spawnflags & SF_ENVMODEL_OFF)
		return STATE_OFF;
	else
		return STATE_ON;
}

void CEnvModel::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if (ShouldToggle(useType, !(pev->spawnflags & SF_ENVMODEL_OFF)))
	{
		if (pev->spawnflags & SF_ENVMODEL_OFF)
			pev->spawnflags &= ~SF_ENVMODEL_OFF;
		else
			pev->spawnflags |= SF_ENVMODEL_OFF;

		SetSequence();
		SetNextThink(0.1);
	}
}

void CEnvModel::Think()
{
	float flInterval = StudioFrameAdvance(); // set m_fSequenceFinished if necessary
	DispatchAnimEvents(flInterval);

//	if (m_fSequenceLoops)
//	{
//		SetNextThink( 1E6 );
//		return; // our work here is done.
//	}
	if (m_fSequenceFinished && !m_fSequenceLoops)
	{
		int iTemp;

		if (pev->spawnflags & SF_ENVMODEL_OFF)
			iTemp = m_iAction_Off;
		else
			iTemp = m_iAction_On;

		switch (iTemp)
		{
			//		case 1: // loop
			//			pev->animtime = gpGlobals->time;
			//			m_fSequenceFinished = FALSE;
			//			m_flLastEventCheck = gpGlobals->time;
			//			pev->frame = 0;
			//			break;
			case 2: // change state
				if (pev->spawnflags & SF_ENVMODEL_OFF)
					pev->spawnflags &= ~SF_ENVMODEL_OFF;
				else
					pev->spawnflags |= SF_ENVMODEL_OFF;
				SetSequence();
				break;
			default: //remain frozen
				return;
		}
	}
	SetNextThink(0.1);
}

void CEnvModel::SetSequence(void)
{
	int iszSequence;

	if (pev->spawnflags & SF_ENVMODEL_OFF)
		iszSequence = m_iszSequence_Off;
	else
		iszSequence = m_iszSequence_On;

	if (!iszSequence) {
		pev->sequence = 0;
	}
	else {
		pev->sequence = LookupSequence(STRING(iszSequence));
	}

	if (pev->sequence == -1)
	{
		if (pev->targetname)
			ALERT(at_error, "env_model %s: unknown sequence \"%s\"\n", STRING(pev->targetname), STRING(iszSequence));
		else
			ALERT(at_error, "env_model: unknown sequence \"%s\"\n", STRING(pev->targetname), STRING(iszSequence));
		pev->sequence = 0;
	}

	pev->frame = 0;
	ResetSequenceInfo();

	if (pev->spawnflags & SF_ENVMODEL_OFF)
	{
		if (m_iAction_Off == 1)
			m_fSequenceLoops = 1;
		else
			m_fSequenceLoops = 0;
	}
	else
	{
		if (m_iAction_On == 1)
			m_fSequenceLoops = 1;
		else
			m_fSequenceLoops = 0;
	}
}

// automatically set collision box
void CEnvModel::AutoSetSize()
{
	studiohdr_t *pstudiohdr;
	pstudiohdr = (studiohdr_t *)GET_MODEL_PTR(edict());

	if (pstudiohdr == NULL)
	{
		UTIL_SetSize(pev, Vector(-10, -10, -10), Vector(10, 10, 10));
		ALERT(at_error, "CEnvModel::AutoSetSize: unable to fetch model pointer\n");
		return;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	UTIL_SetSize(pev, pseqdesc[pev->sequence].bbmin, pseqdesc[pev->sequence].bbmax);
}
