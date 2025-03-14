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
/*

===== weapons.cpp ========================================================

  functions governing the selection/use of weapons for players

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"
#include "sv_materials.h"
#include "user_messages.h"

extern int gEvilImpulse101;

#define NOT_USED 255

DLL_GLOBAL short	g_sModelIndexLaser;// holds the index for the laser beam
DLL_GLOBAL const char *g_pModelNameLaser = "sprites/laserbeam.spr";
DLL_GLOBAL short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
DLL_GLOBAL short	g_sModelIndexFireball;// holds the index for the fireball
DLL_GLOBAL short	g_sModelIndexSmoke;// holds the index for the smoke cloud
DLL_GLOBAL short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
DLL_GLOBAL short	g_sModelIndexBubbles;// holds the index for the bubbles model
DLL_GLOBAL short	g_sModelIndexBloodDrop;// holds the sprite index for the initial blood
DLL_GLOBAL short	g_sModelIndexBloodSpray;// holds the sprite index for splattered blood

MULTIDAMAGE gMultiDamage;

#define TRACER_FREQ		4			// Tracers fire every fourth bullet


//=========================================================
// MaxAmmoCarry - pass in a name and this function will tell
// you the maximum amount of that type of ammunition that a 
// player can carry.
//=========================================================
int MaxAmmoCarry( int iszName )
{
	for ( int i = 0;  i < MAX_WEAPONS; i++ )
	{
		if ( CBaseWeaponContext::ItemInfoArray[i].pszAmmo1 && !strcmp( STRING(iszName), CBaseWeaponContext::ItemInfoArray[i].pszAmmo1 ) )
			return CBaseWeaponContext::ItemInfoArray[i].iMaxAmmo1;
		if ( CBaseWeaponContext::ItemInfoArray[i].pszAmmo2 && !strcmp( STRING(iszName), CBaseWeaponContext::ItemInfoArray[i].pszAmmo2 ) )
			return CBaseWeaponContext::ItemInfoArray[i].iMaxAmmo2;
	}

	ALERT( at_console, "MaxAmmoCarry() doesn't recognize '%s'!\n", STRING( iszName ) );
	return -1;
}

	
/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage

==============================================================================
*/

//
// ClearMultiDamage - resets the global multi damage accumulator
//
void ClearMultiDamage(void)
{
	gMultiDamage.pEntity = NULL;
	gMultiDamage.amount	= 0;
	gMultiDamage.type = 0;
}


//
// ApplyMultiDamage - inflicts contents of global multi damage register on gMultiDamage.pEntity
//
// GLOBALS USED:
//		gMultiDamage

void ApplyMultiDamage(entvars_t *pevInflictor, entvars_t *pevAttacker )
{
	Vector		vecSpot1;//where blood comes from
	Vector		vecDir;//direction blood should go
	TraceResult	tr;
	
	if ( !gMultiDamage.pEntity )
		return;

	gMultiDamage.pEntity->TakeDamage(pevInflictor, pevAttacker, gMultiDamage.amount, gMultiDamage.type );
}


// GLOBALS USED:
//		gMultiDamage

void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType)
{
	if ( !pEntity )
		return;
	
	gMultiDamage.type |= bitsDamageType;

	if ( pEntity != gMultiDamage.pEntity )
	{
		ApplyMultiDamage(pevInflictor,pevInflictor); // UNDONE: wrong attacker!
		gMultiDamage.pEntity	= pEntity;
		gMultiDamage.amount		= 0;
	}

	gMultiDamage.amount += flDamage;
}

/*
================
SpawnBlood
================
*/
void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage)
{
	UTIL_BloodDrips( vecSpot, g_vecAttackDir, bloodColor, (int)flDamage );
}


const char *DamageDecal( CBaseEntity *pEntity, int bitsDamageType )
{
	if (!pEntity) 
	{
		return "shot";
	}
	return pEntity->DamageDecal( bitsDamageType );
}

