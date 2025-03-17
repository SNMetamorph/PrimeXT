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
#ifndef WEAPONS_H
#define WEAPONS_H

#include "cbase.h"
#include "effects.h"
#include "item_info.h"
#include "weapon_context.h"
#include <memory>

class CBasePlayer;

void DeactivateSatchels( CBasePlayer *pOwner );

// constant items
#define ITEM_HEALTHKIT		1
#define ITEM_ANTIDOTE		2
#define ITEM_SECURITY		3
#define ITEM_BATTERY		4

#define WEAPON_NONE				0

#define MAX_NORMAL_BATTERY	100

// the default amount of ammo that comes with each gun when it spawns
#define MP5_M203_DEFAULT_GIVE		0

// The amount of ammo given to a player by an ammo item.
#define AMMO_URANIUMBOX_GIVE	20
#define AMMO_CHAINBOX_GIVE		200
#define AMMO_M203BOX_GIVE		2
#define AMMO_BUCKSHOTBOX_GIVE	12
#define AMMO_RPGCLIP_GIVE		1
#define AMMO_URANIUMBOX_GIVE	20
#define AMMO_SNARKBOX_GIVE		5

#define WEAPON_IS_ONTARGET 0x40

// Items that the player has in their inventory that they can use
class CBasePlayerItem : public CBaseAnimating
{
	DECLARE_CLASS( CBasePlayerItem, CBaseAnimating );
public:
	virtual void SetObjectCollisionBox( void );

	DECLARE_DATADESC();

	virtual int AddToPlayer( CBasePlayer *pPlayer );	// return TRUE if the item you want the item added to the player inventory
	virtual int AddDuplicate( CBasePlayerItem *pItem ) { return FALSE; }	// return TRUE if you want your duplicate removed from world
	void DestroyItem( void );
	void DefaultTouch( CBaseEntity *pOther );	// default weapon touch
	void FallThink ( void );// when an item is first spawned, this think is run to determine when the object has hit the ground.
	void Materialize( void );// make a weapon visible and tangible
	void AttemptToMaterialize( void );  // the weapon desires to become visible and tangible, if the game rules allow for it
	CBaseEntity* Respawn ( void );// copy a weapon
	void FallInit( void );
	void CheckRespawn( void );
	virtual int GetItemInfo(ItemInfo *p) const { return 0; };	// returns 0 if struct not filled out
	virtual BOOL CanDeploy( void ) { return TRUE; };
	virtual BOOL Deploy( ) { return TRUE; };		// returns is deploy was successful
		 
	virtual BOOL CanHolster( void ) { return TRUE; };		// can this weapon be put away right now?
	virtual void Holster( void );
	virtual void UpdateItemInfo( void ) { return; };

	virtual void ItemPreFrame( void ) { return; }		// called each frame by the player PreThink
	virtual void ItemPostFrame( void ) { return; }		// called each frame by the player PostThink

	virtual void Drop( void );
	virtual void Kill( void );
	virtual void AttachToPlayer ( CBasePlayer *pPlayer );

	virtual int PrimaryAmmoIndex() { return -1; };
	virtual int SecondaryAmmoIndex() { return -1; };

	virtual int UpdateClientData( CBasePlayer *pPlayer ) { return 0; }

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return NULL; };

	CBasePlayer	*m_pPlayer;
	CBasePlayerItem	*m_pNext;

	virtual int	iItemSlot() = 0;
	virtual int	iItemPosition() = 0;
	virtual const char *pszAmmo1() = 0;
	virtual int	iMaxAmmo1() = 0;
	virtual const char *pszAmmo2() = 0;
	virtual int	iMaxAmmo2() = 0;
	virtual const char *pszName() = 0;
	virtual int	iMaxClip() = 0;
	virtual int	iWeight() = 0;
	virtual int	iFlags() = 0;
	virtual int	iWeaponID() = 0;
};

// inventory items that 
class CBasePlayerWeapon : public CBasePlayerItem
{
	DECLARE_CLASS( CBasePlayerWeapon, CBasePlayerItem );
public:
	DECLARE_DATADESC();

	CBasePlayerWeapon();
	~CBasePlayerWeapon();

	// generic weapon versions of CBasePlayerItem calls
	virtual int AddToPlayer( CBasePlayer *pPlayer ) override; 
	virtual int AddDuplicate( CBasePlayerItem *pItem ) override;
	
	int UpdateClientData( CBasePlayer *pPlayer ) override;	// sends hud info to client dll, if things have changed
	virtual void ItemPostFrame() override;	// called each frame by the player PostThink

	int	PrimaryAmmoIndex() override { return m_pWeaponContext->PrimaryAmmoIndex(); }; // forward to weapon logic
	int	SecondaryAmmoIndex() override { return m_pWeaponContext->SecondaryAmmoIndex(); }; // forward to weapon logic

	virtual int GetItemInfo(ItemInfo *p) const override { return m_pWeaponContext->GetItemInfo(p); };	// returns 0 if struct not filled out
	virtual BOOL CanDeploy( void ) override { return m_pWeaponContext->CanDeploy(); };
	virtual BOOL Deploy() override { return m_pWeaponContext->Deploy(); };					// returns is deploy was successful	 
	virtual BOOL CanHolster( void ) override { return m_pWeaponContext->CanHolster(); };	// can this weapon be put away right nxow?
	virtual void Holster(void) override { m_pWeaponContext->Holster(); };

