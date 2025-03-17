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

#include "shotgun.h"

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

// special deathmatch shotgun spreads
#define VECTOR_CONE_DM_SHOTGUN			Vector( 0.08716f, 0.04362f, 0.00f ) // 10 degrees by 5 degrees
#define VECTOR_CONE_DM_DOUBLESHOTGUN	Vector( 0.17365f, 0.04362f, 0.00f ) // 20 degrees by 5 degrees

CShotgunWeaponContext::CShotgunWeaponContext(std::unique_ptr<IWeaponLayer> &&layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_SHOTGUN;
	m_iDefaultAmmo = SHOTGUN_DEFAULT_GIVE;
	m_usSingleFire = m_pLayer->PrecacheEvent("events/shotgun1.sc");
	m_usDoubleFire = m_pLayer->PrecacheEvent("events/shotgun2.sc");
}

int CShotgunWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(SHOTGUN_CLASSNAME);
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = SHOTGUN_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId;
	p->iWeight = SHOTGUN_WEIGHT;
	return 1;
}

bool CShotgunWeaponContext::Deploy()
{
	return DefaultDeploy( "models/v_shotgun.mdl", "models/p_shotgun.mdl", SHOTGUN_DRAW, "shotgun" );
}

void CShotgunWeaponContext::PrimaryAttack()
{
	// don't fire underwater
	if (m_pLayer->GetPlayerWaterlevel() == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		Reload();
		PlayEmptySound();
		return;
	}

	m_iClip--;

	Vector vecSrc = m_pLayer->GetGunPosition();
	matrix3x3 cameraTransform = m_pLayer->GetCameraOrientation();
	cameraTransform.SetForward(m_pLayer->GetAutoaimVector(AUTOAIM_5DEGREES));

	const int32_t bulletsCount = m_pLayer->IsMultiplayer() ? 4 : 6;
	const float spreadCoef = m_pLayer->IsMultiplayer() ? VECTOR_CONE_DM_SHOTGUN.x : VECTOR_CONE_10DEGREES.x;
	Vector spread = m_pLayer->FireBullets(bulletsCount, vecSrc, cameraTransform, 2048, spreadCoef, BULLET_PLAYER_BUCKSHOT, m_pLayer->GetRandomSeed());

	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usSingleFire;
	params.delay = 0.0f;
	params.origin = vecSrc;
	params.angles = cameraTransform.GetAngles();
	params.fparam1 = spread.x;
	params.fparam2 = spread.y;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = 0;
	params.bparam2 = 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;

	player->m_iWeaponVolume = LOUD_GUN_VOLUME;
	player->m_iWeaponFlash = NORMAL_GUN_FLASH;
	player->pev->effects = (int)(player->pev->effects) | EF_MUZZLEFLASH;

	if (!m_iClip && player->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		player->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#endif

	//m_flPumpTime = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5; // ??? is it correct
	m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.75;
	m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.75;

	if (m_iClip != 0)
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 5.0;
	else
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.75;

	m_fInSpecialReload = 0;
}

void CShotgunWeaponContext::SecondaryAttack()
{
	// don't fire underwater
	if (m_pLayer->GetPlayerWaterlevel() == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.15;
		return;
	}

	if (m_iClip <= 1)
	{
		Reload();
		PlayEmptySound();
		return;
	}

	m_iClip -= 2;

	Vector vecSrc = m_pLayer->GetGunPosition();
	matrix3x3 cameraTransform = m_pLayer->GetCameraOrientation();
	cameraTransform.SetForward(m_pLayer->GetAutoaimVector(AUTOAIM_5DEGREES));
	
	const int32_t bulletsCount = m_pLayer->IsMultiplayer() ? 8 : 12;
	const float spreadCoef = m_pLayer->IsMultiplayer() ? VECTOR_CONE_DM_DOUBLESHOTGUN.x : VECTOR_CONE_10DEGREES.x;
	Vector spread = m_pLayer->FireBullets(bulletsCount, vecSrc, cameraTransform, 2048, spreadCoef, BULLET_PLAYER_BUCKSHOT, m_pLayer->GetRandomSeed());

	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usDoubleFire;
	params.delay = 0.0f;
	params.origin = vecSrc;
	params.angles = cameraTransform.GetAngles();
	params.fparam1 = spread.x;
	params.fparam2 = spread.y;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = 0;
	params.bparam2 = 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;

	player->m_iWeaponVolume = LOUD_GUN_VOLUME;
	player->m_iWeaponFlash = NORMAL_GUN_FLASH;
	player->pev->effects = (int)(player->pev->effects) | EF_MUZZLEFLASH;
	player->SetAnimation( PLAYER_ATTACK1 );

	if (!m_iClip && player->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		player->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#endif

	m_flPumpTime = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.95; // ??? is it correct

	m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.5;
	m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.5;

	if (m_iClip != 0)
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 6.0;
	else
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.5;

	m_fInSpecialReload = 0;
}

void CShotgunWeaponContext::Reload()
{
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) <= 0 || m_iClip == SHOTGUN_MAX_CLIP)
		return;

	// don't reload until recoil is done
	if (m_flNextPrimaryAttack > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	// check to see if we're ready to reload
	if (m_fInSpecialReload == 0)
	{
		m_fInSpecialReload = 1;
		SendWeaponAnim(SHOTGUN_START_RELOAD);
		m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.6);

		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.6;
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0;
		m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0;
		return;
	}
	else if (m_fInSpecialReload == 1)
	{
		if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
			return;

		// was waiting for gun to move to side
		m_fInSpecialReload = 2;

#ifndef CLIENT_DLL
		CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
		if (RANDOM_LONG(0, 1))
			EMIT_SOUND_DYN(ENT(player->pev), CHAN_ITEM, "weapons/reload1.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0, 0x1f));
		else
			EMIT_SOUND_DYN(ENT(player->pev), CHAN_ITEM, "weapons/reload3.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0, 0x1f));
#endif

		SendWeaponAnim(SHOTGUN_RELOAD);
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5;
	}
	else
	{
		// Add them to the clip
		m_iClip += 1;
		m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - 1);
		m_fInSpecialReload = 1;
	}
}

void CShotgunWeaponContext::WeaponIdle()
{
	ResetEmptySound( );

	m_pLayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if (m_flTimeWeaponIdle < m_pLayer->GetWeaponTimeBase(UsePredicting()))
	{
		if (m_iClip == 0 && m_fInSpecialReload == 0 && m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) > 0)
		{
			Reload();
		}
		else if (m_fInSpecialReload != 0)
		{
			if (m_iClip != SHOTGUN_MAX_CLIP && m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) > 0)
			{
				Reload();
			}
			else
			{
				// reload debounce has timed out
				SendWeaponAnim( SHOTGUN_PUMP );
#ifndef CLIENT_DLL
				// play cocking sound
				CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
				EMIT_SOUND_DYN(ENT(player->pev), CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0,0x1f));
#endif
				m_fInSpecialReload = 0;
				m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.5;
			}
		}
		else
		{
			int iAnim;
			float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.f, 1.f);
			if (flRand <= 0.8f)
			{
				iAnim = SHOTGUN_IDLE_DEEP;
				m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + (60.0/12.0);// * RANDOM_LONG(2, 5);
			}
			else if (flRand <= 0.95f)
			{
				iAnim = SHOTGUN_IDLE;
				m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + (20.0/9.0);
			}
			else
			{
				iAnim = SHOTGUN_IDLE4;
				m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + (20.0/9.0);
			}
			SendWeaponAnim( iAnim );
		}
	}
}
