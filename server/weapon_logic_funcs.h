#pragma once

class IWeaponLogicFuncs
{
public:
	virtual ~IWeaponLogicFuncs() {};
	virtual const char *GetWeaponClassname() = 0;
	virtual int GetWeaponBodygroup() = 0;
};
