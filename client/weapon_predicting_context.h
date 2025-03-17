/*
weapon_predicting_context.h - part of client-side weapons predicting implementation
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

#pragma once
#include "vector.h"
#include "mathlib.h"
#include "usercmd.h"
#include "entity_state.h"
#include "weapon_context.h"
#include <stdint.h>
#include <memory>
#include <unordered_map>

class CWeaponPredictingContext
{
public:
	struct PlayerState
	{
		int32_t buttons;
		int32_t buttonsPressed;
		int32_t buttonsReleased;
		int32_t flags;
		int32_t deadflag;
		int32_t waterlevel;
		int32_t weaponanim;
		int32_t activeWeaponanim;
		int32_t viewmodel;
		Vector viewAngles;
		Vector viewOffset;
		Vector origin;
		Vector velocity;
		float maxSpeed;
		float fov;
		float nextAttack;
		std::unordered_map<uint32_t, int32_t> ammo;

		// state unrelated to player, but required anyway
		double time;
		bool runfuncs;
		uint32_t randomSeed;

		// variables that are used for tracking of changes between frames
		struct {
			float health;
			int32_t buttons;
		} cached;
	};

	CWeaponPredictingContext();
	void PostThink(local_state_t *from, local_state_t *to, usercmd_t *cmd, bool runfuncs, double time, uint32_t randomSeed);

private:
	void ReadPlayerState(const local_state_t *from, const local_state_t *to, usercmd_t *cmd);
	void WritePlayerState(local_state_t *to);
	void UpdatePlayerTimers(const usercmd_t *cmd);
	void UpdateWeaponTimers(CBaseWeaponContext *weapon, const usercmd_t *cmd);
	void ReadWeaponsState(const local_state_t *from);
	void WriteWeaponsState(local_state_t *to, const usercmd_t *cmd);
	void ReadWeaponSpecificData(CBaseWeaponContext *weapon, const local_state_t *from);
	void WriteWeaponSpecificData(CBaseWeaponContext *weapon, local_state_t *to);
	void HandlePlayerSpawnDeath(local_state_t *to, CBaseWeaponContext *weapon);
	void HandleWeaponSwitch(const local_state_t *from, local_state_t *to, const usercmd_t *cmd, CBaseWeaponContext *weapon);
	CBaseWeaponContext* GetWeaponContext(uint32_t weaponID);

	PlayerState m_playerState;
	std::unordered_map<uint32_t, std::unique_ptr<CBaseWeaponContext>> m_weaponsState;
};
