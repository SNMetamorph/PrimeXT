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

#include "crossbow.h"

#ifdef CLIENT_DLL
#else
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "crossbow_bolt.h"
#endif

#define BOLT_AIR_VELOCITY	2000
#define BOLT_WATER_VELOCITY	1000

CCrossbowWeaponContext::CCrossbowWeaponContext(std::unique_ptr<IWeaponLayer> &&layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_CROSSBOW;
	m_iDefaultAmmo = CROSSBOW_DEFAULT_GIVE;
	m_fInZoom = false;
	m_usCrossbow = m_pLayer->PrecacheEvent("events/crossbow1.sc");
	m_usCrossbow2 = m_pLayer->PrecacheEvent("events/crossbow2.sc");
}

int CCrossbowWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(CROSSBOW_CLASSNAME);
	p->pszAmmo1 = "bolts";
	p->iMaxAmmo1 = BOLT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = CROSSBOW_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 2;
	p->iId = m_iId;
	p->iFlags = 0;
	p->iWeight = CROSSBOW_WEIGHT;
	return 1;
}

bool CCrossbowWeaponContext::Deploy( )
{
	if (m_iClip)
		return DefaultDeploy( "models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW1, "bow" );
	return DefaultDeploy( "models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW2, "bow" );
}

void CCrossbowWeaponContext::Holster( void )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	if ( m_fInZoom )
	{
		SecondaryAttack( );
	}

	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);

	if (m_iClip)
		SendWeaponAnim( CROSSBOW_HOLSTER1 );
	else
		SendWeaponAnim( CROSSBOW_HOLSTER2 );
}

void CCrossbowWeaponContext::PrimaryAttack( void )
{
	if (m_iClip < 1)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.15f;
		return;
	}

	m_iClip--;

	if (m_fInZoom && m_pLayer->IsMultiplayer()) {
		FireSniperBolt();
	}
	else {
		FireBolt();
	}
}

// this function only gets called in multiplayer
void CCrossbowWeaponContext::FireSniperBolt()
{
	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usCrossbow2;
	params.delay = 0.0f;
	params.origin = m_pLayer->GetGunPosition();
	params.angles = m_pLayer->GetCameraOrientation().GetAngles();
	params.fparam1 = 0;
	params.fparam2 = 0;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = m_iClip == 0;
	params.bparam2 = m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) == 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

	m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.75f;
#ifndef CLIENT_DLL
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	player->SetAnimation( PLAYER_ATTACK1 );
	player->m_iWeaponVolume = QUIET_GUN_VOLUME;

	Vector anglesAim = player->pev->v_angle + player->pev->punchangle;
	UTIL_MakeVectors( anglesAim );
	Vector vecSrc = player->GetGunPosition( ) - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, player->edict(), &tr);

	if ( tr.pHit->v.takedamage )
	{
		switch( RANDOM_LONG(0,1) )
		{
		case 0:
			EMIT_SOUND( tr.pHit, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM); break;
		case 1:
			EMIT_SOUND( tr.pHit, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM); break;
		}

		ClearMultiDamage( );
		CBaseEntity::Instance(tr.pHit)->TraceAttack(player->pev, 120, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB ); 
		ApplyMultiDamage( m_pLayer->GetWeaponEntity()->pev, player->pev );
	}
	else
	{
		// create a bolt
		CCrossbowBolt *pBolt = CCrossbowBolt::BoltCreate();
		pBolt->SetAbsOrigin( tr.vecEndPos - vecDir * 10 );
		pBolt->SetAbsAngles( UTIL_VecToAngles( vecDir ));
		pBolt->pev->solid = SOLID_NOT;
		pBolt->SetTouch( NULL );
		pBolt->SetThink( &CBaseEntity::SUB_Remove );

		EMIT_SOUND( pBolt->edict(), CHAN_WEAPON, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM );

		if( UTIL_PointContents( tr.vecEndPos ) != CONTENTS_WATER )
		{
			UTIL_Sparks( tr.vecEndPos );
		}

		if( FClassnameIs( tr.pHit, "worldspawn" ))
		{
			// let the bolt sit around for a while if it hit static architecture
			pBolt->pev->nextthink = gpGlobals->time + 5.0;
		}
		else
		{
			pBolt->pev->nextthink = gpGlobals->time;
		}
	}
#endif
}

