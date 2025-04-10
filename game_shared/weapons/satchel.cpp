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

#include "satchel.h"
#include <utility>

#ifndef CLIENT_DLL
#include "monster_satchel.h"
#include "weapon_satchel.h"
#endif

enum satchel_e
{
	SATCHEL_IDLE1 = 0,
	SATCHEL_FIDGET1,
	SATCHEL_DRAW,
	SATCHEL_DROP
};

enum satchel_radio_e
{
	SATCHEL_RADIO_IDLE1 = 0,
	SATCHEL_RADIO_FIDGET1,
	SATCHEL_RADIO_DRAW,
	SATCHEL_RADIO_FIRE,
	SATCHEL_RADIO_HOLSTER
};

CSatchelWeaponContext::CSatchelWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_SATCHEL;
	m_iDefaultAmmo = SATCHEL_DEFAULT_GIVE;
	m_chargeReady = 0;
}

int CSatchelWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(SATCHEL_CLASSNAME);
	p->pszAmmo1 = "Satchel Charge";
	p->iMaxAmmo1 = SATCHEL_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 1;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	p->iId = m_iId;
	p->iWeight = SATCHEL_WEIGHT;
	return 1;
}

//=========================================================
//=========================================================
bool CSatchelWeaponContext::IsUseable( void )
{
	if ( m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) > 0 ) 
	{
		// player is carrying some satchels
		return TRUE;
	}

	if ( m_chargeReady != 0 )
	{
		// player isn't carrying any satchels, but has some out
		return TRUE;
	}

	return FALSE;
}

bool CSatchelWeaponContext::CanDeploy( void )
{
	if (m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) > 0) 
	{
		// player is carrying some satchels
		return TRUE;
	}

	if ( m_chargeReady != 0 )
	{
		// player isn't carrying any satchels, but has some out
		return TRUE;
	}

	return FALSE;
}

bool CSatchelWeaponContext::Deploy( void )
{
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f);
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f);

	if ( m_chargeReady )
		return DefaultDeploy( "models/v_satchel_radio.mdl", "models/p_satchel_radio.mdl", SATCHEL_RADIO_DRAW, "hive" );
	return DefaultDeploy( "models/v_satchel.mdl", "models/p_satchel.mdl", SATCHEL_DRAW, "trip" );
}

void CSatchelWeaponContext::Holster( void )
{
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);
	
	SendWeaponAnim( m_chargeReady ? SATCHEL_RADIO_HOLSTER : SATCHEL_DROP );

#ifndef CLIENT_DLL
	CSatchel *pWeapon = static_cast<CSatchel*>(m_pLayer->GetWeaponEntity());
	EMIT_SOUND(ENT(pWeapon->m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0f, ATTN_NORM);
#endif

	if ( !m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) && !m_chargeReady )
	{
#ifndef CLIENT_DLL
		pWeapon->m_pPlayer->RemoveWeapon( WEAPON_SATCHEL );
		pWeapon->SetThink( &CSatchel::DestroyItem );
		pWeapon->pev->nextthink = gpGlobals->time + 0.1f;
#endif
	}
}

void CSatchelWeaponContext::PrimaryAttack( void )
{
	if (m_chargeReady == 0)
	{
		Throw();
	}
	else if (m_chargeReady == 1)
	{
		SendWeaponAnim( SATCHEL_RADIO_FIRE );

#ifndef CLIENT_DLL
		CSatchel *pWeapon = static_cast<CSatchel*>(m_pLayer->GetWeaponEntity());
		edict_t *pPlayer = pWeapon->m_pPlayer->edict();
		CBaseEntity *pSatchel = NULL;
		while ((pSatchel = UTIL_FindEntityInSphere(pSatchel, pWeapon->m_pPlayer->GetAbsOrigin(), 4096)) != NULL)
		{
			if (FClassnameIs(pSatchel->pev, "monster_satchel"))
			{
				if (pSatchel->pev->owner == pPlayer)
				{
					pSatchel->Use(pWeapon->m_pPlayer, pWeapon->m_pPlayer, USE_ON, 0);
					m_chargeReady = 2;
				}
			}
		}
#endif
		m_chargeReady = 2;
		m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.5f);
		m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
	}
}

void CSatchelWeaponContext::SecondaryAttack( void )
{
	if (m_chargeReady != 2)
	{
		Throw();
	}
}

void CSatchelWeaponContext::Throw( void )
{
	if (m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) > 0)
	{
#ifndef CLIENT_DLL
		CSatchel *pWeapon = static_cast<CSatchel*>(m_pLayer->GetWeaponEntity());
		Vector vecSrc = pWeapon->m_pPlayer->GetAbsOrigin();
		Vector vecThrow = gpGlobals->v_forward * 274 + pWeapon->m_pPlayer->GetAbsVelocity();

		if (pWeapon->m_pPlayer->GetGroundEntity())
			vecThrow += pWeapon->m_pPlayer->GetGroundEntity()->GetAbsVelocity();

		CBaseEntity *pSatchel = CBaseEntity::Create("monster_satchel", vecSrc, g_vecZero, pWeapon->m_pPlayer->edict());
		pSatchel->SetAbsVelocity( vecThrow );
		Vector vecAvelocity = g_vecZero;
		vecAvelocity.y = 400;
		pSatchel->SetLocalAvelocity( vecAvelocity );

		pWeapon->m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_satchel_radio.mdl");
		pWeapon->m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
#endif
		m_pLayer->SetPlayerViewmodel("models/v_satchel_radio.mdl");
		SendWeaponAnim( SATCHEL_RADIO_DRAW );

		m_chargeReady = 1;
		m_pLayer->SetPlayerAmmo(PrimaryAmmoIndex(), m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) - 1);
		m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(1.0f);
		m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5;
	}
}

void CSatchelWeaponContext::WeaponIdle( void )
{
	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

#ifndef CLIENT_DLL
	CSatchel *pWeapon = static_cast<CSatchel*>(m_pLayer->GetWeaponEntity());
#endif

	switch( m_chargeReady )
	{
	case 0:
		SendWeaponAnim( SATCHEL_FIDGET1 );
#ifndef CLIENT_DLL
		// use tripmine animations
		strcpy( pWeapon->m_pPlayer->m_szAnimExtention, "trip" );
#endif
		break;
	case 1:
		SendWeaponAnim( SATCHEL_RADIO_FIDGET1 );
#ifndef CLIENT_DLL
		// use hivehand animations
		strcpy( pWeapon->m_pPlayer->m_szAnimExtention, "hive" );
#endif
		break;
	case 2:
		if (m_pLayer->GetPlayerAmmo(PrimaryAmmoIndex()) < 1)
		{
			m_chargeReady = 0;
#ifndef CLIENT_DLL
			pWeapon->RetireWeapon();
#endif
			return;
		}

#ifndef CLIENT_DLL
		// use tripmine animations
		strcpy( pWeapon->m_pPlayer->m_szAnimExtention, "trip" );
		pWeapon->m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_satchel.mdl");
#endif
		m_pLayer->SetPlayerViewmodel("models/v_satchel.mdl");
		SendWeaponAnim( SATCHEL_DRAW );

		m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.5f);
		m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
		m_chargeReady = 0;
		break;
	}
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f);
}
