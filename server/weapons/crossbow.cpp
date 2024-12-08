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
#include "crossbow_bolt.h"
#include "weapon_logic_funcs_impl.h"

#define BOLT_AIR_VELOCITY	2000
#define BOLT_WATER_VELOCITY	1000

enum crossbow_e
{
	CROSSBOW_IDLE1 = 0,	// full
	CROSSBOW_IDLE2,	// empty
	CROSSBOW_FIDGET1,	// full
	CROSSBOW_FIDGET2,	// empty
	CROSSBOW_FIRE1,	// full
	CROSSBOW_FIRE2,	// reload
	CROSSBOW_FIRE3,	// empty
	CROSSBOW_RELOAD,	// from empty
	CROSSBOW_DRAW1,	// full
	CROSSBOW_DRAW2,	// empty
	CROSSBOW_HOLSTER1,	// full
	CROSSBOW_HOLSTER2,	// empty
};

LINK_ENTITY_TO_CLASS( weapon_crossbow, CCrossbow );

BEGIN_DATADESC( CCrossbow )
	//DEFINE_FIELD( m_fInZoom, FIELD_BOOLEAN ),
	//DEFINE_FIELD( m_fZoomInUse, FIELD_BOOLEAN ),
END_DATADESC()

void CCrossbow::Spawn( void )
{
	pev->classname = MAKE_STRING( "weapon_crossbow" );
	Precache( );
	SET_MODEL(ENT(pev), "models/w_crossbow.mdl");
	FallInit();// get ready to fall down.
	m_pWeaponLogic = new CCrossbowWeaponLogic(new CWeaponLogicFuncsImpl(this));
}

int CCrossbow::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_pWeaponLogic->m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

void CCrossbow::Precache( void )
{
	PRECACHE_MODEL("models/w_crossbow.mdl");
	PRECACHE_MODEL("models/v_crossbow.mdl");
	PRECACHE_MODEL("models/p_crossbow.mdl");

	PRECACHE_SOUND("weapons/xbow_fire1.wav");
	PRECACHE_SOUND("weapons/xbow_reload1.wav");

	UTIL_PrecacheOther( "crossbow_bolt" );
}

CCrossbowWeaponLogic::CCrossbowWeaponLogic(IWeaponLogicFuncs *funcs) :
	CBaseWeaponLogic(funcs)
{
	m_iId = WEAPON_CROSSBOW;
	m_iDefaultAmmo = CROSSBOW_DEFAULT_GIVE;
}

int CCrossbowWeaponLogic::GetItemInfo(ItemInfo *p)
{
	p->pszName = m_pFuncs->GetWeaponClassname(); //STRING(pev->classname);
	p->pszAmmo1 = "bolts";
	p->iMaxAmmo1 = BOLT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = CROSSBOW_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 2;
	p->iId = WEAPON_CROSSBOW;
	p->iFlags = 0;
	p->iWeight = CROSSBOW_WEIGHT;
	return 1;
}

BOOL CCrossbowWeaponLogic::Deploy( )
{
	if (m_iClip)
		return DefaultDeploy( "models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW1, "bow" );
	return DefaultDeploy( "models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW2, "bow" );
}

void CCrossbowWeaponLogic::Holster( void )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	if ( m_fInZoom )
	{
		m_fZoomInUse = 0;
		SecondaryAttack( );
	}

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	if (m_iClip)
		SendWeaponAnim( CROSSBOW_HOLSTER1 );
	else
		SendWeaponAnim( CROSSBOW_HOLSTER2 );
}

void CCrossbowWeaponLogic::PrimaryAttack( void )
{
	if ( m_fInZoom && g_pGameRules->IsMultiplayer() )
	{
		FireSniperBolt();
		return;
	}

	FireBolt();
}

// this function only gets called in multiplayer
void CCrossbowWeaponLogic::FireSniperBolt()
{
	m_flNextPrimaryAttack = gpGlobals->time + 0.75;

	if (m_iClip == 0)
	{
		PlayEmptySound( );
		return;
	}

	TraceResult tr;

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_iClip--;

	// make twang sound
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/xbow_fire1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));

	if (m_iClip)
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
		SendWeaponAnim( CROSSBOW_FIRE1 );
	}
	else if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0)
	{
		SendWeaponAnim( CROSSBOW_FIRE3 );
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	
	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );
	Vector vecSrc = m_pPlayer->GetGunPosition( ) - gpGlobals->v_up * 2;
	Vector vecDir = gpGlobals->v_forward;

	UTIL_TraceLine(vecSrc, vecSrc + vecDir * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

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
		CBaseEntity::Instance(tr.pHit)->TraceAttack(m_pPlayer->pev, 120, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB ); 
		ApplyMultiDamage( pev, m_pPlayer->pev );
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
}

