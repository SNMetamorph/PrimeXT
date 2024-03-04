/*
env_postfx_controller.cpp - entity for manipulating player post-FX parameters
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "env_postfx_controller.h"
#include "user_messages.h"

LINK_ENTITY_TO_CLASS( env_postfx_controller, CEnvPostFxController );

BEGIN_DATADESC( CEnvPostFxController )
	DEFINE_FIELD( m_flFadeInTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBrightness, FIELD_FLOAT ),
	DEFINE_FIELD( m_flSaturation, FIELD_FLOAT ),
	DEFINE_FIELD( m_flContrast, FIELD_FLOAT ),
	DEFINE_FIELD( m_flRedLevel, FIELD_FLOAT ),
	DEFINE_FIELD( m_flGreenLevel, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBlueLevel, FIELD_FLOAT ),
	DEFINE_FIELD( m_flVignetteScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFilmGrainScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_flColorAccentScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecAccentColor, FIELD_VECTOR ),
END_DATADESC()

void CEnvPostFxController::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->frame = 0;
	m_vecAccentColor = Vector(
		pev->rendercolor.x / 255.f,
		pev->rendercolor.y / 255.f,
		pev->rendercolor.z / 255.f
	);
}

void CEnvPostFxController::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "brightness"))
	{
		m_flBrightness = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "saturation"))
	{
		m_flSaturation = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "contrast"))
	{
		m_flContrast = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "red_level"))
	{
		m_flRedLevel = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "green_level"))
	{
		m_flGreenLevel = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "blue_level"))
	{
		m_flBlueLevel = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "vignette_scale"))
	{
		m_flVignetteScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "film_grain_scale"))
	{
		m_flFilmGrainScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "color_accent_scale"))
	{
		m_flColorAccentScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fade_in_time"))
	{
		m_flFadeInTime = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else {
		CPointEntity::KeyValue(pkvd);
	}
}

void CEnvPostFxController::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	bool skipMessage = false;
	bool fadeOut = false;

	if (pev->spawnflags & SF_POSTFX_CONTROLLER_OUT) {
		fadeOut = true;
	}

	if (pev->spawnflags & SF_POSTFX_CONTROLLER_ONLYONE)
	{
		if (pActivator->IsNetClient()) {
			MESSAGE_BEGIN(MSG_ONE, gmsgPostFxSettings, NULL, pActivator->edict());
		}
		else {
			skipMessage = true;
		}
	}
	else {
		MESSAGE_BEGIN(MSG_ALL, gmsgPostFxSettings);
	}

	if (!skipMessage)
	{
		WRITE_FLOAT(m_flFadeInTime);
		WRITE_FLOAT(fadeOut ? 0.0f : m_flBrightness);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flSaturation);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flContrast);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flRedLevel);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flGreenLevel);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flBlueLevel);
		WRITE_FLOAT(fadeOut ? 0.0f : m_flVignetteScale);
		WRITE_FLOAT(fadeOut ? 0.0f : m_flFilmGrainScale);
		WRITE_FLOAT(fadeOut ? 0.0f : m_flColorAccentScale);
		WRITE_FLOAT(fadeOut ? 1.0f : m_vecAccentColor.x);
		WRITE_FLOAT(fadeOut ? 1.0f : m_vecAccentColor.y);
		WRITE_FLOAT(fadeOut ? 1.0f : m_vecAccentColor.z);
		MESSAGE_END();
	}

	SUB_UseTargets( this, USE_TOGGLE, 0 );
}
