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

enum hgun_e
{
	HGUN_IDLE1 = 0,
	HGUN_FIDGETSWAY,
	HGUN_FIDGETSHAKE,
	HGUN_DOWN,
	HGUN_UP,
	HGUN_SHOOT
};

enum firemode_e
{
	FIREMODE_TRACK = 0,
	FIREMODE_FAST
};

LINK_ENTITY_TO_CLASS( weapon_hornetgun, CHgun );

void CHgun::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_HORNETGUN;
	SET_MODEL(ENT(pev), "models/w_hgun.mdl");

	m_iDefaultAmmo = HIVEHAND_DEFAULT_GIVE;
	m_iFirePhase = 0;

	FallInit();// get ready to fall down.
}

void CHgun::Precache( void )
{
	PRECACHE_MODEL("models/v_hgun.mdl");
	PRECACHE_MODEL("models/w_hgun.mdl");
	PRECACHE_MODEL("models/p_hgun.mdl");

	UTIL_PrecacheOther("hornet");
}

int CHgun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		if ( g_pGameRules->IsMultiplayer() )
		{
			// in multiplayer, all hivehands come full. 
			pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] = HORNET_MAX_CARRY;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

int CHgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Hornets";
	p->iMaxAmmo1 = HORNET_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_HORNETGUN;
	p->iFlags = ITEM_FLAG_NOAUTOSWITCHEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iWeight = HORNETGUN_WEIGHT;

	return 1;
}

BOOL CHgun::Deploy( void )
{
	return DefaultDeploy( "models/v_hgun.mdl", "models/p_hgun.mdl", HGUN_UP, "hive" );
}

void CHgun::Holster( void )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	// m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
	SendWeaponAnim( HGUN_DOWN );

	//!!!HACKHACK - can't select hornetgun if it's empty! no way to get ammo for it, either.
	if ( !m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] )
	{
		m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] = 1;
	}
}

void CHgun::PrimaryAttack( void )
{
	Reload( );

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		return;
	}

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );

	CBaseEntity *pHornet = CBaseEntity::Create( "hornet", m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12, m_pPlayer->pev->v_angle, m_pPlayer->edict() );
	pHornet->SetAbsVelocity( gpGlobals->v_forward * 300 );
	
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_flRechargeTime = gpGlobals->time + 0.5;

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	SendWeaponAnim( HGUN_SHOOT );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = m_flNextPrimaryAttack + 0.25;

	if (m_flNextPrimaryAttack < gpGlobals->time)
	{
		m_flNextPrimaryAttack = gpGlobals->time + 0.25;
	}

	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
}

void CHgun::SecondaryAttack( void )
{
	Reload();

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		return;
	}

	CBaseEntity *pHornet;
	Vector vecSrc;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );

	vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12;

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

	pHornet = CBaseEntity::Create( "hornet", vecSrc, m_pPlayer->pev->v_angle, m_pPlayer->edict() );
	pHornet->SetAbsVelocity( gpGlobals->v_forward * 1200 );
	pHornet->SetAbsAngles( UTIL_VecToAngles( pHornet->GetAbsVelocity() ));

	pHornet->SetThink( &CHornet::StartDart );

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	m_flRechargeTime = gpGlobals->time + 0.5;

	SendWeaponAnim( HGUN_SHOOT );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.1;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
	m_pPlayer->pev->punchangle.x = RANDOM_FLOAT( 0, 2 );
}

void CHgun::Reload( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= HORNET_MAX_CARRY)
		return;

	while (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < HORNET_MAX_CARRY && m_flRechargeTime < gpGlobals->time)
	{
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]++;
		m_flRechargeTime += 0.5;
	}
}

void CHgun::WeaponIdle( void )
{
	Reload( );

	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.75)
	{
		iAnim = HGUN_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = HGUN_FIDGETSWAY;
		m_flTimeWeaponIdle = gpGlobals->time + 40.0 / 16.0;
	}
	else
	{
		iAnim = HGUN_FIDGETSHAKE;
		m_flTimeWeaponIdle = gpGlobals->time + 35.0 / 16.0;
	}
	SendWeaponAnim( iAnim );
}