void DecalGunshot( TraceResult *pTrace, int iBulletType, const vec3_t &origin, const vec3_t &vecEnd)
{
	if (!UTIL_IsValidEntity(pTrace->pHit))
		return;

	CBaseEntity *pTarget = CBaseEntity::Instance(pTrace->pHit);
	matdef_t *pMaterial = nullptr;
	bool characterEntity = pTarget->pev->flags & (FL_MONSTER | FL_CLIENT); // is player/NPC
	bool studiomodelEntity = pTarget->pev->solid == SOLID_CUSTOM;

	if (UTIL_GetModelType(pTarget->pev->modelindex) == mod_brush)
	{
		msurface_t *surf = TRACE_SURFACE(pTrace->pHit, origin, vecEnd);
		pMaterial = COM_MatDefFromSurface(surf, pTrace->vecEndPos);
		if (pMaterial) {
			UTIL_TraceCustomDecal(pTrace, pMaterial->impact_decal, RANDOM_FLOAT(0.0f, 360.0f));
		}
	}
	else if (characterEntity)
	{
		// Decal the model with a blood
		switch (iBulletType)
		{
			case BULLET_PLAYER_9MM:
			case BULLET_MONSTER_9MM:
			case BULLET_PLAYER_MP5:
			case BULLET_MONSTER_MP5:
			case BULLET_PLAYER_BUCKSHOT:
			case BULLET_PLAYER_357:
			default:
				// smoke and decal
				UTIL_StudioDecalTrace(pTrace, DamageDecal(pTarget, DMG_BULLET));
				break;
			case BULLET_MONSTER_12MM:
				// smoke and decal
				UTIL_StudioDecalTrace(pTrace, DamageDecal(pTarget, DMG_BULLET));
				break;
			case BULLET_PLAYER_CROWBAR:
				// wall decal
				UTIL_StudioDecalTrace(pTrace, DamageDecal(pTarget, DMG_CLUB));
				break;
		}
	}
	else if (studiomodelEntity)
	{
		if (pTrace->materialHash)
		{
			matdesc_t *mat = COM_FindMaterial(pTrace->materialHash);
			pMaterial = mat ? mat->effects : nullptr;
			if (pMaterial) {
				UTIL_StudioDecalTrace(pTrace, pMaterial->impact_decal);
			}
		}
	}
}

//
// EjectBrass - tosses a brass shell from passed origin at passed velocity
//
void EjectBrass ( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype )
{
	// FIX: when the player shoots, their gun isn't in the same position as it is on the model other players see.

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_MODEL);
		WRITE_COORD( vecOrigin.x);
		WRITE_COORD( vecOrigin.y);
		WRITE_COORD( vecOrigin.z);
		WRITE_COORD( vecVelocity.x);
		WRITE_COORD( vecVelocity.y);
		WRITE_COORD( vecVelocity.z);
		WRITE_ANGLE( rotation );
		WRITE_SHORT( model );
		WRITE_BYTE ( soundtype);
		WRITE_BYTE ( 25 );// 2.5 seconds
	MESSAGE_END();
}

int giAmmoIndex = 0;

// Precaches the ammo and queues the ammo info for sending to clients
void AddAmmoNameToAmmoRegistry( const char *szAmmoname )
{
	// make sure it's not already in the registry
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( !CBaseWeaponContext::AmmoInfoArray[i].pszName)
			continue;

		if ( stricmp( CBaseWeaponContext::AmmoInfoArray[i].pszName, szAmmoname ) == 0 )
			return; // ammo already in registry, just quite
	}


	giAmmoIndex++;
	ASSERT( giAmmoIndex < MAX_AMMO_SLOTS );
	if ( giAmmoIndex >= MAX_AMMO_SLOTS )
		giAmmoIndex = 0;

	CBaseWeaponContext::AmmoInfoArray[giAmmoIndex].pszName = szAmmoname;
	CBaseWeaponContext::AmmoInfoArray[giAmmoIndex].iId = giAmmoIndex;   // yes, this info is redundant
}


// Precaches the weapon and queues the weapon info for sending to clients
void UTIL_PrecacheOtherWeapon( const char *szClassname )
{
	CBaseEntity *pEntity = CreateEntityByName( szClassname );

	if( !pEntity )
	{
		ALERT ( at_console, "NULL Ent in UTIL_PrecacheOtherWeapon\n" );
		return;
	}
	
	ItemInfo II;
	pEntity->Precache( );
	memset( &II, 0, sizeof II );

	if ( ((CBasePlayerItem*)pEntity)->GetItemInfo( &II ) )
	{
		CBaseWeaponContext::ItemInfoArray[II.iId] = II;

		if ( II.pszAmmo1 && *II.pszAmmo1 )
		{
			AddAmmoNameToAmmoRegistry( II.pszAmmo1 );
		}

		if ( II.pszAmmo2 && *II.pszAmmo2 )
		{
			AddAmmoNameToAmmoRegistry( II.pszAmmo2 );
		}

		memset( &II, 0, sizeof II );
	}

	REMOVE_ENTITY(pEntity->edict());
}

