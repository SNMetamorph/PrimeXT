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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"

#define BOLT_AIR_VELOCITY	2000
#define BOLT_WATER_VELOCITY	1000

class CCrossbowBolt : public CBaseEntity
{
	DECLARE_CLASS( CCrossbowBolt, CBaseEntity );

	void Spawn( void );
	void Precache( void );
	int Classify ( void );
	void BubbleThink( void );
	void BoltTouch( CBaseEntity *pOther );
	void ExplodeThink( void );
	virtual BOOL IsProjectile( void ) { return TRUE; }
	void TransferReset( void );
	void OnChangeLevel( void );
	void OnTeleport( void );
	int m_iTrail;

	DECLARE_DATADESC();
public:
	static CCrossbowBolt *BoltCreate( void );
};

LINK_ENTITY_TO_CLASS( crossbow_bolt, CCrossbowBolt );

BEGIN_DATADESC( CCrossbowBolt )
	DEFINE_FUNCTION( BubbleThink ),
	DEFINE_FUNCTION( ExplodeThink ),
	DEFINE_FUNCTION( BoltTouch ),
END_DATADESC()

CCrossbowBolt *CCrossbowBolt::BoltCreate( void )
{
	// Create a new entity with CCrossbowBolt private data
	CCrossbowBolt *pBolt = GetClassPtr( (CCrossbowBolt *)NULL );
	pBolt->pev->classname = MAKE_STRING( "crossbow_bolt" );
	pBolt->Spawn();

	return pBolt;
}

void CCrossbowBolt::Spawn( void )
{
	Precache( );
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	pev->gravity = 0.5;

	SET_MODEL( edict(), "models/crossbow_bolt.mdl");

	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	RelinkEntity( TRUE );

	SetTouch( &CCrossbowBolt::BoltTouch );
	SetThink( &CCrossbowBolt::BubbleThink );
	pev->nextthink = gpGlobals->time + 0.2;
}

void CCrossbowBolt::Precache( )
{
	PRECACHE_MODEL ("models/crossbow_bolt.mdl");
	PRECACHE_SOUND("weapons/xbow_hitbod1.wav");
	PRECACHE_SOUND("weapons/xbow_hitbod2.wav");
	PRECACHE_SOUND("weapons/xbow_fly1.wav");
	PRECACHE_SOUND("weapons/xbow_hit1.wav");
	PRECACHE_SOUND("fvox/beep.wav");
	m_iTrail = PRECACHE_MODEL("sprites/streak.spr");
}

int CCrossbowBolt :: Classify ( void )
{
	return CLASS_NONE;
}

void CCrossbowBolt :: OnChangeLevel( void )
{
	// NOTE: clear parent. We can't moving it properly for non-global entities
	SetParent( NULL );
}

void CCrossbowBolt :: TransferReset( void )
{
	// NOTE: i'm need to do it here because my parent already has
	// landmark offset but i'm is not
	Vector origin = GetAbsOrigin();
	origin += gpGlobals->vecLandmarkOffset;
	SetAbsOrigin( origin );

	// changelevel issues
	if ( m_hParent == NULL )
	{
		TraceResult tr;

		UTIL_MakeVectors( GetAbsAngles() );
		Vector vecSrc = GetAbsOrigin() + gpGlobals->v_forward * -32;
		Vector vecDst = GetAbsOrigin() + gpGlobals->v_forward * 32;

		UTIL_TraceHull( vecSrc, vecDst, ignore_monsters, head_hull, edict(), &tr );

		if( tr.pHit && ENTINDEX( tr.pHit ) != 0 )
		{
			CBaseEntity *pNewParent = CBaseEntity::Instance( tr.pHit );

			if( pNewParent && pNewParent->IsBSPModel( ))
			{
				ALERT( at_aiconsole, "SetNewParent: %s\n", pNewParent->GetClassname());
				SetParent( pNewParent );
			}
		}
	}
}

