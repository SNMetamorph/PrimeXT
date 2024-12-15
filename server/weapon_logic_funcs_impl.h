#pragma once
#include "weapon_logic_funcs.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"

class CWeaponLogicFuncsImpl : public IWeaponLogicFuncs
{
public:
	CWeaponLogicFuncsImpl(CBasePlayerWeapon *weaponEntity);

	const char *GetWeaponClassname() override;
	int GetWeaponBodygroup() override;
	bool GetNextBestWeapon() override;

private:
	CBasePlayerWeapon *m_pWeapon;
};
