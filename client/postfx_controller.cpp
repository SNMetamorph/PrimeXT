/*
postfx_controller.cpp - class for holding and manipulating post effects state
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

#include "postfx_controller.h"
#include "utils.h"

CPostFxController &g_PostFxController = CPostFxController::GetInstance();

CPostFxController &CPostFxController::GetInstance()
{
	static CPostFxController instance;
	return instance;
}

void CPostFxController::UpdateState(const CPostFxParameters &state, bool interpolate)
{
	m_PrevState = m_CurrentState;
	m_CurrentState = state;
	m_bInterpolate = interpolate;
	m_flStateUpdateTime = gEngfuncs.GetClientTime();
}

CPostFxParameters CPostFxController::GetState() const
{
	if (m_bInterpolate)
	{
		CPostFxParameters actualState;
		const float timeDelta = gEngfuncs.GetClientTime() - m_flStateUpdateTime;
		const float t = Q_min(timeDelta / m_CurrentState.GetFadeDuration(), 1.0f);
		actualState.Interpolate(m_PrevState, m_CurrentState, t);
		return actualState;
	}
	return m_CurrentState;
}
