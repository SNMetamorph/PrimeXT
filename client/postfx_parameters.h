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

#pragma once
#include "vector.h"

class CPostFxParameters
{
public:
    void ParseMessage();
    void Interpolate(const CPostFxParameters &oldState, const CPostFxParameters &newState, float t);
    static const CPostFxParameters& Defaults();

    inline void SetBrightness(float value)      { m_flBrightness = value; }
    inline void SetSaturation(float value)      { m_flSaturation = value; }
    inline void SetContrast(float value)        { m_flContrast = value; }
    inline void SetRedLevel(float value)        { m_flRedLevel = value; }
    inline void SetGreenLevel(float value)      { m_flGreenLevel = value; }
    inline void SetBlueLevel(float value)       { m_flBlueLevel = value; }
    inline void SetVignetteScale(float value)   { m_flVignetteScale = value; }
    inline void SetFilmGrainScale(float value)  { m_flFilmGrainScale = value; }
    inline void SetColorAccentScale(float value) { m_flColorAccentScale = value; }
    inline void SetAccentColor(Vector value)    { m_vecAccentColor = value; }

    inline float GetFadeDuration() const        { return Q_max(m_flFadeDuration, 0.0001f); }
    inline float GetBrightness() const          { return m_flBrightness; }
    inline float GetSaturation() const          { return m_flSaturation; }
    inline float GetContrast() const            { return m_flContrast; }
    inline float GetRedLevel() const            { return m_flRedLevel; }
    inline float GetGreenLevel() const          { return m_flGreenLevel; }
    inline float GetBlueLevel() const           { return m_flBlueLevel; }
    inline float GetVignetteScale() const       { return m_flVignetteScale; }
    inline float GetFilmGrainScale() const      { return m_flFilmGrainScale; }
    inline float GetColorAccentScale() const    { return m_flColorAccentScale; }
    inline const Vector& GetAccentColor() const { return m_vecAccentColor; }

private:
    float InterpolateParameter(float a, float b, float t);

    float   m_flFadeDuration;
	float   m_flBrightness;
    float   m_flSaturation;
    float   m_flContrast;
    float   m_flRedLevel;
    float   m_flGreenLevel;
    float   m_flBlueLevel;
    float   m_flVignetteScale;
    float   m_flFilmGrainScale;
    float   m_flColorAccentScale;
    Vector  m_vecAccentColor;
};
