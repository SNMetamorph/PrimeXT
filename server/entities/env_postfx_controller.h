/*
env_postfx_controller.h - entity for manipulating player post-FX parameters
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

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define SF_POSTFX_CONTROLLER_OUT		0x0001		// Fade out
#define SF_POSTFX_CONTROLLER_ONLYONE	0x0002      // Setting applies only for activator, not for all players

class CEnvPostFxController : public CPointEntity
{
	DECLARE_CLASS( CEnvPostFxController, CPointEntity );
public:
	void	Spawn();
	void	Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	void	KeyValue(KeyValueData *pkvd);

private:
	DECLARE_DATADESC();

	float	m_flFadeInTime;
	float	m_flBrightness;
	float	m_flSaturation;
	float	m_flContrast;
	float	m_flRedLevel;
	float	m_flGreenLevel;
	float	m_flBlueLevel;
	float	m_flVignetteScale;
	float	m_flFilmGrainScale;
	float	m_flColorAccentScale;
	Vector	m_vecAccentColor;
};