// called by worldspawn
void W_Precache(void)
{
	memset( CBaseWeaponContext::ItemInfoArray, 0, sizeof(CBaseWeaponContext::ItemInfoArray) );
	memset( CBaseWeaponContext::AmmoInfoArray, 0, sizeof(CBaseWeaponContext::AmmoInfoArray) );
	giAmmoIndex = 0;

	// custom items...

	// common world objects
	UTIL_PrecacheOther( "item_suit" );
	UTIL_PrecacheOther( "item_battery" );
	UTIL_PrecacheOther( "item_antidote" );
	UTIL_PrecacheOther( "item_security" );
	UTIL_PrecacheOther( "item_longjump" );

	// shotgun
	UTIL_PrecacheOtherWeapon( "weapon_shotgun" );
	UTIL_PrecacheOther( "ammo_buckshot" );

	// crowbar
	UTIL_PrecacheOtherWeapon( "weapon_crowbar" );

	// glock
	UTIL_PrecacheOtherWeapon( "weapon_9mmhandgun" );
	UTIL_PrecacheOther( "ammo_9mmclip" );

	// mp5
	UTIL_PrecacheOtherWeapon( "weapon_9mmAR" );
	UTIL_PrecacheOther( "ammo_9mmAR" );
	UTIL_PrecacheOther( "ammo_ARgrenades" );

	// python
	UTIL_PrecacheOtherWeapon( "weapon_357" );
	UTIL_PrecacheOther( "ammo_357" );

	// gauss
	UTIL_PrecacheOtherWeapon( "weapon_gauss" );
	UTIL_PrecacheOther( "ammo_gaussclip" );

	// rpg
	UTIL_PrecacheOtherWeapon( "weapon_rpg" );
	UTIL_PrecacheOther( "ammo_rpgclip" );

	// crossbow
	UTIL_PrecacheOtherWeapon( "weapon_crossbow" );
	UTIL_PrecacheOther( "ammo_crossbow" );

	// egon
	UTIL_PrecacheOtherWeapon( "weapon_egon" );

	// tripmine
	UTIL_PrecacheOtherWeapon( "weapon_tripmine" );

	// satchel charge
	UTIL_PrecacheOtherWeapon( "weapon_satchel" );

	// hand grenade
	UTIL_PrecacheOtherWeapon("weapon_handgrenade");

	// squeak grenade
	UTIL_PrecacheOtherWeapon( "weapon_snark" );

	// hornetgun
	UTIL_PrecacheOtherWeapon( "weapon_hornetgun" );

	if ( g_pGameRules->IsDeathmatch() )
	{
		UTIL_PrecacheOther( "weaponbox" );// container for dropped deathmatch weapons
	}

	g_sModelIndexFireball = PRECACHE_MODEL ("sprites/zerogxplode.spr");// fireball
	g_sModelIndexWExplosion = PRECACHE_MODEL ("sprites/WXplo1.spr");// underwater fireball
	g_sModelIndexSmoke = PRECACHE_MODEL ("sprites/steam1.spr");// smoke
	g_sModelIndexBubbles = PRECACHE_MODEL ("sprites/bubble.spr");//bubbles
	g_sModelIndexBloodSpray = PRECACHE_MODEL ("sprites/bloodspray.spr"); // initial blood
	g_sModelIndexBloodDrop = PRECACHE_MODEL ("sprites/blood.spr"); // splattered blood 

	g_sModelIndexLaser = PRECACHE_MODEL( (char *)g_pModelNameLaser );
	g_sModelIndexLaserDot = PRECACHE_MODEL("sprites/laserdot.spr");

	// used by explosions
	PRECACHE_MODEL("models/grenade.mdl");
	PRECACHE_MODEL("sprites/explode1.spr");
	PRECACHE_MODEL("models/m_flash1.mdl");

	PRECACHE_SOUND("weapons/debris1.wav");// explosion aftermaths
	PRECACHE_SOUND("weapons/debris2.wav");// explosion aftermaths
	PRECACHE_SOUND("weapons/debris3.wav");// explosion aftermaths

	PRECACHE_SOUND("weapons/grenade_hit1.wav");//grenade
	PRECACHE_SOUND("weapons/grenade_hit2.wav");//grenade
	PRECACHE_SOUND("weapons/grenade_hit3.wav");//grenade

	PRECACHE_SOUND("weapons/bullet_hit1.wav");	// hit by bullet
	PRECACHE_SOUND("weapons/bullet_hit2.wav");	// hit by bullet

	PRECACHE_SOUND("items/weapondrop1.wav");// weapon falls to the ground

}

BEGIN_DATADESC( CBasePlayerItem )
	DEFINE_FIELD( m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_pNext, FIELD_CLASSPTR ),
	DEFINE_FUNCTION( DestroyItem ),
	DEFINE_FUNCTION( DefaultTouch ),
	DEFINE_FUNCTION( FallThink ),
	DEFINE_FUNCTION( Materialize ),
	DEFINE_FUNCTION( AttemptToMaterialize ),
END_DATADESC()

#define DEFINE_BASEPLAYERWEAPON_FIELD( x, ft ) \
	DEFINE_CUSTOM_FIELD( x, ft, [](CBaseEntity *pEntity, void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = reinterpret_cast<CBasePlayerWeapon*>(pEntity); \
		std::memcpy(pData, &p->m_pWeaponContext->x, dataSize); \
	}, \
	[](CBaseEntity *pEntity, const void *pData, size_t dataSize) { \
		CBasePlayerWeapon *p = reinterpret_cast<CBasePlayerWeapon*>(pEntity); \
		std::memcpy(&p->m_pWeaponContext->x, pData, dataSize); \
	})
	
