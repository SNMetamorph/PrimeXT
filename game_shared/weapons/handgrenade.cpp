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

#include "handgrenade.h"
#include <utility>

#ifndef CLIENT_DLL
#include "weapon_handgrenade.h"
#endif

#define HANDGRENADE_PRIMARY_VOLUME	450

enum handgrenade_e
{
	HANDGRENADE_IDLE = 0,
	HANDGRENADE_FIDGET,
	HANDGRENADE_PINPULL,
	HANDGRENADE_THROW1,	// toss
	HANDGRENADE_THROW2,	// medium
	HANDGRENADE_THROW3,	// hard
	HANDGRENADE_HOLSTER,
	HANDGRENADE_DRAW
};

CHandGrenadeWeaponContext::CHandGrenadeWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_HANDGRENADE;
	m_iDefaultAmmo = HANDGRENADE_DEFAULT_GIVE;
	m_flReleaseThrow = 0.0f;
	m_flStartThrow = 0.0f;
}

int CHandGrenadeWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(HANDGRENADE_CLASSNAME);
	p->pszAmmo1 = "Hand Grenade";
	p->iMaxAmmo1 = HANDGRENADE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iId = m_iId;
	p->iWeight = HANDGRENADE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

bool CHandGrenadeWeaponContext::Deploy()
{
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_grenade.mdl", "models/p_grenade.mdl", HANDGRENADE_DRAW, "crowbar" );
}

bool CHandGrenadeWeaponContext::CanHolster()
{
	// can only holster hand grenades when not primed!
	return ( m_flStartThrow == 0 );
}

void CHandGrenadeWeaponContext::Holster()
{
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) > 0)
	{
		SendWeaponAnim( HANDGRENADE_HOLSTER );
	}
	else
	{
#ifndef CLIENT_DLL
		// no more grenades!
		CHandGrenade *pWeapon = static_cast<CHandGrenade*>(m_pLayer->GetWeaponEntity());
		pWeapon->m_pPlayer->RemoveWeapon( WEAPON_HANDGRENADE );
		pWeapon->SetThink( &CHandGrenade::DestroyItem );
		pWeapon->pev->nextthink = gpGlobals->time + 0.1f;
#endif
	}

	// EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

void CHandGrenadeWeaponContext::PrimaryAttack()
{
	if ( !m_flStartThrow && m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) > 0 )
	{
		m_flStartThrow = m_pLayer->GetTime();
		m_flReleaseThrow = 0;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5;
		SendWeaponAnim( HANDGRENADE_PINPULL );
	}
}

void CHandGrenadeWeaponContext::WeaponIdle( void )
{
	if ( m_flReleaseThrow == 0 && m_flStartThrow )
		 m_flReleaseThrow = m_pLayer->GetTime();

	if ( m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()) )
		return;
	
	if ( m_flStartThrow )
	{
		Vector angThrow = m_pLayer->GetViewAngles(); // + m_pPlayer->pev->punchangle;

		if ( angThrow.x < 0 )
			angThrow.x = -10 + angThrow.x * ( ( 90 - 10 ) / 90.0 );
		else
			angThrow.x = -10 + angThrow.x * ( ( 90 + 10 ) / 90.0 );

		const float flMultiplier = 6.5f;
		float flVel = ( 90 - angThrow.x ) * flMultiplier;
		if ( flVel > 1000 )
			flVel = 1000;

#ifndef CLIENT_DLL
		CHandGrenade *pWeapon = static_cast<CHandGrenade*>(m_pLayer->GetWeaponEntity());

		UTIL_MakeVectors( angThrow );
		Vector vecSrc = pWeapon->m_pPlayer->pev->origin + pWeapon->m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;
		Vector vecThrow = gpGlobals->v_forward * flVel + pWeapon->m_pPlayer->pev->velocity;

		// alway explode 3 seconds after the pin was pulled
		float time = m_flStartThrow - gpGlobals->time + 3.0;
		if (time < 0)
			time = 0;

		CGrenade::ShootTimed( pWeapon->m_pPlayer->pev, vecSrc, vecThrow, time );

		// player "shoot" animation
		pWeapon->m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
#endif

		if ( flVel < 500 )
		{
			SendWeaponAnim( HANDGRENADE_THROW1 );
		}
		else if ( flVel < 1000 )
		{
			SendWeaponAnim( HANDGRENADE_THROW2 );
		}
		else
		{
			SendWeaponAnim( HANDGRENADE_THROW3 );
		}

		m_flReleaseThrow = 0;
		m_flStartThrow = 0;
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;

		m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - 1);

		if ( m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) < 1 )
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;// ensure that the animation can finish playing
		}
		return;
	}
	else if ( m_flReleaseThrow > 0 )
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if ( m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) > 0 )
		{
			SendWeaponAnim( HANDGRENADE_DRAW );
		}
		else
		{
#ifndef CLIENT_DLL
			CHandGrenade *pWeapon = static_cast<CHandGrenade*>(m_pLayer->GetWeaponEntity());
			pWeapon->RetireWeapon();
#endif
			return;
		}

		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f);
		m_flReleaseThrow = -1;
		return;
	}

	if ( m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) > 0 )
	{
		int iAnim;
		float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.0f, 1.0f);
		if (flRand <= 0.75f)
		{
			iAnim = HANDGRENADE_IDLE;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f); // how long till we do this again.
		}
		else 
		{
			iAnim = HANDGRENADE_FIDGET;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 75.0f / 30.0f;
		}

		SendWeaponAnim( iAnim );
	}
}