void CCrossbowWeaponContext::FireBolt( void )
{
	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usCrossbow;
	params.delay = 0.0f;
	params.origin = m_pLayer->GetGunPosition();
	params.angles = m_pLayer->GetCameraOrientation().GetAngles();
	params.fparam1 = 0;
	params.fparam2 = 0;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = m_iClip == 0;
	params.bparam2 = m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) == 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

	m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.75;
	m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.75;

	if (m_iClip != 0)
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 5.0;
	else
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.75;


#ifndef CLIENT_DLL
	// player "shoot" animation
	CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
	player->SetAnimation( PLAYER_ATTACK1 );
	player->m_iWeaponVolume = QUIET_GUN_VOLUME;

	Vector anglesAim = player->pev->v_angle + player->pev->punchangle;
	UTIL_MakeVectors( anglesAim );

	// Vector vecSrc = GetAbsOrigin() + gpGlobals->v_up * 16 + gpGlobals->v_forward * 20 + gpGlobals->v_right * 4;
	Vector vecSrc	= player->GetGunPosition( ) - gpGlobals->v_up * 2;
	Vector vecDir	= gpGlobals->v_forward;

	//CBaseEntity *pBolt = CBaseEntity::Create( "crossbow_bolt", vecSrc, anglesAim, m_pPlayer->edict() );
	CCrossbowBolt *pBolt = CCrossbowBolt::BoltCreate();
	pBolt->SetAbsOrigin( vecSrc );
	pBolt->SetAbsAngles( anglesAim );
	pBolt->pev->owner = player->edict();

	Vector vecGround;
	float flGroundSpeed;

	if( player->GetGroundEntity( ))
		vecGround = player->GetGroundEntity( )->GetAbsVelocity();
	else vecGround = g_vecZero;

	flGroundSpeed = vecGround.Length();

	if (player->pev->waterlevel == 3)
	{
		pBolt->SetLocalVelocity( vecDir * BOLT_WATER_VELOCITY + vecGround );
		pBolt->pev->speed = BOLT_WATER_VELOCITY + flGroundSpeed;
	}
	else
	{
		pBolt->SetLocalVelocity( vecDir * BOLT_AIR_VELOCITY + vecGround );
		pBolt->pev->speed = BOLT_AIR_VELOCITY + flGroundSpeed;
	}

	Vector avelocity( 0, 0, 10 );

	pBolt->SetLocalAvelocity( avelocity );

	if (!m_iClip && player->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		player->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	// m_pPlayer->pev->punchangle.x -= 2;
#endif
}

void CCrossbowWeaponContext::SecondaryAttack( void )
{
	if (m_pLayer->GetPlayerFOV() != 0.0f)
	{
		m_pLayer->SetPlayerFOV(0.0f); // 0 means reset to default fov
		m_fInZoom = false;
	}
	else
	{
		m_pLayer->SetPlayerFOV(20.0f);
		m_fInZoom = true;
	}
	
	m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.3;
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 5.0;
}

void CCrossbowWeaponContext::Reload( void )
{
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) < 1)
		return;

	if ( m_fInZoom )
	{
		SecondaryAttack();
	}

	if (DefaultReload( 5, CROSSBOW_RELOAD, 4.5 ))
	{
#ifndef CLIENT_DLL
		CBasePlayer *player = m_pLayer->GetWeaponEntity()->m_pPlayer;
		EMIT_SOUND_DYN(ENT(player->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
#endif
	}
}

void CCrossbowWeaponContext::WeaponIdle( void )
{
	m_pLayer->GetAutoaimVector( AUTOAIM_2DEGREES );  // get the autoaim vector but ignore it;  used for autoaim crosshair in DM

	ResetEmptySound( );

	if (m_flTimeWeaponIdle < m_pLayer->GetWeaponTimeBase(UsePredicting()))
	{
		float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.f, 1.f);
		if (flRand <= 0.75f)
		{
			if (m_iClip)
			{
				SendWeaponAnim( CROSSBOW_IDLE1 );
			}
			else
			{
				SendWeaponAnim( CROSSBOW_IDLE2 );
			}
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
		}
		else
		{
			if (m_iClip)
			{
				SendWeaponAnim( CROSSBOW_FIDGET1 );
				m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 90.0 / 30.0;
			}
			else
			{
				SendWeaponAnim( CROSSBOW_FIDGET2 );
				m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 80.0 / 30.0;
			}
		}
	}
}
