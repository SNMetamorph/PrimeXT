/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#pragma once
#include "weapon_context.h"
#include "weapon_layer.h"
#include <array>
#include <memory>

#ifndef CLIENT_DLL
#include "env_beam.h"
#include "env_sprite.h"
#endif

#define WEAPON_EGON				10
#define EGON_WEIGHT				20
#define EGON_MAX_CLIP			WEAPON_NOCLIP
#define EGON_DEFAULT_GIVE		20
#define EGON_CLASSNAME			weapon_egon

#define EGON_PRIMARY_VOLUME		450
#define EGON_BEAM_SPRITE		"sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE		"sprites/XSpark1.spr"
#define EGON_SOUND_OFF			"weapons/egon_off1.wav"
#define EGON_SOUND_RUN			"weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP		"weapons/egon_windup2.wav"

#define EGON_SWITCH_NARROW_TIME	0.75	// Time it takes to switch fire modes
#define EGON_SWITCH_WIDE_TIME	1.5
#define EGON_PULSE_INTERVAL		0.25
#define EGON_DISCHARGE_INTERVAL	0.25

enum egon_e
{
	EGON_IDLE1 = 0,
	EGON_FIDGET1,
	EGON_ALTFIREON,
	EGON_ALTFIRECYCLE,
	EGON_ALTFIREOFF,
	EGON_FIRE1,
	EGON_FIRE2,
	EGON_FIRE3,
	EGON_FIRE4,
	EGON_DRAW,
	EGON_HOLSTER
};

enum EGON_FIRESTATE : int { FIRE_OFF, FIRE_CHARGE };
enum EGON_FIREMODE : int { FIRE_NARROW, FIRE_WIDE };

class CEgonWeaponContext : public CBaseWeaponContext
{
public:
	CEgonWeaponContext() = delete;
	CEgonWeaponContext(std::unique_ptr<IWeaponLayer> &&layer);
	~CEgonWeaponContext() = default;

	int iItemSlot() override { return 4; }
	int GetItemInfo(ItemInfo *p) const override;
	bool Deploy() override;
	void Holster() override;
	void PrimaryAttack() override;
	void WeaponIdle() override;

private:
	void UpdateEffect(const Vector &startPoint, const Vector &endPoint, float timeBlend);
	void CreateEffect();
	void DestroyEffect();

	void EndAttack();
	void Attack();

	float GetPulseInterval();
	float GetDischargeInterval();

	void Fire(const Vector &vecOrigSrc, const Vector &vecDir);

	bool HasAmmo();
	void UseAmmo(int count);
	
public:
	static std::array<int, 4> g_fireAnims1;
	static std::array<int, 1> g_fireAnims2;
#ifndef CLIENT_DLL
	CBeam	*m_pBeam;
	CBeam	*m_pNoise;
	CSprite	*m_pSprite;
#endif
	uint16_t	m_usEgonFire;
	uint16_t	m_usEgonStop;
	float		m_flAttackCooldown;
	EGON_FIRESTATE	m_fireState;
	EGON_FIREMODE	m_fireMode;
	float	m_shakeTime;
	bool	m_deployed;
	float	m_flAmmoUseTime; // since we use < 1 point of ammo per update, we subtract ammo on a timer.
};

template<>
struct CBaseWeaponContext::AssignedWeaponID<CEgonWeaponContext> {
	static constexpr int32_t value = WEAPON_EGON;
};
