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

#include "glock.h"

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

enum glock_e
{
	GLOCK_IDLE1 = 0,
	GLOCK_IDLE2,
	GLOCK_IDLE3,
	GLOCK_SHOOT,
	GLOCK_SHOOT_EMPTY,
	GLOCK_RELOAD,
	GLOCK_RELOAD_NOT_EMPTY,
	GLOCK_DRAW,
	GLOCK_HOLSTER,
	GLOCK_ADD_SILENCER
};

CGlockWeaponLogic::CGlockWeaponLogic(IWeaponLayer *layer) :
	CBaseWeaponContext(layer)
{
	m_iDefaultAmmo = GLOCK_DEFAULT_GIVE;
	m_iId = WEAPON_GLOCK;
	m_usFireGlock1 = m_pLayer->PrecacheEvent("events/glock1.sc");
	m_usFireGlock2 = m_pLayer->PrecacheEvent("events/glock2.sc");
}

int CGlockWeaponLogic::GetItemInfo(ItemInfo *p)
{
	p->pszName = CLASSNAME_STR(GLOCK_CLASSNAME);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLOCK_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId;
	p->iWeight = GLOCK_WEIGHT;

	return 1;
}

bool CGlockWeaponLogic::Deploy( )
{
	// pev->body = 1;
	return DefaultDeploy( "models/v_9mmhandgun.mdl", "models/p_9mmhandgun.mdl", GLOCK_DRAW, "onehanded" );
}

void CGlockWeaponLogic::SecondaryAttack( void )
{
	GlockFire( 0.1, 0.2, FALSE );
}

void CGlockWeaponLogic::PrimaryAttack( void )
{
	GlockFire( 0.01, 0.3, TRUE );
}

void CGlockWeaponLogic::GlockFire( float flSpread , float flCycleTime, bool fUseAutoAim )
{
	if (m_iClip <= 0)
	{
		if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase() + 0.2;
		}

		return;
	}

	m_iClip--;

	if (m_iClip != 0)
		SendWeaponAnim( GLOCK_SHOOT );
	else
		SendWeaponAnim( GLOCK_SHOOT_EMPTY );

#ifndef CLIENT_DLL
	// player "shoot" animation
	m_pLayer->GetWeaponEntity()->m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pLayer->GetWeaponEntity()->m_pPlayer->pev->effects = (int)(m_pLayer->GetWeaponEntity()->m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// silenced
	if (m_pLayer->GetWeaponBodygroup() == 1)
	{
		m_pLayer->GetWeaponEntity()->m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
		m_pLayer->GetWeaponEntity()->m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	}
	else
	{
		// non-silenced
		m_pLayer->GetWeaponEntity()->m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
		m_pLayer->GetWeaponEntity()->m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	}
#endif

	Vector vecSrc = m_pLayer->GetGunPosition();
	matrix3x3 aimMatrix = m_pLayer->GetCameraOrientation();

	if (fUseAutoAim) {
		aimMatrix.SetForward(m_pLayer->GetAutoaimVector(AUTOAIM_10DEGREES));
	}

	Vector vecDir = m_pLayer->FireBullets(1, vecSrc, aimMatrix, 8192, flSpread, BULLET_PLAYER_9MM, m_pLayer->GetRandomSeed());
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase() + flCycleTime;

	WeaponEventParams params;
	params.flags = WeaponEventFlags::None; //WeaponEventFlags::NotHost;
	params.eventindex = fUseAutoAim ? m_usFireGlock1 : m_usFireGlock2;
	params.delay = 0.0f;
	params.origin = vecSrc;
	params.angles = aimMatrix.GetAngles();
	params.fparam1 = vecDir.x;
	params.fparam2 = vecDir.y;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = (m_iClip == 0) ? 1 : 0;
	params.bparam2 = 0;
	m_pLayer->PlaybackWeaponEvent(params);

	// PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), fUseAutoAim ? m_usFireGlock1 : m_usFireGlock2, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, ( m_iClip == 0 ) ? 1 : 0, 0 );

#ifndef CLIENT_DLL
	if (!m_iClip && m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) <= 0)
		// HEV suit - indicate out of ammo condition
		m_pLayer->GetWeaponEntity()->m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
#endif
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase() + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
	//m_pPlayer->pev->punchangle.x -= 2;
}

void CGlockWeaponLogic::Reload( void )
{
	int iResult;

	if (m_iClip == 0)
		iResult = DefaultReload( 17, GLOCK_RELOAD, 1.5 );
	else
		iResult = DefaultReload( 17, GLOCK_RELOAD_NOT_EMPTY, 1.5 );

	if (iResult)
	{
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase() + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f);
	}
}

void CGlockWeaponLogic::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pLayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase())
		return;

	// only idle if the slid isn't back
	if (m_iClip != 0)
	{
		int iAnim;
		float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.0f, 1.0f);
		if (flRand <= 0.3 + 0 * 0.75)
		{
			iAnim = GLOCK_IDLE3;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase() + 49.0 / 16;
		}
		else if (flRand <= 0.6 + 0 * 0.875)
		{
			iAnim = GLOCK_IDLE1;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase() + 60.0 / 16.0;
		}
		else
		{
			iAnim = GLOCK_IDLE2;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase() + 40.0 / 16.0;
		}
		SendWeaponAnim( iAnim );
	}
}
