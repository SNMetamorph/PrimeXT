/*
weapon_layer.h - interface for abstracting client & server weapons implementation differences
Copyright (C) 2024 SNMetamorph

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

#define CLASSNAME_STR(s) (#s)

class IWeaponLayer
{
public:
	virtual ~IWeaponLayer() {};

	// accessing weapon entity state
	virtual int GetWeaponBodygroup() = 0;
	virtual bool GetNextBestWeapon() = 0;

	// modifying/accessing player state
	virtual int GetPlayerAmmo(int ammoType) = 0;
	virtual void SetPlayerAmmo(int ammoType, int count) = 0;
	virtual void SetPlayerWeaponAnim(int anim) = 0;
	virtual void SetPlayerViewmodel(int model) = 0;
	virtual bool CheckPlayerButtonFlag(int buttonMask) = 0;
	virtual void ClearPlayerButtonFlag(int buttonMask) = 0;
	virtual float GetPlayerNextAttackTime() = 0;
	virtual void SetPlayerNextAttackTime(float value) = 0;

	// miscellaneous things
	virtual float GetWeaponTimeBase() = 0;
};