void CCrossbowBolt::BoltTouch( CBaseEntity *pOther )
{
	SetTouch( NULL );
	SetThink( NULL );

	if (pOther->pev->takedamage)
	{
		TraceResult tr = UTIL_GetGlobalTrace( );
		entvars_t	*pevOwner;

		pevOwner = VARS( pev->owner );

		// UNDONE: this needs to call TraceAttack instead
		ClearMultiDamage( );

		if ( pOther->IsPlayer() )
		{
			pOther->TraceAttack(pevOwner, gSkillData.plrDmgCrossbowClient, GetAbsVelocity().Normalize(), &tr, DMG_NEVERGIB ); 
		}
		else
		{
			pOther->TraceAttack(pevOwner, gSkillData.plrDmgCrossbowMonster, GetAbsVelocity().Normalize(), &tr, DMG_BULLET | DMG_NEVERGIB ); 
		}

		ApplyMultiDamage( pev, pevOwner );

		// play body "thwack" sound
		switch( RANDOM_LONG(0,1) )
		{
		case 0:
			EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM); break;
		case 1:
			EMIT_SOUND(ENT(pev), CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM); break;
		}

		if ( !g_pGameRules->IsMultiplayer() )
		{
			if( pOther->IsRigidBody( ))
			{
				Vector vecDir = GetAbsVelocity().Normalize( );
				UTIL_SetOrigin( this, GetAbsOrigin() - vecDir * 12 );
				SetLocalAngles( UTIL_VecToAngles( vecDir ));
				pev->solid = SOLID_NOT;
				pev->movetype = MOVETYPE_NONE;
				SetLocalVelocity( g_vecZero );
				SetLocalAvelocity( g_vecZero );
				Vector angles = GetLocalAngles();
				angles.z = RANDOM_LONG( 0, 360 );
				SetLocalAngles( angles );
				SetThink( &CBaseEntity::SUB_Remove );
				SetNextThink( 240.0 );

				// g-cont. Setup movewith feature
				SetParent( pOther );
			}
			else
			{
				SetLocalVelocity( g_vecZero );
				Killed( pev, GIB_NEVER );
			}
		}
	}
	else
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_BODY, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,7));
		pev->nextthink = gpGlobals->time;// this will get changed below if the bolt is allowed to stick in what it hit.
		SetThink( &CBaseEntity::SUB_Remove );

		if( UTIL_GetModelType( pOther->pev->modelindex ) == mod_brush || pOther->pev->solid == SOLID_CUSTOM )
		{
			Vector vecDir = GetAbsVelocity().Normalize( );
			UTIL_SetOrigin( this, GetAbsOrigin() - vecDir * 12 );
			SetLocalAngles( UTIL_VecToAngles( vecDir ));
			pev->solid = SOLID_NOT;
			pev->movetype = MOVETYPE_NONE;
			SetLocalVelocity( g_vecZero );
			SetLocalAvelocity( g_vecZero );
			Vector angles = GetLocalAngles();
			angles.z = RANDOM_LONG( 0, 360 );
			SetLocalAngles( angles );
			SetNextThink( 240.0 );

			// g-cont. Setup movewith feature
			SetParent( pOther );
		}

		if( UTIL_PointContents( GetAbsOrigin() ) != CONTENTS_WATER )
		{
			UTIL_Sparks( GetAbsOrigin() );
		}
	}

	if ( g_pGameRules->IsMultiplayer() )
	{
		SetThink( &CCrossbowBolt::ExplodeThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CCrossbowBolt :: OnTeleport( void )
{
	// re-aiming arrow
	SetLocalAngles( UTIL_VecToAngles( GetLocalVelocity( )));
}

void CCrossbowBolt::BubbleThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if( pev->waterlevel == 0 )
		return;

	UTIL_BubbleTrail( GetAbsOrigin() - GetAbsVelocity() * 0.1, GetAbsOrigin(), 1 );
}

void CCrossbowBolt::ExplodeThink( void )
{
	int iContents = UTIL_PointContents( GetAbsOrigin() );
	int iScale;
	
	pev->dmg = 40;
	iScale = 10;

	Vector vecOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_EXPLOSION);		
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		if (iContents != CONTENTS_WATER)
		{
			WRITE_SHORT( g_sModelIndexFireball );
		}
		else
		{
			WRITE_SHORT( g_sModelIndexWExplosion );
		}
		WRITE_BYTE( iScale  ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	entvars_t *pevOwner;

	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	::RadiusDamage( GetAbsOrigin(), pev, pevOwner, pev->dmg, 128, CLASS_NONE, DMG_BLAST|DMG_ALWAYSGIB );

	UTIL_Remove(this);
}

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

class CCrossbow : public CBasePlayerWeapon
{
	DECLARE_CLASS( CCrossbow, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 3; }
	int GetItemInfo(ItemInfo *p);

	void FireBolt( void );
	void FireSniperBolt( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int AddToPlayer( CBasePlayer *pPlayer );

	DECLARE_DATADESC();

	BOOL Deploy( void );
	void Holster( void );
	void Reload( void );
	void WeaponIdle( void );

	int m_fInZoom; // don't save this
	int m_fZoomInUse;
};

LINK_ENTITY_TO_CLASS( weapon_crossbow, CCrossbow );

BEGIN_DATADESC( CCrossbow )
	DEFINE_FIELD( m_fInZoom, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fZoomInUse, FIELD_BOOLEAN ),
END_DATADESC()

void CCrossbow::Spawn( void )
{
	Precache( );
	m_iId = WEAPON_CROSSBOW;
	SET_MODEL(ENT(pev), "models/w_crossbow.mdl");

	m_iDefaultAmmo = CROSSBOW_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

int CCrossbow::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
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

int CCrossbow::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
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

BOOL CCrossbow::Deploy( )
{
	if (m_iClip)
		return DefaultDeploy( "models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW1, "bow" );
	return DefaultDeploy( "models/v_crossbow.mdl", "models/p_crossbow.mdl", CROSSBOW_DRAW2, "bow" );
}

void CCrossbow::Holster( void )
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

void CCrossbow::PrimaryAttack( void )
{
	if ( m_fInZoom && g_pGameRules->IsMultiplayer() )
	{
		FireSniperBolt();
		return;
	}

	FireBolt();
}

// this function only gets called in multiplayer
void CCrossbow::FireSniperBolt()
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

void CCrossbow::FireBolt( void )
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

void CCrossbow::SecondaryAttack( void )
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

void CCrossbow::Reload( void )
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

void CCrossbow::WeaponIdle( void )
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

class CCrossbowAmmo : public CBasePlayerAmmo
{
	DECLARE_CLASS( CCrossbowAmmo, CBasePlayerAmmo );

	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_crossbow_clip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_crossbow_clip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_CROSSBOWCLIP_GIVE, "bolts", BOLT_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_crossbow, CCrossbowAmmo );
