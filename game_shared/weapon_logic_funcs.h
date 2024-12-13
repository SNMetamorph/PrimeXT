#pragma once

class IWeaponLogicFuncs
{
public:
	virtual ~IWeaponLogicFuncs() {};
	virtual const char *GetWeaponClassname() = 0;
	virtual int GetWeaponBodygroup() = 0;
	virtual bool GetNextBestWeapon() = 0;

	// modifying/accessing player state
	virtual int GetPlayerAmmo(int ammoType) = 0;
	virtual void SetPlayerAmmo(int ammoType, int count) = 0;
	virtual int GetPlayerButtons() = 0;

	// miscellaneous things
	virtual float GetGlobalTime() = 0;
};