BEGIN_DATADESC( CBasePlayerWeapon )
	DEFINE_BASEPLAYERWEAPON_FIELD( m_flNextPrimaryAttack, FIELD_TIME ),
	DEFINE_BASEPLAYERWEAPON_FIELD( m_flNextSecondaryAttack, FIELD_TIME ),
	DEFINE_BASEPLAYERWEAPON_FIELD( m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_BASEPLAYERWEAPON_FIELD( m_iPrimaryAmmoType, FIELD_INTEGER ),
	DEFINE_BASEPLAYERWEAPON_FIELD( m_iSecondaryAmmoType, FIELD_INTEGER ),
	DEFINE_BASEPLAYERWEAPON_FIELD( m_iClip, FIELD_INTEGER ),
	DEFINE_BASEPLAYERWEAPON_FIELD( m_iDefaultAmmo, FIELD_INTEGER ),
	DEFINE_BASEPLAYERWEAPON_FIELD( m_iId, FIELD_INTEGER ),
	DEFINE_BASEPLAYERWEAPON_FIELD( m_fInSpecialReload, FIELD_INTEGER ),
END_DATADESC()

void CBasePlayerItem :: SetObjectCollisionBox( void )
{
	pev->absmin = GetAbsOrigin() + Vector(-24, -24, 0);
	pev->absmax = GetAbsOrigin() + Vector(24, 24, 16); 
}


//=========================================================
// Sets up movetype, size, solidtype for a new weapon. 
//=========================================================
void CBasePlayerItem :: FallInit( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;

	UTIL_SetSize(pev, g_vecZero, g_vecZero );//pointsize until it lands on the ground.
	
	SetTouch( &CBasePlayerItem::DefaultTouch );
	SetThink( &CBasePlayerItem::FallThink );

	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// FallThink - Items that have just spawned run this think
// to catch them when they hit the ground. Once we're sure
// that the object is grounded, we change its solid type
// to trigger and set it in a large box that helps the
// player get it.
//=========================================================
void CBasePlayerItem::FallThink ( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->flags & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( !FNullEnt( pev->owner ) )
		{
			int pitch = 95 + RANDOM_LONG(0,29);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "items/weapondrop1.wav", 1, ATTN_NORM, 0, pitch);	
		}

		Vector angles = GetLocalAngles();

		// lie flat
		angles.x = 0;
		angles.z = 0;

		SetLocalAngles( angles );

		Materialize(); 
	}
}

//=========================================================
// Materialize - make a CBasePlayerItem visible and tangible
//=========================================================
void CBasePlayerItem::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	pev->solid = SOLID_TRIGGER;

	RelinkEntity( TRUE ); // link into world.
	SetTouch( &CBasePlayerItem::DefaultTouch);
	SetThink( NULL);

}

//=========================================================
// AttemptToMaterialize - the item is trying to rematerialize,
// should it do so now or wait longer?
//=========================================================
void CBasePlayerItem::AttemptToMaterialize( void )
{
	float time = g_pGameRules->FlWeaponTryRespawn( this );

	if ( time == 0 )
	{
		Materialize();
		return;
	}

	pev->nextthink = gpGlobals->time + time;
}

//=========================================================
// CheckRespawn - a player is taking this weapon, should 
// it respawn?
//=========================================================
void CBasePlayerItem :: CheckRespawn ( void )
{
	switch ( g_pGameRules->WeaponShouldRespawn( this ) )
	{
	case GR_WEAPON_RESPAWN_YES:
		Respawn();
		break;
	case GR_WEAPON_RESPAWN_NO:
		return;
		break;
	}
}

//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CBasePlayerItem::Respawn( void )
{
	// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = CBaseEntity::Create( (char *)STRING( pev->classname ), g_pGameRules->VecWeaponRespawnSpot( this ), GetAbsAngles(), pev->owner );

	if ( pNewWeapon )
	{
		pNewWeapon->pev->effects |= EF_NODRAW;// invisible for now
		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetThink( &CBasePlayerItem::AttemptToMaterialize );

		UTIL_DropToFloor( this );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->pev->nextthink = g_pGameRules->FlWeaponRespawnTime( this );
	}
	else
	{
		ALERT ( at_console, "Respawn failed to create %s!\n", STRING( pev->classname ) );
	}

	return pNewWeapon;
}

void CBasePlayerItem::DefaultTouch( CBaseEntity *pOther )
{
	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	// can I have this?
	if ( !g_pGameRules->CanHavePlayerItem( pPlayer, this ) )
	{
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( this );
		}
		return;
	}

	if (pOther->AddPlayerItem( this ))
	{
		AttachToPlayer( pPlayer );
		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM);
	}

	SUB_UseTargets( pOther, USE_TOGGLE, 0 ); // UNDONE: when should this happen?
}



void CBasePlayerItem::DestroyItem( void )
{
	if ( m_pPlayer )
	{
		// if attached to a player, remove. 
		m_pPlayer->RemovePlayerItem( this );
	}

	Kill( );
}

int CBasePlayerItem::AddToPlayer( CBasePlayer *pPlayer )
{
	m_pPlayer = pPlayer;

	return TRUE;
}

void CBasePlayerItem::Drop( void )
{
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + .1;
}

