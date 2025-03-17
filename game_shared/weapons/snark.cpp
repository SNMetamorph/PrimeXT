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

#include "snark.h"
#include <utility>

#ifndef CLIENT_DLL
#include "monster_snark.h"
#include "weapon_snark.h"
#endif

CSqueakWeaponContext::CSqueakWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_SNARK;
	m_iDefaultAmmo = SNARK_DEFAULT_GIVE;
	m_fJustThrown = false;
	m_usSnarkFire = m_pLayer->PrecacheEvent("events/snarkfire.sc");
}

int CSqueakWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(SNARK_CLASSNAME);
	p->pszAmmo1 = "Snarks";
	p->iMaxAmmo1 = SNARK_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 3;
	p->iId = m_iId;
	p->iWeight = SNARK_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

bool CSqueakWeaponContext::Deploy()
{
#ifndef CLIENT_DLL
	// play hunt sound
	float flRndSound = RANDOM_FLOAT ( 0 , 1 );
	CSqueak *pWeapon = static_cast<CSqueak*>(m_pLayer->GetWeaponEntity());

	if ( flRndSound <= 0.5 )
		EMIT_SOUND_DYN(ENT(pWeapon->pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 100);
	else 
		EMIT_SOUND_DYN(ENT(pWeapon->pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 100);

	pWeapon->m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
#endif
	return DefaultDeploy( "models/v_squeak.mdl", "models/p_squeak.mdl", SQUEAK_UP, "squeak" );
}

void CSqueakWeaponContext::Holster()
{
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5);
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) < 1)
	{
#ifndef CLIENT_DLL
		CSqueak *pWeapon = static_cast<CSqueak*>(m_pLayer->GetWeaponEntity());
		pWeapon->m_pPlayer->RemoveWeapon( WEAPON_SNARK );
		pWeapon->SetThink( &CSqueak::DestroyItem );
		pWeapon->pev->nextthink = gpGlobals->time + 0.1;
#endif
		return;
	}
	
	SendWeaponAnim( SQUEAK_DOWN );
	// EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

void CSqueakWeaponContext::PrimaryAttack()
{
	if (m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()))
	{
#ifndef CLIENT_DLL
		TraceResult tr;
		CSqueak *pWeapon = static_cast<CSqueak*>(m_pLayer->GetWeaponEntity());

		// find place to toss monster
		UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle );
		UTIL_TraceLine( pWeapon->m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 20, pWeapon->m_pPlayer->GetAbsOrigin() + gpGlobals->v_forward * 64, dont_ignore_monsters, NULL, &tr );

		if (tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0.25)
		{
			SendWeaponAnim( SQUEAK_THROW );

			// player "shoot" animation
			pWeapon->m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

			CBaseEntity *pSqueak = CBaseEntity::Create( "monster_snark", tr.vecEndPos, pWeapon->m_pPlayer->pev->v_angle, pWeapon->m_pPlayer->edict() );
			Vector vecGround;

			if( pWeapon->m_pPlayer->GetGroundEntity( ))
				vecGround = pWeapon->m_pPlayer->GetGroundEntity( )->GetAbsVelocity();
			else vecGround = g_vecZero;

			pSqueak->SetAbsVelocity( gpGlobals->v_forward * 200 + pWeapon->m_pPlayer->GetAbsVelocity( ) + vecGround );

			// play hunt sound
			float flRndSound = RANDOM_FLOAT ( 0 , 1 );

			if ( flRndSound <= 0.5 )
				EMIT_SOUND_DYN(ENT(pWeapon->pev), CHAN_VOICE, "squeek/sqk_hunt2.wav", 1, ATTN_NORM, 0, 105);
			else 
				EMIT_SOUND_DYN(ENT(pWeapon->pev), CHAN_VOICE, "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 105);

			pWeapon->m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
			m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - 1);
		}
#endif
		WeaponEventParams params;
		params.flags = WeaponEventFlags::NotHost;
		params.eventindex = m_usSnarkFire;
		params.delay = 0.0f;
		params.origin = m_pLayer->GetGunPosition();
		params.angles = m_pLayer->GetCameraOrientation();
		params.fparam1 = 0.0f;
		params.fparam2 = 0.0f;
		params.iparam1 = 0;
		params.iparam2 = 0;
		params.bparam1 = 0;
		params.bparam2 = 0;

		if (m_pLayer->ShouldRunFuncs()) {
			m_pLayer->PlaybackWeaponEvent(params);
		}

		m_fJustThrown = true;
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.3f;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f;
	}
}

void CSqueakWeaponContext::WeaponIdle()
{
	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	if (m_fJustThrown)
	{
		m_fJustThrown = false;
		if (!m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()))
		{
#ifndef CLIENT_DLL
			CSqueak *pWeapon = static_cast<CSqueak*>(m_pLayer->GetWeaponEntity());
			pWeapon->RetireWeapon();
#endif
			return;
		}

		SendWeaponAnim(SQUEAK_UP);
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
		return;
	}

	int iAnim;
	float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.f, 1.f);
	if (flRand <= 0.75)
	{
		iAnim = SQUEAK_IDLE1;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = SQUEAK_FIDGETFIT;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 70.0 / 16.0;
	}
	else
	{
		iAnim = SQUEAK_FIDGETNIP;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 80.0 / 16.0;
	}
	SendWeaponAnim(iAnim);
}
