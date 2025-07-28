/*
postfx_parameters.h - class for descripting post effects state
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

#include "postfx_parameters.h"
#include "parsemsg.h"
#include "mathlib.h"

void CPostFxParameters::ParseMessage()
{
	m_flFadeDuration = READ_FLOAT();
	m_flBrightness = READ_FLOAT();
	m_flSaturation = READ_FLOAT();
	m_flContrast = READ_FLOAT();
	m_flRedLevel = READ_FLOAT();
	m_flGreenLevel = READ_FLOAT();
	m_flBlueLevel = READ_FLOAT();
	m_flVignetteScale = READ_FLOAT();
	m_flFilmGrainScale = READ_FLOAT();
	m_flColorAccentScale = READ_FLOAT();
	m_vecAccentColor.x = READ_FLOAT();
	m_vecAccentColor.y = READ_FLOAT();
	m_vecAccentColor.z = READ_FLOAT();
}

void CPostFxParameters::Interpolate(const CPostFxParameters &oldState, const CPostFxParameters &newState, float t)
{
	t = bound(0.0f, t, 1.0f);
	m_flBrightness = InterpolateParameter(oldState.GetBrightness(), newState.GetBrightness(), t);
	m_flSaturation = InterpolateParameter(oldState.GetSaturation(), newState.GetSaturation(), t);
	m_flContrast = InterpolateParameter(oldState.GetContrast(), newState.GetContrast(), t);
	m_flRedLevel = InterpolateParameter(oldState.GetRedLevel(), newState.GetRedLevel(), t);
	m_flGreenLevel = InterpolateParameter(oldState.GetGreenLevel(), newState.GetGreenLevel(), t);
	m_flBlueLevel = InterpolateParameter(oldState.GetBlueLevel(), newState.GetBlueLevel(), t);
	m_flVignetteScale = InterpolateParameter(oldState.GetVignetteScale(), newState.GetVignetteScale(), t);
	m_flFilmGrainScale = InterpolateParameter(oldState.GetFilmGrainScale(), newState.GetFilmGrainScale(), t);
	m_flColorAccentScale = InterpolateParameter(oldState.GetColorAccentScale(), newState.GetColorAccentScale(), t);
	m_vecAccentColor.x = InterpolateParameter(oldState.GetAccentColor().x, newState.GetAccentColor().x, t);
	m_vecAccentColor.y = InterpolateParameter(oldState.GetAccentColor().y, newState.GetAccentColor().y, t);
	m_vecAccentColor.z = InterpolateParameter(oldState.GetAccentColor().z, newState.GetAccentColor().z, t);
}

const CPostFxParameters& CPostFxParameters::Defaults()
{
	static CPostFxParameters defaults;
	defaults.m_flBrightness = 0.0f;
	defaults.m_flSaturation = 1.0f;
	defaults.m_flContrast = 1.0f;
	defaults.m_flRedLevel = 1.0f;
	defaults.m_flGreenLevel = 1.0f;
	defaults.m_flBlueLevel = 1.0f;
	defaults.m_flVignetteScale = 0.0f;
	defaults.m_flFilmGrainScale = 0.0f;
	defaults.m_flColorAccentScale = 0.0f;
	defaults.m_vecAccentColor = Vector(1.0f, 1.0f, 1.0f);
	return defaults;
}

float CPostFxParameters::InterpolateParameter(float a, float b, float t)
{
	// use linear interpolation by default
	return (1.0f - t) * a + t * b;
}