void CBasePlayerItem::Kill( void )
{
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove);
	pev->nextthink = gpGlobals->time + .1;
}

void CBasePlayerItem::Holster( void )
{ 
	m_pPlayer->pev->viewmodel = 0; 
	m_pPlayer->pev->weaponmodel = 0;
}

void CBasePlayerItem::AttachToPlayer ( CBasePlayer *pPlayer )
{
	pev->movetype = MOVETYPE_FOLLOW;
	pev->solid = SOLID_NOT;
	pev->aiment = pPlayer->edict();
	pev->effects = EF_NODRAW; // ??
	pev->modelindex = 0;// server won't send down to clients if modelindex == 0
	pev->model = iStringNull;
	pev->owner = pPlayer->edict();
	pev->nextthink = gpGlobals->time + .1;

	SetParent( 0 );
	SetTouch( NULL );
	SetThink( NULL );
}

// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
int CBasePlayerWeapon::AddDuplicate( CBasePlayerItem *pOriginal )
{
	if ( m_pWeaponContext->m_iDefaultAmmo )
	{
		return ExtractAmmo( (CBasePlayerWeapon *)pOriginal );
	}
	else
	{
		// a dead player dropped this.
		return ExtractClipAmmo( (CBasePlayerWeapon *)pOriginal );
	}
}

CBasePlayerWeapon::CBasePlayerWeapon() :
	m_pWeaponContext(nullptr)
{
}

CBasePlayerWeapon::~CBasePlayerWeapon()
{
}

int CBasePlayerWeapon::AddToPlayer( CBasePlayer *pPlayer )
{
	int bResult = CBasePlayerItem::AddToPlayer( pPlayer );

	pPlayer->AddWeapon( m_pWeaponContext->m_iId );

	if ( !m_pWeaponContext->m_iPrimaryAmmoType )
	{
		m_pWeaponContext->m_iPrimaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo1() );
		m_pWeaponContext->m_iSecondaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo2() );
	}

	if (bResult)
		return AddWeapon( );

	return FALSE;
}

int CBasePlayerWeapon::UpdateClientData( CBasePlayer *pPlayer )
{
	BOOL bSend = FALSE;
	int state = 0;
	if ( pPlayer->m_pActiveItem == this )
	{
		if ( pPlayer->m_fOnTarget )
			state = WEAPON_IS_ONTARGET;
		else
			state = 1;
	}

	// Forcing send of all data!
	if ( !pPlayer->m_fWeapon )
	{
		bSend = TRUE;
	}
	
	// This is the current or last weapon, so the state will need to be updated
	if ( this == pPlayer->m_pActiveItem ||
		 this == pPlayer->m_pClientActiveItem )
	{
		if ( pPlayer->m_pActiveItem != pPlayer->m_pClientActiveItem )
		{
			bSend = TRUE;
		}
	}

	// If the ammo, state, or fov has changed, update the weapon
	if ( m_pWeaponContext->m_iClip != m_pWeaponContext->m_iClientClip || 
		 state != m_pWeaponContext->m_iClientWeaponState || 
		 pPlayer->m_iFOV != pPlayer->m_iClientFOV )
	{
		bSend = TRUE;
	}

	if ( bSend )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pPlayer->pev );
			WRITE_BYTE( state );
			WRITE_BYTE( m_pWeaponContext->m_iId );
			WRITE_BYTE( m_pWeaponContext->m_iClip );
		MESSAGE_END();

		m_pWeaponContext->m_iClientClip = m_pWeaponContext->m_iClip;
		m_pWeaponContext->m_iClientWeaponState = state;
		pPlayer->m_fWeapon = TRUE;
	}

	if ( m_pNext )
		m_pNext->UpdateClientData( pPlayer );

	return 1;
}

void CBasePlayerWeapon::ItemPostFrame()
{
	m_pWeaponContext->ItemPostFrame();
}

BOOL CBasePlayerWeapon :: AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry )
{
	int iIdAmmo;

	if (iMaxClip < 1)
	{
		m_pWeaponContext->m_iClip = -1;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}
	else if (m_pWeaponContext->m_iClip == 0)
	{
		int i;
		i = Q_min( m_pWeaponContext->m_iClip + iCount, iMaxClip ) - m_pWeaponContext->m_iClip;
		m_pWeaponContext->m_iClip += i;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount - i, szName, iMaxCarry );
	}
	else
	{
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}
	
	// m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = iMaxCarry; // hack for testing

	if (iIdAmmo > 0)
	{
		m_pWeaponContext->m_iPrimaryAmmoType = iIdAmmo;
		if (m_pPlayer->HasPlayerItem( this ) )
		{
			// play the "got ammo" sound only if we gave some ammo to a player that already had this gun.
			// if the player is just getting this gun for the first time, DefaultTouch will play the "picked up gun" sound for us.
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
	}

	return iIdAmmo > 0 ? TRUE : FALSE;
}


