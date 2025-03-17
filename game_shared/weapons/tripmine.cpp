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

#include "tripmine.h"
#include <utility>

#ifndef CLIENT_DLL
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "weapon_tripmine.h"
#endif

#define TRIPMINE_PRIMARY_VOLUME	450

CTripmineWeaponContext::CTripmineWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_TRIPMINE;
	m_iDefaultAmmo = TRIPMINE_DEFAULT_GIVE;
	m_usTripFire = m_pLayer->PrecacheEvent("events/tripfire.sc");
}

int CTripmineWeaponContext::GetItemInfo(ItemInfo *p)
{
	p->pszName = CLASSNAME_STR(TRIPMINE_CLASSNAME);
	p->pszAmmo1 = "Trip Mine";
	p->iMaxAmmo1 = TRIPMINE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 2;
	p->iId = m_iId;
	p->iWeight = TRIPMINE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

bool CTripmineWeaponContext::Deploy( )
{
	m_pLayer->SetWeaponBodygroup(0);
	return DefaultDeploy( "models/v_tripmine.mdl", "models/p_tripmine.mdl", TRIPMINE_DRAW, "trip" );
}

void CTripmineWeaponContext::Holster( void )
{
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);

	if (!m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType))
	{
#ifndef CLIENT_DLL
		// out of mines
		CTripmine *pWeapon = static_cast<CTripmine*>(m_pLayer->GetWeaponEntity());
		pWeapon->m_pPlayer->RemoveWeapon( WEAPON_TRIPMINE );
		pWeapon->SetThink( &CBasePlayerItem::DestroyItem );
		pWeapon->pev->nextthink = gpGlobals->time + 0.1;
#endif
	}

	SendWeaponAnim( TRIPMINE_HOLSTER );
	// EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

void CTripmineWeaponContext::PrimaryAttack( void )
{
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) <= 0)
		return;

	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usTripFire;
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

#ifndef CLIENT_DLL
	TraceResult tr;
	CTripmine *pWeapon = static_cast<CTripmine*>(m_pLayer->GetWeaponEntity());
	UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle + pWeapon->m_pPlayer->pev->punchangle );
	Vector vecSrc = pWeapon->m_pPlayer->GetGunPosition( );
	Vector vecAiming = gpGlobals->v_forward;

	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 128, ignore_monsters, pWeapon->m_pPlayer->edict(), &tr );

	if (tr.flFraction < 1.0)
	{
		// ALERT( at_console, "hit %f\n", tr.flFraction );

		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if (pEntity && (pEntity->IsBSPModel() || pEntity->IsCustomModel()) && !(pEntity->pev->flags & FL_CONVEYOR))
		{
			Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );

			CBaseEntity *pEnt = CBaseEntity::Create( "monster_tripmine", tr.vecEndPos + tr.vecPlaneNormal * 8, angles, pWeapon->m_pPlayer->edict() );

			// g-cont. attach tripmine to the wall
			// NOTE: we should always attach the tripmine because our parent it's our owner too
			if( pEntity != g_pWorld )
				pEnt->SetParent( pEntity );

			pWeapon->m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

			// player "shoot" animation
			pWeapon->m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

			if (pWeapon->m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
			{
				SendWeaponAnim( TRIPMINE_DRAW );
			}
			else
			{
				// no more mines! 
				pWeapon->RetireWeapon();
				return;
			}
		}
		else
		{
			// ALERT( at_console, "no deploy\n" );
		}
	}
#endif
	m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.3f;
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f);
}

void CTripmineWeaponContext::WeaponIdle()
{
	if ( m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()) )
		return;

	if ( m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) > 0 )
	{
		SendWeaponAnim( TRIPMINE_DRAW );
	}
	else
	{
#ifndef CLIENT_DLL
		CTripmine *pWeapon = static_cast<CTripmine*>(m_pLayer->GetWeaponEntity());
		pWeapon->RetireWeapon();
#endif
		return;
	}

	int iAnim;
	float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.0f, 1.0f);
	if (flRand <= 0.25)
	{
		iAnim = TRIPMINE_IDLE1;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 90.0 / 30.0;
	}
	else if (flRand <= 0.75)
	{
		iAnim = TRIPMINE_IDLE2;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 60.0 / 30.0;
	}
	else
	{
		iAnim = TRIPMINE_FIDGET;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 100.0 / 30.0;
	}

	SendWeaponAnim( iAnim );
}
