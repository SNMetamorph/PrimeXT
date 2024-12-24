#pragma once
#include "weapon_layer.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"

class CServerWeaponLayerImpl : public IWeaponLayer
{
public:
	CServerWeaponLayerImpl(CBasePlayerWeapon *weaponEntity);

	int GetWeaponBodygroup() override;
	bool GetNextBestWeapon() override;

	int GetPlayerAmmo(int ammoType) override;
	void SetPlayerAmmo(int ammoType, int count) override;
	void SetPlayerWeaponAnim(int anim) override;
	void SetPlayerViewmodel(int model) override;
	bool CheckPlayerButtonFlag(int buttonMask) override;
	void ClearPlayerButtonFlag(int buttonMask) override;
	float GetPlayerNextAttackTime() override;
	void SetPlayerNextAttackTime(float value) override;

	float GetWeaponTimeBase() override;

private:
	CBasePlayerWeapon *m_pWeapon;
};