BOOL CBasePlayerWeapon :: AddSecondaryAmmo( int iCount, char *szName, int iMax )
{
	int iIdAmmo;

	iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMax );

	//m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] = iMax; // hack for testing

	if (iIdAmmo > 0)
	{
		m_pWeaponContext->m_iSecondaryAmmoType = iIdAmmo;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
	}
	return iIdAmmo > 0 ? TRUE : FALSE;
}

BEGIN_DATADESC( CBasePlayerAmmo )
	DEFINE_FUNCTION( DefaultTouch ),
	DEFINE_FUNCTION( Materialize ),
END_DATADESC()

void CBasePlayerAmmo::Spawn( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));

	SetTouch( &CBasePlayerAmmo::DefaultTouch );
}

CBaseEntity* CBasePlayerAmmo::Respawn( void )
{
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );

	UTIL_SetOrigin( this, g_pGameRules->VecAmmoRespawnSpot( this ) );// move to wherever I'm supposed to repawn.

	SetThink( &CBasePlayerAmmo::Materialize );
	pev->nextthink = g_pGameRules->FlAmmoRespawnTime( this );

	return this;
}

void CBasePlayerAmmo::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	SetTouch( &CBasePlayerAmmo::DefaultTouch );
}

void CBasePlayerAmmo :: DefaultTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	if (AddAmmo( pOther ))
	{
		if ( g_pGameRules->AmmoShouldRespawn( this ) == GR_AMMO_RESPAWN_YES )
		{
			Respawn();
		}
		else
		{
			SetTouch( NULL );
			SetThink( &CBaseEntity::SUB_Remove);
			pev->nextthink = gpGlobals->time + .1;
		}
	}
	else if( gEvilImpulse101 )
	{
		// evil impulse 101 hack, kill always
		UTIL_Remove( this );
	}
}

//=========================================================
// called by the new item with the existing item as parameter
//
// if we call ExtractAmmo(), it's because the player is picking up this type of weapon for 
// the first time. If it is spawned by the world, m_iDefaultAmmo will have a default ammo amount in it.
// if  this is a weapon dropped by a dying player, has 0 m_iDefaultAmmo, which means only the ammo in 
// the weapon clip comes along. 
//=========================================================
int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon *pWeapon )
{
	int	iReturn = 0;

	if ( pszAmmo1() != NULL )
	{
		// blindly call with m_iDefaultAmmo. It's either going to be a value or zero. If it is zero,
		// we only get the ammo in the weapon's clip, which is what we want. 
		iReturn = pWeapon->AddPrimaryAmmo( m_pWeaponContext->m_iDefaultAmmo, (char *)pszAmmo1(), iMaxClip(), iMaxAmmo1() );
		m_pWeaponContext->m_iDefaultAmmo = 0;
	}

	if ( pszAmmo2() != NULL )
	{
		iReturn = pWeapon->AddSecondaryAmmo( 0, (char *)pszAmmo2(), iMaxAmmo2() );
	}

	return iReturn;
}

//=========================================================
// called by the new item's class with the existing item as parameter
//=========================================================
int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon *pWeapon )
{
	int			iAmmo;

	if ( m_pWeaponContext->m_iClip == WEAPON_NOCLIP )
	{
		iAmmo = 0;// guns with no clips always come empty if they are second-hand
	}
	else
	{
		iAmmo = m_pWeaponContext->m_iClip;
	}
	
	return pWeapon->m_pPlayer->GiveAmmo( iAmmo, (char *)pszAmmo1(), iMaxAmmo1() ); // , &m_iPrimaryAmmoType
}
	
//=========================================================
// RetireWeapon - no more ammo for this gun, put it away.
//=========================================================
void CBasePlayerWeapon::RetireWeapon( void )
{
	// first, no viewmodel at all.
	m_pPlayer->pev->viewmodel = iStringNull;
	m_pPlayer->pev->weaponmodel = iStringNull;
	//m_pPlayer->pev->viewmodelindex = NULL;

	g_pGameRules->GetNextBestWeapon( m_pPlayer, this );
}

//*********************************************************
// weaponbox code:
//*********************************************************

LINK_ENTITY_TO_CLASS( weaponbox, CWeaponBox );

BEGIN_DATADESC( CWeaponBox )
	DEFINE_ARRAY( m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( m_rgiszAmmo, FIELD_STRING, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( m_cAmmoTypes, FIELD_INTEGER ),
	DEFINE_FUNCTION( Kill ),
END_DATADESC()

//=========================================================
//
//=========================================================
void CWeaponBox::Precache( void )
{
	PRECACHE_MODEL("models/w_weaponbox.mdl");
}

//=========================================================
//=========================================================
void CWeaponBox :: KeyValue( KeyValueData *pkvd )
{
	// get support for spirit field too
	if( FStrEq( pkvd->szKeyName, "parent" ) || FStrEq( pkvd->szKeyName, "movewith" ))
	{
		m_iParent = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "reflection" ))
	{
		switch(Q_atoi(pkvd->szValue))
		{
		case 1: pev->effects |= EF_NOREFLECT; break;
		case 2: pev->effects |= EF_REFLECTONLY; break;
		}
	}
	else if ( m_cAmmoTypes < MAX_AMMO_SLOTS )
	{
		PackAmmo( ALLOC_STRING(pkvd->szKeyName), atoi(pkvd->szValue) );
		m_cAmmoTypes++;// count this new ammo type.

		pkvd->fHandled = TRUE;
	}
	else
	{
		ALERT ( at_console, "WeaponBox too full! only %d ammotypes allowed\n", MAX_AMMO_SLOTS );
	}
}

//=========================================================
// CWeaponBox - Spawn 
//=========================================================
void CWeaponBox::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	SET_MODEL( ENT(pev), "models/w_weaponbox.mdl");
}

