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
#ifdef CLIENT_DLL
#include "const.h"
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

CBaseWeaponContext::CBaseWeaponContext(IWeaponLayer *funcs) :
	m_pFuncs(funcs),
	m_pWeaponEntity(nullptr)
{
}

CBaseWeaponContext::~CBaseWeaponContext()
{
	if (m_pFuncs) {
		delete m_pFuncs;
	}
}

void CBaseWeaponContext::ItemPostFrame()
{
	if ((m_fInReload) && m_pFuncs->GetPlayerNextAttackTime() <= m_pFuncs->GetWeaponTimeBase())
	{
		// complete the reload. 
		int j = Q_min( iMaxClip() - m_iClip, m_pFuncs->GetPlayerAmmo(m_iPrimaryAmmoType) );	

		// Add them to the clip
		m_iClip += j;
		m_pFuncs->SetPlayerAmmo( m_iPrimaryAmmoType,  m_pFuncs->GetPlayerAmmo(m_iPrimaryAmmoType) - j );

		m_fInReload = FALSE;
	}

	if (m_pFuncs->CheckPlayerButtonFlag(IN_ATTACK2) && m_flNextSecondaryAttack <= m_pFuncs->GetWeaponTimeBase() )
	{
		if ( pszAmmo2() && !m_pFuncs->GetPlayerAmmo(SecondaryAmmoIndex()) )
		{
			m_fFireOnEmpty = TRUE;
		}

		SecondaryAttack();
		m_pFuncs->ClearPlayerButtonFlag(IN_ATTACK2);
	}
	else if (m_pFuncs->CheckPlayerButtonFlag(IN_ATTACK) && m_flNextPrimaryAttack <= m_pFuncs->GetWeaponTimeBase() )
	{
		if ( (m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && !m_pFuncs->GetPlayerAmmo(PrimaryAmmoIndex())) )
		{
			m_fFireOnEmpty = TRUE;
		}

		PrimaryAttack();
	}
	else if ( m_pFuncs->CheckPlayerButtonFlag(IN_RELOAD) && iMaxClip() != WEAPON_NOCLIP && !m_fInReload ) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if ( !(m_pFuncs->CheckPlayerButtonFlag(IN_ATTACK|IN_ATTACK2) ) )
	{
		// no fire buttons down

		m_fFireOnEmpty = FALSE;
#ifndef CLIENT_DLL // we don't need this branch on client side, because client is not responsible for changing weapons
		if ( !IsUseable() && m_flNextPrimaryAttack < m_pFuncs->GetWeaponTimeBase() ) 
		{
			// weapon isn't useable, switch. GetNextBestWeapon exactly does weapon switching
			if ( !(iFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && m_pFuncs->GetNextBestWeapon() )
			{
				m_flNextPrimaryAttack = m_pFuncs->GetWeaponTimeBase() + 0.3;
				return;
			}
		}
		else
#endif
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip == 0 && !(iFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < m_pFuncs->GetWeaponTimeBase() )
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
	m_pFuncs->SetPlayerViewmodel(0);
#ifndef CLIENT_DLL
	m_pWeaponEntity->m_pPlayer->pev->weaponmodel = 0;
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
		if ( m_pFuncs->GetPlayerAmmo( PrimaryAmmoIndex() ) <= 0 && iMaxAmmo1() != -1 )			
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
		bHasAmmo |= (m_pFuncs->GetPlayerAmmo(m_iPrimaryAmmoType) != 0);
	}
	if ( pszAmmo2() )
	{
		bHasAmmo |= (m_pFuncs->GetPlayerAmmo(m_iSecondaryAmmoType) != 0);
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

bool CBaseWeaponContext :: DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal /* = 0 */, int body )
{
	if (!CanDeploy( ))
		return FALSE;

#ifndef CLIENT_DLL
	//m_pWeaponEntity->m_pPlayer->TabulateAmmo();
	m_pWeaponEntity->m_pPlayer->pev->viewmodel = MAKE_STRING(szViewModel);
	m_pWeaponEntity->m_pPlayer->pev->weaponmodel = MAKE_STRING(szWeaponModel);
	strcpy( m_pWeaponEntity->m_pPlayer->m_szAnimExtention, szAnimExt );
#else
	// gEngfuncs.CL_LoadModel( szViewModel, &m_pPlayer->pev->viewmodel );
#endif
	SendWeaponAnim( iAnim, skiplocal, body );

	m_pFuncs->SetPlayerNextAttackTime(m_pFuncs->GetWeaponTimeBase() + 0.5);
	m_flTimeWeaponIdle = m_pFuncs->GetWeaponTimeBase() + 1.0;
	return TRUE;
}

BOOL CBaseWeaponContext :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body )
{
	if (m_pFuncs->GetPlayerAmmo(m_iPrimaryAmmoType) <= 0)
		return FALSE;

	int j = Q_min(iClipSize - m_iClip, m_pFuncs->GetPlayerAmmo(m_iPrimaryAmmoType));	

	if (j == 0)
		return FALSE;

	m_pFuncs->SetPlayerNextAttackTime(m_pFuncs->GetWeaponTimeBase() + fDelay);

	//!!UNDONE -- reload sound goes here !!!
	SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0, body );

	m_fInReload = TRUE;

	m_flTimeWeaponIdle = m_pFuncs->GetWeaponTimeBase() + 3;
	return TRUE;
}

void CBaseWeaponContext::SendWeaponAnim( int iAnim, int skiplocal, int body )
{
	m_pFuncs->SetPlayerWeaponAnim(iAnim);

#ifndef CLIENT_DLL
	if ( UseDecrement() )
		skiplocal = 1;
	else
		skiplocal = 0;

	if ( skiplocal && ENGINE_CANSKIP( m_pWeaponEntity->m_pPlayer->edict() ) )
		return;

	MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, m_pWeaponEntity->m_pPlayer->pev );
		WRITE_BYTE( iAnim );						// sequence number
		WRITE_BYTE( m_pFuncs->GetWeaponBodygroup() );					// weaponmodel bodygroup.
	MESSAGE_END();
#else
	// HUD_SendWeaponAnim( iAnim, body, 0 );
#endif
}

bool CBaseWeaponContext :: PlayEmptySound()
{
	if (m_iPlayEmptySound)
	{
#ifdef CLIENT_DLL
		// HUD_PlaySound( "weapons/357_cock1.wav", 0.8 );
#else
		EMIT_SOUND(ENT(m_pWeaponEntity->m_pPlayer->pev), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM);
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
