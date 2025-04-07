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

#include "hornetgun.h"

#ifndef CLIENT_DLL
#include "weapon_hornetgun.h"
#endif

enum firemode_e
{
	FIREMODE_TRACK = 0,
	FIREMODE_FAST
};

CHornetgunWeaponContext::CHornetgunWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_HORNETGUN;
	m_iDefaultAmmo = HIVEHAND_DEFAULT_GIVE;
	m_iFirePhase = 0;
	m_usHornetFire = m_pLayer->PrecacheEvent("events/firehornet.sc");
	m_flRechargeTime = 0.0f;
}

int CHornetgunWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(HORNETGUN_CLASSNAME);
	p->pszAmmo1 = "Hornets";
	p->iMaxAmmo1 = HORNET_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 3;
	p->iId = m_iId;
	p->iFlags = ITEM_FLAG_NOAUTOSWITCHEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iWeight = HORNETGUN_WEIGHT;
	return 1;
}

bool CHornetgunWeaponContext::Deploy( void )
{
	return DefaultDeploy( "models/v_hgun.mdl", "models/p_hgun.mdl", HGUN_UP, "hive" );
}

void CHornetgunWeaponContext::Holster( void )
{
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);
	SendWeaponAnim(HGUN_DOWN);

	//!!!HACKHACK - can't select hornetgun if it's empty! no way to get ammo for it, either.
	if (!m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex())) {
		m_pLayer->SetPlayerAmmo(PrimaryAmmoIndex(), 1);
	}
}

void CHornetgunWeaponContext::PrimaryAttack( void )
{
	Reload();

	if (m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) < 1)
	{
		return;
	}

#ifndef CLIENT_DLL
	CHornetgun *pWeapon = static_cast<CHornetgun*>(m_pLayer->GetWeaponEntity());
	UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle );

	CBaseEntity *pHornet = CBaseEntity::Create( "hornet", pWeapon->m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12, pWeapon->m_pPlayer->pev->v_angle, pWeapon->m_pPlayer->edict() );
	pHornet->pev->velocity = gpGlobals->v_forward * 300;

	m_flRechargeTime = gpGlobals->time + GetRechargeTime();

	// player "shoot" animation
	pWeapon->m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pWeapon->m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	pWeapon->m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
#endif

	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usHornetFire;
	params.delay = 0.0f;
	params.origin = m_pLayer->GetGunPosition();
	params.angles = m_pLayer->GetViewAngles();
	params.fparam1 = 0.0f;
	params.fparam2 = 0.0f;
	params.iparam1 = FIREMODE_TRACK;
	params.iparam2 = 0;
	params.bparam1 = 0;
	params.bparam2 = 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

	m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - 1);
	m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.25f;

	if (m_flNextPrimaryAttack < m_pLayer->GetWeaponTimeBase(UsePredicting()))
	{
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.25f;
	}
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) < 1)
	{
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + GetRechargeTime();
	}
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f);
}

void CHornetgunWeaponContext::SecondaryAttack( void )
{
	Reload();

	if (m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) <= 0)
	{
		return;
	}

#ifndef CLIENT_DLL
	Vector vecSrc;
	CBaseEntity *pHornet;
	CHornetgun *pWeapon = static_cast<CHornetgun*>(m_pLayer->GetWeaponEntity());
	
	UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle );
	vecSrc = pWeapon->m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12;

	m_iFirePhase++;
	switch ( m_iFirePhase )
	{
	case 1:
		vecSrc = vecSrc + gpGlobals->v_up * 8;
		break;
	case 2:
		vecSrc = vecSrc + gpGlobals->v_up * 8;
		vecSrc = vecSrc + gpGlobals->v_right * 8;
		break;
	case 3:
		vecSrc = vecSrc + gpGlobals->v_right * 8;
		break;
	case 4:
		vecSrc = vecSrc + gpGlobals->v_up * -8;
		vecSrc = vecSrc + gpGlobals->v_right * 8;
		break;
	case 5:
		vecSrc = vecSrc + gpGlobals->v_up * -8;
		break;
	case 6:
		vecSrc = vecSrc + gpGlobals->v_up * -8;
		vecSrc = vecSrc + gpGlobals->v_right * -8;
		break;
	case 7:
		vecSrc = vecSrc + gpGlobals->v_right * -8;
		break;
	case 8:
		vecSrc = vecSrc + gpGlobals->v_up * 8;
		vecSrc = vecSrc + gpGlobals->v_right * -8;
		m_iFirePhase = 0;
		break;
	}

	pHornet = CBaseEntity::Create( "hornet", vecSrc, pWeapon->m_pPlayer->pev->v_angle, pWeapon->m_pPlayer->edict() );
	pHornet->SetAbsVelocity( gpGlobals->v_forward * 1200 );
	pHornet->SetAbsAngles( UTIL_VecToAngles( pHornet->GetAbsVelocity() ));
	pHornet->SetThink( &CHornet::StartDart );

	// player "shoot" animation
	pWeapon->m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	pWeapon->m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	pWeapon->m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	m_flRechargeTime = gpGlobals->time + GetRechargeTime();
#endif

	m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - 1);

	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usHornetFire;
	params.delay = 0.0f;
	params.origin = m_pLayer->GetGunPosition();
	params.angles = m_pLayer->GetViewAngles();
	params.fparam1 = 0.0f;
	params.fparam2 = 0.0f;
	params.iparam1 = FIREMODE_FAST;
	params.iparam2 = 0;
	params.bparam1 = 0;
	params.bparam2 = 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.1f;
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) < 1)
	{
#ifndef CLIENT_DLL
		m_flRechargeTime = gpGlobals->time + GetRechargeTime();
#endif
		m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
	}
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f);
}

void CHornetgunWeaponContext::Reload( void )
{
	if (m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) >= HORNET_MAX_CARRY)
		return;

#ifndef CLIENT_DLL
	while (m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) < HORNET_MAX_CARRY && m_flRechargeTime < gpGlobals->time)
	{
		CHornetgun *pWeapon = static_cast<CHornetgun*>(m_pLayer->GetWeaponEntity());
		pWeapon->m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]++;
		m_flRechargeTime = gpGlobals->time + GetRechargeTime();
	}
#endif
}

void CHornetgunWeaponContext::WeaponIdle( void )
{
	Reload();

	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	int iAnim;
	float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.0f, 1.0f);
	if (flRand <= 0.75)
	{
		iAnim = HGUN_IDLE1;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = HGUN_FIDGETSWAY;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 40.0 / 16.0;
	}
	else
	{
		iAnim = HGUN_FIDGETSHAKE;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 35.0 / 16.0;
	}
	SendWeaponAnim( iAnim );
}

float CHornetgunWeaponContext::GetRechargeTime() const
{
	return m_pLayer->IsMultiplayer() ? 0.3f : 0.5f;
}