	void UpdateItemInfo( void ) override {};	// updates HUD state
	CBasePlayerItem *GetWeaponPtr( void ) override { return (CBasePlayerItem *)this; };
	void RetireWeapon( void );

	int iItemSlot() override		{ return m_pWeaponContext->iItemSlot(); }
	int	iItemPosition() override	{ return m_pWeaponContext->iItemPosition(); }
	const char *pszAmmo1() override	{ return m_pWeaponContext->pszAmmo1(); }
	int iMaxAmmo1() override		{ return m_pWeaponContext->iMaxAmmo1(); }
	const char *pszAmmo2() override	{ return m_pWeaponContext->pszAmmo2(); }
	int	iMaxAmmo2() override		{ return m_pWeaponContext->iMaxAmmo2(); }
	const char *pszName() override	{ return m_pWeaponContext->pszName(); }
	int	iMaxClip() override			{ return m_pWeaponContext->iMaxClip(); }
	int	iWeight() override			{ return m_pWeaponContext->iWeight(); }
	int	iFlags() override			{ return m_pWeaponContext->iFlags(); }
	int iWeaponID() override		{ return m_pWeaponContext->m_iId; }

	std::unique_ptr<CBaseWeaponContext> m_pWeaponContext;

protected:
	int ExtractAmmo( CBasePlayerWeapon *pWeapon );		// Return TRUE if you can add ammo to yourself when picked up
	int ExtractClipAmmo( CBasePlayerWeapon *pWeapon );		// Return TRUE if you can add ammo to yourself when picked up

	int AddWeapon( void ) { ExtractAmmo( this ); return TRUE; };	// Return TRUE if you want to add yourself to the player

	// generic "shared" ammo handlers
	BOOL AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry );
	BOOL AddSecondaryAmmo( int iCount, char *szName, int iMaxCarry );
	void PrintState( void );
};

class CBasePlayerAmmo : public CBaseEntity
{
	DECLARE_CLASS( CBasePlayerAmmo, CBaseEntity );
public:
	virtual void Spawn( void );
	void DefaultTouch( CBaseEntity *pOther ); // default weapon touch
	virtual BOOL AddAmmo( CBaseEntity *pOther ) { return TRUE; };

	CBaseEntity* Respawn( void );
	void Materialize( void );

	DECLARE_DATADESC();
};

extern DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
extern DLL_GLOBAL	const char *g_pModelNameLaser;

extern DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
extern DLL_GLOBAL	short	g_sModelIndexFireball;// holds the index for the fireball
extern DLL_GLOBAL	short	g_sModelIndexSmoke;// holds the index for the smoke cloud
extern DLL_GLOBAL	short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
extern DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model
extern DLL_GLOBAL	short	g_sModelIndexBloodDrop;// holds the sprite index for blood drops
extern DLL_GLOBAL	short	g_sModelIndexBloodSpray;// holds the sprite index for blood spray (bigger)

extern void ClearMultiDamage(void);
extern void ApplyMultiDamage(entvars_t* pevInflictor, entvars_t* pevAttacker );
extern void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType);

extern void DecalGunshot( TraceResult *pTrace, int iBulletType, const vec3_t &origin, const vec3_t &vecEnd );
extern void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage);
extern const char* DamageDecal( CBaseEntity *pEntity, int bitsDamageType );
extern void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType );

typedef struct 
{
	CBaseEntity		*pEntity;
	float			amount;
	int			type;
} MULTIDAMAGE;

extern MULTIDAMAGE gMultiDamage;


#define LOUD_GUN_VOLUME		1000
#define NORMAL_GUN_VOLUME		600
#define QUIET_GUN_VOLUME		200

#define BRIGHT_GUN_FLASH		512
#define NORMAL_GUN_FLASH		256
#define DIM_GUN_FLASH		128

#define BIG_EXPLOSION_VOLUME	2048
#define NORMAL_EXPLOSION_VOLUME	1024
#define SMALL_EXPLOSION_VOLUME	512

#define WEAPON_ACTIVITY_VOLUME	64

//=========================================================
// CWeaponBox - a single entity that can store weapons
// and ammo. 
//=========================================================
class CWeaponBox : public CBaseEntity
{
	DECLARE_CLASS( CWeaponBox, CBaseEntity );

	void Precache( void );
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	BOOL IsEmpty( void );
	int  GiveAmmo( int iCount, char *szName, int iMax, int *pIndex = NULL );
	void SetObjectCollisionBox( void );

public:
	void Kill ( void );

	DECLARE_DATADESC();

	BOOL HasWeapon( CBasePlayerItem *pCheckItem );
	BOOL PackWeapon( CBasePlayerItem *pWeapon );
	BOOL PackAmmo( int iszName, int iCount );
	
	CBasePlayerItem	*m_rgpPlayerItems[MAX_ITEM_TYPES];// one slot for each 

	int m_rgiszAmmo[MAX_AMMO_SLOTS];// ammo names
	int m_rgAmmo[MAX_AMMO_SLOTS];// ammo quantities

	int m_cAmmoTypes;// how many ammo types packed into this box (if packed by a level designer)
};

class CLaserSpot : public CBaseEntity
{
	DECLARE_CLASS( CLaserSpot, CBaseEntity );

	void Spawn( void );
	void Precache( void );

	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }

public:
	void Suspend( float flSuspendTime );
	void Revive( void );

	DECLARE_DATADESC();
	
	static CLaserSpot *CreateSpot( void );
};

#endif // WEAPONS_H
