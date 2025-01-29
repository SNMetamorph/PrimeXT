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

	int GetWeaponBodygroup() override;
	Vector GetGunPosition() override;
	matrix3x3 GetCameraOrientation() override;
	Vector GetAutoaimVector(float delta) override;
	Vector FireBullets(int bullets, Vector origin, matrix3x3 orientation, float distance, float spread, int bulletType, uint32_t seed, int damage = 0) override;
	CBasePlayerWeapon* GetWeaponEntity() override { return m_pWeapon; };

	int GetPlayerAmmo(int ammoType) override;
	void SetPlayerAmmo(int ammoType, int count) override;
	void SetPlayerWeaponAnim(int anim) override;
	void SetPlayerViewmodel(int model) override;
	bool CheckPlayerButtonFlag(int buttonMask) override;
	void ClearPlayerButtonFlag(int buttonMask) override;
	float GetPlayerNextAttackTime() override;
	void SetPlayerNextAttackTime(float value) override;

	float GetWeaponTimeBase(bool usePredicting) override;
	uint32_t GetRandomSeed() override;
	uint32_t GetRandomInt(uint32_t seed, int32_t min, int32_t max) override;
	float GetRandomFloat(uint32_t seed, float min, float max) override;
	uint16_t PrecacheEvent(const char *eventName) override;
	void PlaybackWeaponEvent(const WeaponEventParams &params) override;

private:
	CBasePlayerWeapon *m_pWeapon;
	CSeededRandomGenerator m_randomGenerator;
};
