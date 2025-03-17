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

#include "rpg.h"
#include <utility>

#ifndef CLIENT_DLL
#include "rpg_rocket.h"
#include "weapon_rpg.h"
#endif

CRpgWeaponContext::CRpgWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) : 
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_RPG;
	m_iDefaultAmmo = m_pLayer->IsMultiplayer() ? (RPG_DEFAULT_GIVE * 2) : RPG_DEFAULT_GIVE;
	m_fSpotActive = 1;
	m_cActiveRockets = 0;
	m_usRpg = m_pLayer->PrecacheEvent("events/rpg.sc");

#ifndef CLIENT_DLL
	m_pSpot = nullptr;
#endif
}

int CRpgWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(RPG_CLASSNAME);
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = ROCKET_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = RPG_MAX_CLIP;
	p->iSlot = 3;
	p->iPosition = 0;
	p->iId = m_iId;
	p->iFlags = 0;
	p->iWeight = RPG_WEIGHT;
	return 1;
}

bool CRpgWeaponContext::Deploy()
{
	if (m_iClip == 0)
	{
		return DefaultDeploy("models/v_rpg.mdl", "models/p_rpg.mdl", RPG_DRAW_UL, "rpg");
	}
	return DefaultDeploy("models/v_rpg.mdl", "models/p_rpg.mdl", RPG_DRAW1, "rpg");
}

bool CRpgWeaponContext::CanHolster()
{
	if (m_fSpotActive && m_cActiveRockets)
	{
		// can't put away while guiding a missile.
		return FALSE;
	}
	return TRUE;
}

void CRpgWeaponContext::Holster()
{
	m_fInReload = FALSE; // cancel any reload in progress.
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);
	SendWeaponAnim( RPG_HOLSTER1 );

#ifndef CLIENT_DLL
	if (m_pSpot)
	{
		m_pSpot->Killed( NULL, GIB_NEVER );
		m_pSpot = nullptr;
	}
#endif
}

void CRpgWeaponContext::PrimaryAttack()
{
	if (m_iClip)
	{
#ifndef CLIENT_DLL
		// player "shoot" animation
		CRpg *pWeapon = static_cast<CRpg*>(m_pLayer->GetWeaponEntity());
		pWeapon->m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		pWeapon->m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		pWeapon->m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

		UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle );
		Vector vecSrc = pWeapon->m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;
		
		CRpgRocket *pRocket = CRpgRocket::CreateRpgRocket( vecSrc, pWeapon->m_pPlayer->pev->v_angle, pWeapon->m_pPlayer, pWeapon );

		UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle ); // RpgRocket::Create stomps on globals, so redo
		pRocket->SetLocalVelocity( pRocket->GetLocalVelocity() + gpGlobals->v_forward * DotProduct( pWeapon->m_pPlayer->GetAbsVelocity(), gpGlobals->v_forward ));

		// firing RPG no longer turns on the designator. ALT fire is a toggle switch for the LTD.
		// Ken signed up for this as a global change (sjb)
#endif
		WeaponEventParams params;
		params.flags = WeaponEventFlags::NotHost;
		params.eventindex = m_usRpg;
		params.delay = 0.0f;
		params.origin = m_pLayer->GetGunPosition();
		params.angles = m_pLayer->GetViewAngles();
		params.fparam1 = 0.0f;
		params.fparam2 = 0.0f;
		params.iparam1 = 0;
		params.iparam2 = 0;
		params.bparam1 = 0;
		params.bparam2 = 0;

		if (m_pLayer->ShouldRunFuncs()) {
			m_pLayer->PlaybackWeaponEvent(params);
		}

		m_iClip--; 
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.5f;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.5f;
		// m_pPlayer->pev->punchangle.x -= 5;
		ResetEmptySound();
	}
	else
	{
		PlayEmptySound();
	}
	UpdateSpot();
}

void CRpgWeaponContext::SecondaryAttack()
{
	m_fSpotActive = !m_fSpotActive;
#ifndef CLIENT_DLL
	if (!m_fSpotActive && m_pSpot)
	{
		m_pSpot->Killed( NULL, GIB_NORMAL );
		m_pSpot = nullptr;
	}
#endif
	m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.2f;
}

void CRpgWeaponContext::Reload( void )
{
	bool iResult = false;

	if ( m_iClip == 1 )
	{
		// don't bother with any of this if don't need to reload.
		return;
	}

	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) < 1)
		return;

	// because the RPG waits to autoreload when no missiles are active while  the LTD is on, the
	// weapons code is constantly calling into this function, but is often denied because 
	// a) missiles are in flight, but the LTD is on
	// or
	// b) player is totally out of ammo and has nothing to switch to, and should be allowed to
	//    shine the designator around
	//
	// Set the next attack time into the future so that WeaponIdle will get called more often
	// than reload, allowing the RPG LTD to be updated
	
	m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;

	if ( m_cActiveRockets && m_fSpotActive )
	{
		// no reloading when there are active missiles tracking the designator.
		// ward off future autoreload attempts by setting next attack time into the future for a bit. 
		return;
	}

#ifndef CLIENT_DLL
	if (m_pSpot && m_fSpotActive)
	{
		m_pSpot->Suspend( 2.1f );
		m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 2.1f;
	}
#endif

	if (m_iClip == 0)
	{
		iResult = DefaultReload( RPG_MAX_CLIP, RPG_RELOAD, 2 );
	}

	if (iResult)
	{
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f);
	}
}

void CRpgWeaponContext::WeaponIdle( void )
{
	UpdateSpot( );

	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType))
	{
		int iAnim;
		float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.0f, 1.0f);
		if (flRand <= 0.75f || m_fSpotActive)
		{
			if ( m_iClip == 0 )
				iAnim = RPG_IDLE_UL;
			else
				iAnim = RPG_IDLE;

			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 90.0f / 15.0f;
		}
		else
		{
			if ( m_iClip == 0 )
				iAnim = RPG_FIDGET_UL;
			else
				iAnim = RPG_FIDGET;

			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 3.0f;
		}

		ResetEmptySound( );
		SendWeaponAnim( iAnim );
	}
	else
	{
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f;
	}
}

void CRpgWeaponContext::UpdateSpot( void )
{
	if (m_fSpotActive)
	{
#ifndef CLIENT_DLL
		if (!m_pSpot)
		{
			m_pSpot = CLaserSpot::CreateSpot();
		}

		CRpg *pWeapon = static_cast<CRpg*>(m_pLayer->GetWeaponEntity());
		UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle );
		Vector vecSrc = pWeapon->m_pPlayer->GetGunPosition( );
		Vector vecAiming = gpGlobals->v_forward;

		TraceResult tr;
		UTIL_TraceLine ( vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(pWeapon->m_pPlayer->pev), &tr );
		UTIL_SetOrigin( m_pSpot, tr.vecEndPos );
#endif
	}
}
