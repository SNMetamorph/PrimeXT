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

#define TRIPMINE_PRIMARY_VOLUME	450

LINK_ENTITY_TO_CLASS( weapon_tripmine, CTripmine );

void CTripmine::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_TRIPMINE;
	SET_MODEL(ENT(pev), "models/v_tripmine.mdl");
	pev->frame = 0;
	pev->body = 3;
	pev->sequence = TRIPMINE_GROUND;
	// ResetSequenceInfo( );
	pev->framerate = 0;

	FallInit();// get ready to fall down

	m_iDefaultAmmo = TRIPMINE_DEFAULT_GIVE;

	if ( !g_pGameRules->IsDeathmatch() )
	{
		UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 28) ); 
	}
}

void CTripmine::Precache( void )
{
	PRECACHE_MODEL ("models/v_tripmine.mdl");
	PRECACHE_MODEL ("models/p_tripmine.mdl");
	UTIL_PrecacheOther( "monster_tripmine" );
}

int CTripmine::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Trip Mine";
	p->iMaxAmmo1 = TRIPMINE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_TRIPMINE;
	p->iWeight = TRIPMINE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CTripmine::Deploy( )
{
	pev->body = 0;
	return DefaultDeploy( "models/v_tripmine.mdl", "models/p_tripmine.mdl", TRIPMINE_DRAW, "trip" );
}

void CTripmine::Holster( void )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		// out of mines
		m_pPlayer->RemoveWeapon( WEAPON_TRIPMINE );
		SetThink( &CBasePlayerItem::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	SendWeaponAnim( TRIPMINE_HOLSTER );
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

void CTripmine::PrimaryAttack( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = gpGlobals->v_forward;

	TraceResult tr;

	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 128, ignore_monsters, m_pPlayer->edict(), &tr );

	if (tr.flFraction < 1.0)
	{
		// ALERT( at_console, "hit %f\n", tr.flFraction );

		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
		if (pEntity && (pEntity->IsBSPModel() || pEntity->IsCustomModel()) && !(pEntity->pev->flags & FL_CONVEYOR))
		{
			Vector angles = UTIL_VecToAngles( tr.vecPlaneNormal );

			CBaseEntity *pEnt = CBaseEntity::Create( "monster_tripmine", tr.vecEndPos + tr.vecPlaneNormal * 8, angles, m_pPlayer->edict() );

			// g-cont. attach tripmine to the wall
			// NOTE: we should always attach the tripmine because our parent it's our owner too
			if( pEntity != g_pWorld )
				pEnt->SetParent( pEntity );

			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

			if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
			{
				SendWeaponAnim( TRIPMINE_DRAW );
			}
			else
			{
				// no more mines! 
				RetireWeapon();
				return;
			}
		}
		else
		{
			// ALERT( at_console, "no deploy\n" );
		}
	}
	else
	{

	}

	m_flNextPrimaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
}

void CTripmine::WeaponIdle( void )
{
	if (m_flTimeWeaponIdle > gpGlobals->time)
		return;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		SendWeaponAnim( TRIPMINE_DRAW );
	}
	else
	{
		RetireWeapon(); 
		return;
	}

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.25)
	{
		iAnim = TRIPMINE_IDLE1;
		m_flTimeWeaponIdle = gpGlobals->time + 90.0 / 30.0;
	}
	else if (flRand <= 0.75)
	{
		iAnim = TRIPMINE_IDLE2;
		m_flTimeWeaponIdle = gpGlobals->time + 60.0 / 30.0;
	}
	else
	{
		iAnim = TRIPMINE_FIDGET;
		m_flTimeWeaponIdle = gpGlobals->time + 100.0 / 30.0;
	}

	SendWeaponAnim( iAnim );
}
