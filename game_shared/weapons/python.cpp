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

#include "python.h"

#ifdef CLIENT_DLL
#else
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#endif

// check VECTOR_CONE_1DEGREES macro
#define CONE_1DEGREES 0.00873

CPythonWeaponContext::CPythonWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_PYTHON;
	m_iDefaultAmmo = PYTHON_DEFAULT_GIVE;
	m_fInZoom = false;
	m_usFirePython = m_pLayer->PrecacheEvent("events/python.sc");
}

int CPythonWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(PYTHON_CLASSNAME);
	p->pszAmmo1 = "357";
	p->iMaxAmmo1 = _357_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = PYTHON_MAX_CLIP;
	p->iFlags = 0;
	p->iSlot = 1;
	p->iPosition = 1;
	p->iId = m_iId;
	p->iWeight = PYTHON_WEIGHT;
	return 1;
}

bool CPythonWeaponContext::Deploy()
{
	if (m_pLayer->IsMultiplayer())
	{
		// enable laser sight geometry.
		m_pLayer->SetWeaponBodygroup(1);
	}
	else
	{
		m_pLayer->SetWeaponBodygroup(0);
	}

	return DefaultDeploy("models/v_357.mdl", "models/p_357.mdl", PYTHON_DRAW, "python");
}

void CPythonWeaponContext::Holster()
{
	m_fInReload = FALSE; // cancel any reload in progress.

	if (m_fInZoom) {
		SecondaryAttack();
	}

	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f);
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
	SendWeaponAnim(PYTHON_HOLSTER);
}

void CPythonWeaponContext::SecondaryAttack()
{
	if (!m_pLayer->IsMultiplayer())
	{
		return;
	}

	if (m_pLayer->GetPlayerFOV() != 0.0f)
	{
		m_pLayer->SetPlayerFOV(0.0f); // 0 means reset to default fov
		m_fInZoom = false;
	}
	else
	{
		m_pLayer->SetPlayerFOV(40.0f);
		m_fInZoom = true;
	}

	m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
}

void CPythonWeaponContext::PrimaryAttack()
{
	// don't fire underwater
	if (m_pLayer->GetPlayerWaterlevel() == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.15f;
		return;
	}

	if (m_iClip <= 0)
	{
		if (!m_fFireOnEmpty)
		{
			Reload();
		}
		else
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.15f;
		}

		return;
	}

	m_iClip--;

	Vector vecSrc = m_pLayer->GetGunPosition();
	matrix3x3 cameraTransform = m_pLayer->GetCameraOrientation();
	cameraTransform.SetForward(m_pLayer->GetAutoaimVector(AUTOAIM_10DEGREES));
	Vector spread = m_pLayer->FireBullets(1, vecSrc, cameraTransform, 8192, CONE_1DEGREES, BULLET_PLAYER_357, m_pLayer->GetRandomSeed());

	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usFirePython;
	params.delay = 0.0f;
	params.origin = vecSrc;
	params.angles = cameraTransform.GetAngles();
	params.fparam1 = spread.x;
	params.fparam2 = spread.y;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = (m_iClip == 0) ? 1 : 0;
	params.bparam2 = 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;

	player->SetAnimation(PLAYER_ATTACK1);
	player->pev->effects = (int)(player->pev->effects) | EF_MUZZLEFLASH;
	player->m_iWeaponVolume = LOUD_GUN_VOLUME;
	player->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	if (!m_iClip && player->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		player->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#endif

	// m_pPlayer->pev->punchangle.x -= 10;
	m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.75f;
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
}

void CPythonWeaponContext::Reload()
{
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) < 1)
		return;

	if (m_pLayer->GetPlayerFOV() != 0.0f)
	{
		m_pLayer->SetPlayerFOV(0.0f); // 0 means reset to default fov
		m_fInZoom = false;
	}

	DefaultReload(6, PYTHON_RELOAD, 2.0f, m_pLayer->IsMultiplayer() ? 1 : 0);
}

void CPythonWeaponContext::WeaponIdle()
{
	ResetEmptySound();

	m_pLayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	int iAnim;
	float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.f, 1.f);
	if (flRand <= 0.5f)
	{
		iAnim = PYTHON_IDLE1;
		m_flTimeWeaponIdle = (70.0 / 30.0);
	}
	else if (flRand <= 0.7f)
	{
		iAnim = PYTHON_IDLE2;
		m_flTimeWeaponIdle = (60.0 / 30.0);
	}
	else if (flRand <= 0.9f)
	{
		iAnim = PYTHON_IDLE3;
		m_flTimeWeaponIdle = (88.0 / 30.0);
	}
	else
	{
		iAnim = PYTHON_FIDGET;
		m_flTimeWeaponIdle = (170.0 / 30.0);
	}

	SendWeaponAnim(iAnim, m_pLayer->IsMultiplayer() ? 1 : 0);
}
