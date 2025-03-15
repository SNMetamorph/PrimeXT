/*
server_weapon_layer_impl.h - part of server-side weapons predicting implementation
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
#include "weapon_layer.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "seeded_random_generator.h"

class CServerWeaponLayerImpl : public IWeaponLayer
{
public:
	CServerWeaponLayerImpl(CBasePlayerWeapon *weaponEntity);
	~CServerWeaponLayerImpl() = default;

	int GetWeaponBodygroup() override;
	void SetWeaponBodygroup(int value) override;
	Vector GetGunPosition() override;
	matrix3x3 GetCameraOrientation() override;
	Vector GetViewAngles() override;
	Vector GetAutoaimVector(float delta) override;
	Vector FireBullets(int bullets, Vector origin, matrix3x3 orientation, float distance, float spread, int bulletType, uint32_t seed, int damage = 0) override;
	CBasePlayerWeapon* GetWeaponEntity() override { return m_pWeapon; };

	int GetPlayerAmmo(int ammoType) override;
	void SetPlayerAmmo(int ammoType, int count) override;
	void SetPlayerWeaponAnim(int anim) override;
	void SetPlayerViewmodel(std::string_view model) override;
	void DisablePlayerViewmodel() override;
	int GetPlayerViewmodel() override;
	int GetPlayerWaterlevel() override;
	bool CheckPlayerButtonFlag(int buttonMask) override;
	void ClearPlayerButtonFlag(int buttonMask) override;
	float GetPlayerNextAttackTime() override;
	void SetPlayerNextAttackTime(float value) override;
	void SetPlayerFOV(float value) override;
	float GetPlayerFOV() override;
	Vector GetPlayerVelocity() override;
	void SetPlayerVelocity(Vector value) override;

	float GetWeaponTimeBase(bool usePredicting) override;
	float GetTime() override;
	uint32_t GetRandomSeed() override;
	uint32_t GetRandomInt(uint32_t seed, int32_t min, int32_t max) override;
	float GetRandomFloat(uint32_t seed, float min, float max) override;
	uint16_t PrecacheEvent(const char *eventName) override;
	void PlaybackWeaponEvent(const WeaponEventParams &params) override;
	bool ShouldRunFuncs() override;
	bool IsMultiplayer() override;

private:
	CBasePlayerWeapon *m_pWeapon;
	CSeededRandomGenerator m_randomGenerator;
};
