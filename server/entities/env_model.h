/*
env_model.h - entity that represents animated studiomodel
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

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"

//=================================================================
// env_model: like env_sprite, except you can specify a sequence.
//=================================================================
#define SF_ENVMODEL_OFF				1
#define SF_ENVMODEL_DROPTOFLOOR		2
#define SF_ENVMODEL_SOLID			4
#define SF_ENVMODEL_NEWLIGHTING		8
#define SF_ENVMODEL_NODYNSHADOWS	16

class CEnvModel : public CBaseAnimating
{
	DECLARE_CLASS(CEnvModel, CBaseAnimating);
public:
	void Spawn();
	void Precache();
	void EXPORT Think();
	void KeyValue(KeyValueData *pkvd);
	void HandleAnimEvent(MonsterEvent_t *pEvent);
	STATE GetState();
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	virtual int	ObjectCaps() { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void SetSequence();
	void AutoSetSize();

	DECLARE_DATADESC();

	string_t m_iszSequence_On;
	string_t m_iszSequence_Off;
	int m_iAction_On;
	int m_iAction_Off;
};
