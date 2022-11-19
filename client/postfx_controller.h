/*
postfx_controller.h - class for holding and manipulating post effects state
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
#include "postfx_parameters.h"

class CPostFxController
{
public:
	static CPostFxController& GetInstance();
	void UpdateState(const CPostFxParameters &state, bool interpolate = true);
	CPostFxParameters GetState() const;

private:
	CPostFxController() {};
	CPostFxController(const CPostFxController&) = delete;
	CPostFxController &operator=(const CPostFxController&) = delete;

	bool m_bInterpolate = true;
	float m_flStateUpdateTime = 0.0f;
	CPostFxParameters m_CurrentState = CPostFxParameters::Defaults();
	CPostFxParameters m_PrevState = CPostFxParameters::Defaults();
};

extern CPostFxController &g_PostFxController;