void CCrossbowWeaponLogic::FireBolt( void )
{
	TraceResult tr;

	if (m_iClip == 0)
	{
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

	m_iClip--;

	// make twang sound
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/xbow_fire1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));

	if (m_iClip)
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
		SendWeaponAnim( CROSSBOW_FIRE1 );
	}
	else if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0)
	{
		SendWeaponAnim( CROSSBOW_FIRE3 );
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
	UTIL_MakeVectors( anglesAim );

	// Vector vecSrc = GetAbsOrigin() + gpGlobals->v_up * 16 + gpGlobals->v_forward * 20 + gpGlobals->v_right * 4;
	Vector vecSrc	= m_pPlayer->GetGunPosition( ) - gpGlobals->v_up * 2;
	Vector vecDir	= gpGlobals->v_forward;

	//CBaseEntity *pBolt = CBaseEntity::Create( "crossbow_bolt", vecSrc, anglesAim, m_pPlayer->edict() );
	CCrossbowBolt *pBolt = CCrossbowBolt::BoltCreate();
	pBolt->SetAbsOrigin( vecSrc );
	pBolt->SetAbsAngles( anglesAim );
	pBolt->pev->owner = m_pPlayer->edict();

	Vector vecGround;
	float flGroundSpeed;

	if( m_pPlayer->GetGroundEntity( ))
		vecGround = m_pPlayer->GetGroundEntity( )->GetAbsVelocity();
	else vecGround = g_vecZero;

	flGroundSpeed = vecGround.Length();

	if (m_pPlayer->pev->waterlevel == 3)
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

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = gpGlobals->time + 0.75;

	m_flNextSecondaryAttack = gpGlobals->time + 0.75;
	if (m_iClip != 0)
		m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	else
		m_flTimeWeaponIdle = 0.75;

	m_pPlayer->pev->punchangle.x -= 2;
}

void CCrossbowWeaponLogic::SecondaryAttack( void )
{
	// do not switch zoom when player stay button is pressed
	if (m_fZoomInUse)
		return;

	m_fZoomInUse = 1;

	if (m_fInZoom)
	{
		m_pPlayer->m_iFOV = 0; // 0 means reset to default fov
		m_fInZoom = 0;
	}
	else
	{
		m_pPlayer->m_iFOV = 20;
		m_fInZoom = 1;
	}
	
	m_flNextSecondaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = gpGlobals->time + 5.0;
}

void CCrossbowWeaponLogic::Reload( void )
{
	if ( m_fInZoom )
	{
		m_fZoomInUse = 0;
		SecondaryAttack();
	}

	if (DefaultReload( 5, CROSSBOW_RELOAD, 4.5 ))
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/xbow_reload1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));
	}
}

void CCrossbowWeaponLogic::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );  // get the autoaim vector but ignore it;  used for autoaim crosshair in DM

	ResetEmptySound( );
	m_fZoomInUse = 0;
	
	if (m_flTimeWeaponIdle < gpGlobals->time)
	{
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand <= 0.75)
		{
			if (m_iClip)
			{
				SendWeaponAnim( CROSSBOW_IDLE1 );
			}
			else
			{
				SendWeaponAnim( CROSSBOW_IDLE2 );
			}
			m_flTimeWeaponIdle = gpGlobals->time + RANDOM_FLOAT ( 10, 15 );
		}
		else
		{
			if (m_iClip)
			{
				SendWeaponAnim( CROSSBOW_FIDGET1 );
				m_flTimeWeaponIdle = gpGlobals->time + 90.0 / 30.0;
			}
			else
			{
				SendWeaponAnim( CROSSBOW_FIDGET2 );
				m_flTimeWeaponIdle = gpGlobals->time + 80.0 / 30.0;
			}
		}
	}
}