//=========================================================
// CWeaponBox - Kill - the think function that removes the
// box from the world.
//=========================================================
void CWeaponBox::Kill( void )
{
	CBasePlayerItem *pWeapon;
	int i;

	// destroy the weapons
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pWeapon = m_rgpPlayerItems[ i ];

		while ( pWeapon )
		{
			pWeapon->SetThink( &CBaseEntity::SUB_Remove);
			pWeapon->pev->nextthink = gpGlobals->time + 0.1;
			pWeapon = pWeapon->m_pNext;
		}
	}

	// remove the box
	UTIL_Remove( this );
}

//=========================================================
// CWeaponBox - Touch: try to add my contents to the toucher
// if the toucher is a player.
//=========================================================
void CWeaponBox::Touch( CBaseEntity *pOther )
{
	if ( !(pev->flags & FL_ONGROUND ) )
	{
		return;
	}

	if ( !pOther->IsPlayer() )
	{
		// only players may touch a weaponbox.
		return;
	}

	if ( !pOther->IsAlive() )
	{
		// no dead guys.
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	int i;

// dole out ammo
	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// there's some ammo of this type. 
			pPlayer->GiveAmmo( m_rgAmmo[ i ], (char *)STRING( m_rgiszAmmo[ i ] ), MaxAmmoCarry( m_rgiszAmmo[ i ] ) );

			//ALERT ( at_console, "Gave %d rounds of %s\n", m_rgAmmo[i], STRING(m_rgiszAmmo[i]) );

			// now empty the ammo from the weaponbox since we just gave it to the player
			m_rgiszAmmo[ i ] = iStringNull;
			m_rgAmmo[ i ] = 0;
		}
	}

// go through my weapons and try to give the usable ones to the player. 
// it's important the the player be given ammo first, so the weapons code doesn't refuse 
// to deploy a better weapon that the player may pick up because he has no ammo for it.
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			CBasePlayerItem *pItem;

			// have at least one weapon in this slot
			while ( m_rgpPlayerItems[ i ] )
			{
				//ALERT ( at_console, "trying to give %s\n", STRING( m_rgpPlayerItems[ i ]->pev->classname ) );

				pItem = m_rgpPlayerItems[ i ];
				m_rgpPlayerItems[ i ] = m_rgpPlayerItems[ i ]->m_pNext;// unlink this weapon from the box

				if ( pPlayer->AddPlayerItem( pItem ) )
				{
					pItem->AttachToPlayer( pPlayer );
				}
			}
		}
	}

	EMIT_SOUND( pOther->edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
	SetTouch( NULL);
	UTIL_Remove(this);
}

//=========================================================
// CWeaponBox - PackWeapon: Add this weapon to the box
//=========================================================
BOOL CWeaponBox::PackWeapon( CBasePlayerItem *pWeapon )
{
	// is one of these weapons already packed in this box?
	if ( HasWeapon( pWeapon ) )
	{
		return FALSE;// box can only hold one of each weapon type
	}

	if ( pWeapon->m_pPlayer )
	{
		if ( !pWeapon->m_pPlayer->RemovePlayerItem( pWeapon ) )
		{
			// failed to unhook the weapon from the player!
			return FALSE;
		}
	}

	int iWeaponSlot = pWeapon->iItemSlot();
	
	if ( m_rgpPlayerItems[ iWeaponSlot ] )
	{
		// there's already one weapon in this slot, so link this into the slot's column
		pWeapon->m_pNext = m_rgpPlayerItems[ iWeaponSlot ];	
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
	}
	else
	{
		// first weapon we have for this slot
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
		pWeapon->m_pNext = NULL;	
	}

	pWeapon->pev->spawnflags |= SF_NORESPAWN;// never respawn
	pWeapon->pev->movetype = MOVETYPE_NONE;
	pWeapon->pev->solid = SOLID_NOT;
	pWeapon->pev->effects = EF_NODRAW;
	pWeapon->pev->modelindex = 0;
	pWeapon->pev->model = iStringNull;
	pWeapon->pev->owner = edict();
	pWeapon->SetThink( NULL );// crowbar may be trying to swing again, etc.
	pWeapon->SetTouch( NULL );
	pWeapon->m_pPlayer = NULL;

	//ALERT ( at_console, "packed %s\n", STRING(pWeapon->pev->classname) );

	return TRUE;
}

