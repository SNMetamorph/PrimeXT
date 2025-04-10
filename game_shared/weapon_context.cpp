/*
weapon_context.cpp - part of weapons implementation common for client & server
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

#include "weapon_context.h"
#include <cmath>
#include <utility>

#ifdef CLIENT_DLL
#include "const.h"
#include "hud.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"
#else
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"
#endif

ItemInfo CBaseWeaponContext::ItemInfoArray[ MAX_WEAPONS ];
AmmoInfo CBaseWeaponContext::AmmoInfoArray[ MAX_AMMO_SLOTS ];

CBaseWeaponContext::CBaseWeaponContext(std::unique_ptr<IWeaponLayer> &&layer) :
	m_pLayer(std::move(layer)),
	m_fFireOnEmpty(false),
	m_fInReload(false),
	m_fInSpecialReload(false),
	m_flNextPrimaryAttack(0.0f),
	m_flNextSecondaryAttack(0.0f),
	m_flPrevPrimaryAttack(0.0f),
	m_flLastFireTime(0.0f),
	m_flPumpTime(0.0f),
	m_flTimeWeaponIdle(0.0f),
	m_iClientClip(0),
	m_iClientWeaponState(0),
	m_iClip(0),
	m_iDefaultAmmo(0),
	m_iPlayEmptySound(false),
	m_iPrimaryAmmoType(0),
	m_iSecondaryAmmoType(0),
	m_iId(-1)
{
}

CBaseWeaponContext::~CBaseWeaponContext()
{
}

void CBaseWeaponContext::ItemPostFrame()
{
	if ((m_fInReload) && m_pLayer->GetPlayerNextAttackTime() <= m_pLayer->GetWeaponTimeBase(false))
	{
		// complete the reload. 
		int j = Q_min( iMaxClip() - m_iClip, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) );	

		// Add them to the clip
		m_iClip += j;
		m_pLayer->SetPlayerAmmo( m_iPrimaryAmmoType,  m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - j );

		m_fInReload = FALSE;
	}

	if (!m_pLayer->CheckPlayerButtonFlag(IN_ATTACK))
	{
		m_flLastFireTime = 0.0f;
	}

	if (m_pLayer->CheckPlayerButtonFlag(IN_ATTACK2) && CanAttack(m_flNextSecondaryAttack))
	{
		if ( pszAmmo2() && !m_pLayer->GetPlayerAmmo(SecondaryAmmoIndex()) )
		{
			m_fFireOnEmpty = TRUE;
		}

		SecondaryAttack();
		m_pLayer->ClearPlayerButtonFlag(IN_ATTACK2);
	}
	else if (m_pLayer->CheckPlayerButtonFlag(IN_ATTACK) && CanAttack(m_flNextPrimaryAttack))
	{
		if ( (m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && !m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex())) )
		{
			m_fFireOnEmpty = TRUE;
		}

		PrimaryAttack();
	}
	else if ( m_pLayer->CheckPlayerButtonFlag(IN_RELOAD) && iMaxClip() != WEAPON_NOCLIP && !m_fInReload ) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if ( !(m_pLayer->CheckPlayerButtonFlag(IN_ATTACK|IN_ATTACK2) ) )
	{
		// no fire buttons down

		m_fFireOnEmpty = FALSE;
#ifndef CLIENT_DLL // we don't need this branch on client side, because client is not responsible for changing weapons
		if ( !IsUseable() && m_flNextPrimaryAttack < m_pLayer->GetWeaponTimeBase(UsePredicting()) ) 
		{
			// weapon isn't useable, switch. GetNextBestWeapon does weapon switching
			if ( !(iFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && g_pGameRules->GetNextBestWeapon( m_pLayer->GetWeaponEntity()->m_pPlayer, m_pLayer->GetWeaponEntity() ))
			{
				m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.3;
				return;
			}
		}
		else
#endif
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip == 0 && !(iFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < m_pLayer->GetWeaponTimeBase(UsePredicting()) )
			{
				Reload();
				return;
			}
		}

		WeaponIdle( );
		return;
	}
	
	// catch all
	if ( ShouldWeaponIdle() )
	{
		WeaponIdle();
	}
}

void CBaseWeaponContext::Holster()
{ 
	m_fInReload = FALSE; // cancel any reload in progress.
	m_pLayer->DisablePlayerViewmodel();
#ifndef CLIENT_DLL
	m_pLayer->GetWeaponEntity()->m_pPlayer->pev->weaponmodel = 0;
#endif
}

//=========================================================
// IsUseable - this function determines whether or not a 
// weapon is useable by the player in its current state. 
// (does it have ammo loaded? do I have any ammo for the 
// weapon?, etc)
//=========================================================
bool CBaseWeaponContext :: IsUseable()
{
	if ( m_iClip <= 0 )
	{
		if ( m_pLayer->GetPlayerAmmo( PrimaryAmmoIndex() ) <= 0 && iMaxAmmo1() != -1 )			
		{
			// clip is empty (or nonexistant) and the player has no more ammo of this type. 
			return FALSE;
		}
	}

	return TRUE;
}

bool CBaseWeaponContext :: CanDeploy()
{
	BOOL bHasAmmo = 0;

	if ( !pszAmmo1() )
	{
		// this weapon doesn't use ammo, can always deploy.
		return TRUE;
	}

	if ( pszAmmo1() )
	{
		bHasAmmo |= (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) != 0);
	}
	if ( pszAmmo2() )
	{
		bHasAmmo |= (m_pLayer->GetPlayerAmmo(m_iSecondaryAmmoType) != 0);
	}
	if (m_iClip > 0)
	{
		bHasAmmo |= 1;
	}
	if (!bHasAmmo)
	{
		return FALSE;
	}

	return TRUE;
}

bool CBaseWeaponContext :: DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int body )
{
	if (!CanDeploy( ))
		return FALSE;

#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	player->pev->weaponmodel = MAKE_STRING(szWeaponModel);
	strcpy( player->m_szAnimExtention, szAnimExt );
#endif
	m_pLayer->SetPlayerViewmodel(szViewModel);
	SendWeaponAnim( iAnim, body );

	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5);
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0;
	m_flLastFireTime = 0.0f;
	return TRUE;
}

BOOL CBaseWeaponContext :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body )
{
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) <= 0)
		return FALSE;

	int j = Q_min(iClipSize - m_iClip, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType));	

	if (j == 0)
		return FALSE;

	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + fDelay);

	//!!UNDONE -- reload sound goes here !!!
	SendWeaponAnim( iAnim, body );

	m_fInReload = TRUE;

	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 3;
	return TRUE;
}

void CBaseWeaponContext::SendWeaponAnim( int iAnim, int body )
{
	m_pLayer->SetPlayerWeaponAnim(iAnim);

#ifdef CLIENT_DLL
	if (m_pLayer->ShouldRunFuncs()) {
		gEngfuncs.pfnWeaponAnim(iAnim, body);
	}
#else
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;

	if ( UsePredicting() && ENGINE_CANSKIP( player->edict() ) )
		return;

	MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, player->pev );
		WRITE_BYTE( iAnim );							// sequence number
		WRITE_BYTE( m_pLayer->GetWeaponBodygroup() );	// weaponmodel bodygroup.
	MESSAGE_END();
#endif
}

float CBaseWeaponContext::GetNextPrimaryAttackDelay(float delay)
{
	if (m_flLastFireTime <= (0.0f + FLT_EPSILON) || m_flNextPrimaryAttack <= (-1.0f + FLT_EPSILON))
 	{ 
 		// at this point, we are assuming that the client has stopped firing 
 		// and we are going to reset our book keeping variables. 
 		m_flLastFireTime = m_pLayer->GetTime(); // maybe we should use actual time instead of predicted? not obvious
 		m_flPrevPrimaryAttack = delay; 
 	} 

 	// calculate the time between this shot and the previous 
 	float flTimeBetweenFires = m_pLayer->GetTime() - m_flLastFireTime; 
 	float flCreep = 0.0f; 

	if (flTimeBetweenFires > 0.0f) {
		flCreep = flTimeBetweenFires - m_flPrevPrimaryAttack;
	}
 	 
 	m_flLastFireTime = m_pLayer->GetTime();		 
 	 
	// we need to remember what the m_flNextPrimaryAttack time is set to for each shot,
 	// store it as m_flPrevPrimaryAttack. 
 	float flNextAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + delay - flCreep; 
 	m_flPrevPrimaryAttack = flNextAttack - m_pLayer->GetWeaponTimeBase(UsePredicting()); 
 	return flNextAttack; 
}

bool CBaseWeaponContext::CanAttack(float attack_time)
{
	return (static_cast<int>(std::floor(attack_time * 1000.0)) * 1000.0f) <= m_pLayer->GetWeaponTimeBase(UsePredicting());
}

bool CBaseWeaponContext :: PlayEmptySound()
{
	if (m_iPlayEmptySound)
	{
#ifdef CLIENT_DLL
		if (m_pLayer->ShouldRunFuncs()) {
			gEngfuncs.pfnPlaySoundByNameAtLocation("weapons/357_cock1.wav", 0.8, m_pLayer->GetGunPosition());
		}
#else
		EMIT_SOUND(ENT(m_pLayer->GetWeaponEntity()->m_pPlayer->pev), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM);
#endif
		m_iPlayEmptySound = 0;
		return 0;
	}
	return 0;
}

void CBaseWeaponContext :: ResetEmptySound()
{
	m_iPlayEmptySound = 1;
}

int CBaseWeaponContext::PrimaryAmmoIndex()
{
	return m_iPrimaryAmmoType;
}

int CBaseWeaponContext::SecondaryAmmoIndex()
{
	return -1;
}

int CBaseWeaponContext::iItemSlot()				{ return 0; }	// return 0 to MAX_ITEMS_SLOTS, used in hud
int	CBaseWeaponContext::iItemPosition() 		{ return CBaseWeaponContext::ItemInfoArray[ m_iId ].iPosition; }
const char *CBaseWeaponContext::pszAmmo1() 		{ return CBaseWeaponContext::ItemInfoArray[ m_iId ].pszAmmo1; }
int CBaseWeaponContext::iMaxAmmo1() 			{ return CBaseWeaponContext::ItemInfoArray[ m_iId ].iMaxAmmo1; }
const char *CBaseWeaponContext::pszAmmo2() 		{ return CBaseWeaponContext::ItemInfoArray[ m_iId ].pszAmmo2; }
int	CBaseWeaponContext::iMaxAmmo2() 			{ return CBaseWeaponContext::ItemInfoArray[ m_iId ].iMaxAmmo2; }
const char *CBaseWeaponContext::pszName() 		{ return CBaseWeaponContext::ItemInfoArray[ m_iId ].pszName; }
int	CBaseWeaponContext::iMaxClip() 				{ return CBaseWeaponContext::ItemInfoArray[ m_iId ].iMaxClip; }
int	CBaseWeaponContext::iWeight() 				{ return CBaseWeaponContext::ItemInfoArray[ m_iId ].iWeight; }
int CBaseWeaponContext::iFlags() 				{ return CBaseWeaponContext::ItemInfoArray[ m_iId ].iFlags; }
