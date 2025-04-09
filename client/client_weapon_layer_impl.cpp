/*
client_weapon_layer_impl.cpp - part of client-side weapons predicting implementation
Copyright (C) 2025 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "client_weapon_layer_impl.h"
#include "hud.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"

CClientWeaponLayerImpl::CClientWeaponLayerImpl(CWeaponPredictingContext::PlayerState &state) :
	m_playerState(state)
{
}

int CClientWeaponLayerImpl::GetWeaponBodygroup()
{
	// stub for now, later this better to be sent thru network within weapon_data_t
	// but anyway it isn't much important, because it is not even used by default
	return 0; 
}

void CClientWeaponLayerImpl::SetWeaponBodygroup(int value)
{
	// also stub for now, investigate it later
}

Vector CClientWeaponLayerImpl::GetGunPosition()
{
	return m_playerState.origin + m_playerState.viewOffset;
}

matrix3x3 CClientWeaponLayerImpl::GetCameraOrientation()
{
	return matrix3x3(m_playerState.viewAngles);
}

Vector CClientWeaponLayerImpl::GetViewAngles()
{
	return m_playerState.viewAngles;
}

Vector CClientWeaponLayerImpl::GetAutoaimVector(float delta)
{
	matrix3x3 camera(m_playerState.viewAngles);
	return camera.GetForward(); // stub
}

Vector CClientWeaponLayerImpl::FireBullets(int bullets, Vector origin, matrix3x3 orientation, float distance, float spread, int bulletType, uint32_t seed, int damage)
{
	float x, y, z;

	for (uint32_t i = 1; i <= bullets; i++)
	{
		// use player's random seed, get circular gaussian spread
		// TODO: reimplement it for proper uniform-distributed generating of points within circle
		x = m_randomGenerator.GetFloat(seed + i, -0.5f, 0.5f) + m_randomGenerator.GetFloat(seed + 1 + i, -0.5f, 0.5f);
		y = m_randomGenerator.GetFloat(seed + 2 + i, -0.5f, 0.5f) + m_randomGenerator.GetFloat(seed + 3 + i, -0.5f, 0.5f);
		z = x * x + y * y;
	}

	return Vector( x * spread, y * spread, 0.0 );
}

int CClientWeaponLayerImpl::GetPlayerAmmo(int ammoType)
{
	return m_playerState.ammo[ammoType];
}

void CClientWeaponLayerImpl::SetPlayerAmmo(int ammoType, int count)
{
	m_playerState.ammo[ammoType] = count;
}

void CClientWeaponLayerImpl::SetPlayerWeaponAnim(int anim)
{
	m_playerState.weaponanim = anim;
	if (m_playerState.runfuncs) 
	{
		// to avoid animation desync, this should changed only when runfuncs is true.
		// i don't know why it works like this, but this is the only way.
		m_playerState.activeWeaponanim = anim;
	}
}

void CClientWeaponLayerImpl::SetPlayerViewmodel(std::string_view model)
{
	int modelIndex;
	gEngfuncs.CL_LoadModel(model.data(), &modelIndex);
	m_playerState.viewmodel = modelIndex;
}

void CClientWeaponLayerImpl::DisablePlayerViewmodel()
{
	m_playerState.viewmodel = 0;
}

int CClientWeaponLayerImpl::GetPlayerViewmodel()
{
	return m_playerState.viewmodel;
}

int CClientWeaponLayerImpl::GetPlayerWaterlevel()
{
	return m_playerState.waterlevel;
}

bool CClientWeaponLayerImpl::CheckPlayerButtonFlag(int buttonMask)
{
	return FBitSet(m_playerState.buttons, buttonMask);
}

void CClientWeaponLayerImpl::ClearPlayerButtonFlag(int buttonMask)
{
	ClearBits(m_playerState.buttons, buttonMask);
}

float CClientWeaponLayerImpl::GetPlayerNextAttackTime()
{
	return m_playerState.nextAttack; 
}

void CClientWeaponLayerImpl::SetPlayerNextAttackTime(float value)
{
	m_playerState.nextAttack = value;
}

void CClientWeaponLayerImpl::SetPlayerFOV(float value)
{
	m_playerState.fov = value;
}

float CClientWeaponLayerImpl::GetPlayerFOV()
{
	return m_playerState.fov;
}

Vector CClientWeaponLayerImpl::GetPlayerVelocity()
{
	return m_playerState.velocity;
}

void CClientWeaponLayerImpl::SetPlayerVelocity(Vector value)
{
	m_playerState.velocity = value;
}

float CClientWeaponLayerImpl::GetWeaponTimeBase(bool usePredicting)
{
	return usePredicting ? 0.0f : m_playerState.time;
}

float CClientWeaponLayerImpl::GetTime()
{
	return static_cast<float>(m_playerState.time);
}

uint32_t CClientWeaponLayerImpl::GetRandomSeed()
{
	return m_playerState.randomSeed;
}

uint32_t CClientWeaponLayerImpl::GetRandomInt(uint32_t seed, int32_t min, int32_t max)
{
	return m_randomGenerator.GetInteger(seed, min, max);
}

float CClientWeaponLayerImpl::GetRandomFloat(uint32_t seed, float min, float max)
{
	return m_randomGenerator.GetFloat(seed, min, max);
}

uint16_t CClientWeaponLayerImpl::PrecacheEvent(const char *eventName)
{
	return gEngfuncs.pfnPrecacheEvent(1, eventName);
}

void CClientWeaponLayerImpl::PlaybackWeaponEvent(const WeaponEventParams &params)
{
	gEngfuncs.pfnPlaybackEvent(static_cast<int>(params.flags), nullptr,
		params.eventindex, params.delay, 
		params.origin, params.angles, 
		params.fparam1, params.fparam2, 
		params.iparam1, params.iparam2, 
		params.bparam1, params.bparam2);
}

// if you need to do something that has visible effects for user or that should happen only 
// once per game frame, then you should use this function for doing check before running such code. 
// that's because CBaseWeaponContext::ItemPostFrame could be run multiple times within one game frame.
// if it returns true, you should run desired code. otherwise, you can just update some internal state,
// but do not run code that meant to be invoked once a frame - it will be called multiple times.
bool CClientWeaponLayerImpl::ShouldRunFuncs()
{
	return m_playerState.runfuncs;
}

bool CClientWeaponLayerImpl::IsMultiplayer()
{
	return gEngfuncs.GetMaxClients() > 1;
}