//=========================================================
// CWeaponBox - PackAmmo
//=========================================================
BOOL CWeaponBox::PackAmmo( int iszName, int iCount )
{
	int iMaxCarry;

	if ( FStringNull( iszName ) )
	{
		// error here
		ALERT ( at_console, "NULL String in PackAmmo!\n" );
		return FALSE;
	}
	
	iMaxCarry = MaxAmmoCarry( iszName );

	if ( iMaxCarry != -1 && iCount > 0 )
	{
		//ALERT ( at_console, "Packed %d rounds of %s\n", iCount, STRING(iszName) );
		GiveAmmo( iCount, (char *)STRING( iszName ), iMaxCarry );
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox - GiveAmmo
//=========================================================
int CWeaponBox::GiveAmmo( int iCount, char *szName, int iMax, int *pIndex/* = NULL*/ )
{
	int i;

	for (i = 1; i < MAX_AMMO_SLOTS && !FStringNull( m_rgiszAmmo[i] ); i++)
	{
		if (stricmp( szName, STRING( m_rgiszAmmo[i])) == 0)
		{
			if (pIndex)
				*pIndex = i;

			int iAdd = Q_min( iCount, iMax - m_rgAmmo[i]);
			if (iCount == 0 || iAdd > 0)
			{
				m_rgAmmo[i] += iAdd;

				return i;
			}
			return -1;
		}
	}
	if (i < MAX_AMMO_SLOTS)
	{
		if (pIndex)
			*pIndex = i;

		m_rgiszAmmo[i] = MAKE_STRING( szName );
		m_rgAmmo[i] = iCount;

		return i;
	}
	ALERT( at_console, "out of named ammo slots\n");
	return i;
}

//=========================================================
// CWeaponBox::HasWeapon - is a weapon of this type already
// packed in this box?
//=========================================================
BOOL CWeaponBox::HasWeapon( CBasePlayerItem *pCheckItem )
{
	CBasePlayerItem *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs( pItem->pev, STRING( pCheckItem->pev->classname) ))
		{
			return TRUE;
		}
		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox::IsEmpty - is there anything in this box?
//=========================================================
BOOL CWeaponBox::IsEmpty( void )
{
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			return FALSE;
		}
	}

	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// still have a bit of this type of ammo
			return FALSE;
		}
	}

	return TRUE;
}

//=========================================================
//=========================================================
void CWeaponBox::SetObjectCollisionBox( void )
{
	pev->absmin = GetAbsOrigin() + Vector(-16, -16, 0);
	pev->absmax = GetAbsOrigin() + Vector( 16, 16, 16); 
}

void CBasePlayerWeapon::PrintState( void )
{
	ALERT( at_console, "primary:  %f\n", m_pWeaponContext->m_flNextPrimaryAttack );
	ALERT( at_console, "idle   :  %f\n", m_pWeaponContext->m_flTimeWeaponIdle );

//	ALERT( at_console, "nextrl :  %f\n", m_flNextReload );
//	ALERT( at_console, "nextpum:  %f\n", m_flPumpTime );

//	ALERT( at_console, "m_frt  :  %f\n", m_fReloadTime );
	ALERT( at_console, "m_finre:  %i\n", m_pWeaponContext->m_fInReload );
//	ALERT( at_console, "m_finsr:  %i\n", m_fInSpecialReload );

	ALERT( at_console, "m_iclip:  %i\n", m_pWeaponContext->m_iClip );
}

//=========================================================
//=========================================================
BEGIN_DATADESC( CLaserSpot )
	DEFINE_FUNCTION( Revive ),
END_DATADESC()

CLaserSpot *CLaserSpot::CreateSpot( void )
{
	CLaserSpot *pSpot = GetClassPtr( (CLaserSpot *)NULL );
	pSpot->Spawn();

	pSpot->pev->classname = MAKE_STRING("laser_spot");

	return pSpot;
}

//=========================================================
//=========================================================
LINK_ENTITY_TO_CLASS( laser_spot, CLaserSpot );

void CLaserSpot::Spawn( void )
{
	Precache( );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	pev->rendermode = kRenderGlow;
	pev->renderfx = kRenderFxNoDissipation;
	pev->renderamt = 255;

	SET_MODEL(edict(), "sprites/laserdot.spr");
}

//=========================================================
// Suspend- make the laser sight invisible. 
//=========================================================
void CLaserSpot::Suspend( float flSuspendTime )
{
	pev->effects |= EF_NODRAW;
	
	SetThink( &CLaserSpot::Revive );
	pev->nextthink = gpGlobals->time + flSuspendTime;
}

//=========================================================
// Revive - bring a suspended laser sight back.
//=========================================================
void CLaserSpot::Revive( void )
{
	pev->effects &= ~EF_NODRAW;

	SetThink( NULL );
}

void CLaserSpot::Precache( void )
{
	PRECACHE_MODEL("sprites/laserdot.spr");
}
