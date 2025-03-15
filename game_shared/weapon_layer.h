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
#include "vector.h"
#include "matrix.h"
#include "bitops_traits.h"
#include <string_view>
#include <stdint.h>

#define _CLASSNAME_STR(s) (#s)
#define CLASSNAME_STR(s) _CLASSNAME_STR(s)

#define AUTOAIM_2DEGREES  0.0348994967025
#define AUTOAIM_5DEGREES  0.08715574274766
#define AUTOAIM_8DEGREES  0.1391731009601
#define AUTOAIM_10DEGREES 0.1736481776669

// do not change, these flags are shared with engine
enum class WeaponEventFlags : int
{
	// Empty flag
	None = 0,

	// Skip local host for event send.
	NotHost	= (1<<0), 

	// Send the event reliably. You must specify the origin and angles and use
	// PLAYBACK_EVENT_FULL for this to work correctly on the server for anything
	// that depends on the event origin/angles.  I.e., the origin/angles are not
	// taken from the invoking edict for reliable events.
	Reliable = (1<<1),	 

	// Don't restrict to PAS/PVS, send this event to _everybody_ on the server ( useful for stopping CHAN_STATIC
	// sounds started by client event when client is not in PVS anymore ( hwguy in TFC e.g. ).
	Global = (1<<2),

	// If this client already has one of these events in its queue, just update the event instead of sending it as a duplicate
	Update = (1<<3),

	// Only send to entity specified as the invoker
	HostOnly = (1<<4),

	// Only send if the event was created on the server.
	Server = (1<<5),

	// Only issue event client side ( from shared code )
	Client = (1<<6)
};

DEFINE_ENUM_BITOPS( WeaponEventFlags );

struct WeaponEventParams
{
	WeaponEventFlags flags;
	float *origin;
	float *angles;
	float delay;
	float fparam1;
	float fparam2; 
	int iparam1;
	int iparam2; 
	int bparam1; 
	int bparam2;
	uint16_t eventindex;
};

// forward declaration, client should not know anything about server entities
class CBasePlayerWeapon;

class IWeaponLayer
{
public:
	virtual ~IWeaponLayer() {};

	// accessing weapon entity state
	virtual int GetWeaponBodygroup() = 0;
	virtual void SetWeaponBodygroup(int value) = 0;
	virtual Vector GetGunPosition() = 0;
	virtual matrix3x3 GetCameraOrientation() = 0;
	virtual Vector GetViewAngles() = 0;
	virtual Vector GetAutoaimVector(float delta) = 0;
	virtual Vector FireBullets(int bullets, Vector origin, matrix3x3 orientation, float distance, float spread, int bulletType, uint32_t seed, int damage = 0) = 0;
	virtual CBasePlayerWeapon* GetWeaponEntity() = 0;

	// modifying/accessing player state
	virtual int GetPlayerAmmo(int ammoType) = 0;
	virtual void SetPlayerAmmo(int ammoType, int count) = 0;
	virtual void SetPlayerWeaponAnim(int anim) = 0;
	virtual void SetPlayerViewmodel(std::string_view model) = 0;
	virtual void DisablePlayerViewmodel() = 0;
	virtual int GetPlayerViewmodel() = 0;
	virtual int GetPlayerWaterlevel() = 0;
	virtual bool CheckPlayerButtonFlag(int buttonMask) = 0;
	virtual void ClearPlayerButtonFlag(int buttonMask) = 0;
	virtual float GetPlayerNextAttackTime() = 0;
	virtual void SetPlayerNextAttackTime(float value) = 0;
	virtual void SetPlayerFOV(float value) = 0;
	virtual float GetPlayerFOV() = 0;
	virtual Vector GetPlayerVelocity() = 0;
	virtual void SetPlayerVelocity(Vector value) = 0;

	// miscellaneous things
	virtual float GetWeaponTimeBase(bool usePredicting) = 0;
	virtual float GetTime() = 0;
	virtual uint32_t GetRandomSeed() = 0;
	virtual uint32_t GetRandomInt(uint32_t seed, int32_t min, int32_t max) = 0;
	virtual float GetRandomFloat(uint32_t seed, float min, float max) = 0;
	virtual uint16_t PrecacheEvent(const char *eventName) = 0;
	virtual void PlaybackWeaponEvent(const WeaponEventParams &params) = 0;
	virtual bool ShouldRunFuncs() = 0;
	virtual bool IsMultiplayer() = 0;
};
