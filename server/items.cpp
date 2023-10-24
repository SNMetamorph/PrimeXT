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

===== items.cpp ========================================================

  functions governing the selection/use of weapons for players

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "skill.h"
#include "items.h"
#include "gamerules.h"
#include "UserMessages.h"

class CWorldItem : public CBaseEntity
{
public:
	void	KeyValue(KeyValueData *pkvd ); 
	void	Spawn( void );
	int	m_iType;
};

LINK_ENTITY_TO_CLASS(world_items, CWorldItem);

void CWorldItem::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		m_iType = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CWorldItem::Spawn( void )
{
	CBaseEntity *pEntity = NULL;

	switch (m_iType) 
	{
	case 44: // ITEM_BATTERY:
		pEntity = CBaseEntity::Create( "item_battery", GetAbsOrigin(), GetAbsAngles() );
		break;
	case 42: // ITEM_ANTIDOTE:
		pEntity = CBaseEntity::Create( "item_antidote", GetAbsOrigin(), GetAbsAngles() );
		break;
	case 43: // ITEM_SECURITY:
		pEntity = CBaseEntity::Create( "item_security", GetAbsOrigin(), GetAbsAngles() );
		break;
	case 45: // ITEM_SUIT:
		pEntity = CBaseEntity::Create( "item_suit", GetAbsOrigin(), GetAbsAngles() );
		break;
	}

	if (!pEntity)
	{
		ALERT( at_console, "unable to create world_item %d\n", m_iType );
	}
	else
	{
		pEntity->pev->target = pev->target;
		pEntity->pev->targetname = pev->targetname;
		pEntity->pev->spawnflags = pev->spawnflags;
	}

	REMOVE_ENTITY(edict());
}

BEGIN_DATADESC( CItem )
	DEFINE_FUNCTION( ItemTouch ),
	DEFINE_FUNCTION( Materialize ),
	DEFINE_FUNCTION( StartItem ),
END_DATADESC()

void CItem::Spawn( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin( this, GetLocalOrigin( ));
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ));

	SetTouch( &CItem::ItemTouch );
	SetThink( &CItem::StartItem );
	SetNextThink( 0.1f );
}

extern int gEvilImpulse101;

void CItem::ItemTouch( CBaseEntity *pOther )
{
	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	// ok, a player is touching this item, but can he have it?
	if ( !g_pGameRules->CanHaveItem( pPlayer, this ) )
	{
		// no? Ignore the touch.
		return;
	}

	if( IsLockedByMaster( pOther ))
		return;

	if (MyTouch( pPlayer ))
	{
		SUB_UseTargets( pOther, USE_TOGGLE, 0 );
		SetParent( 0 );
		SetTouch( NULL );
		
		// player grabbed the item. 
		g_pGameRules->PlayerGotItem( pPlayer, this );
		if ( g_pGameRules->ItemShouldRespawn( this ) == GR_ITEM_RESPAWN_YES )
		{
			Respawn(); 
		}
		else
		{
			UTIL_Remove( this );
		}
	}
	else if (gEvilImpulse101)
	{
		UTIL_Remove( this );
	}
}

CBaseEntity* CItem::Respawn( void )
{
	SetTouch( NULL );
	pev->effects |= EF_NODRAW;

	UTIL_SetOrigin( this, g_pGameRules->VecItemRespawnSpot( this ) );// blip to whereever you should respawn.

	SetThink( &CItem::Materialize );
	pev->nextthink = g_pGameRules->FlItemRespawnTime( this ); 
	return this;
}

void CItem :: StartItem( void )
{
	if( UTIL_DropToFloor( this ) == 0 )
	{
		ALERT( at_error, "Item %s fell out of level at %f,%f,%f\n", STRING( pev->classname ),
		GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
		UTIL_Remove( this );
		return;
	}

	SetThink( NULL );
	SetTouch( &CItem::ItemTouch );
}

void CItem::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	SetTouch( &CItem::ItemTouch );
}

#define SF_SUIT_SHORTLOGON		0x0001

class CItemSuit : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_suit.mdl");
		CItem::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_suit.mdl");
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		if ( pPlayer->HasWeapon( WEAPON_SUIT ))
			return FALSE;

		// g-cont. disable logon on "give"
		if( !FBitSet( pev->spawnflags, SF_NORESPAWN ))
		{
			if ( pev->spawnflags & SF_SUIT_SHORTLOGON )
				EMIT_SOUND_SUIT(pPlayer->edict(), "!HEV_A0");	// short version of suit logon,
			else
				EMIT_SOUND_SUIT(pPlayer->edict(), "!HEV_AAx");	// long version of suit logon
		}

		pPlayer->AddWeapon( WEAPON_SUIT );
		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS(item_suit, CItemSuit);



class CItemBattery : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		if (pev->model)
			SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
		else
			SET_MODEL(ENT(pev), "models/w_battery.mdl");
		CItem::Spawn( );
	}
	void Precache( void )
	{
		if (pev->model)
			PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
		else
			PRECACHE_MODEL ("models/w_battery.mdl");

		if (pev->noise)
			PRECACHE_SOUND( (char*)STRING(pev->noise) ); //LRC
		else
			PRECACHE_SOUND( "items/gunpickup2.wav" );
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		if ( pPlayer->pev->deadflag != DEAD_NO )
		{
			return FALSE;
		}

		if ((pPlayer->pev->armorvalue < MAX_NORMAL_BATTERY) && pPlayer->HasWeapon( WEAPON_SUIT ))
		{
			int pct;
			char szcharge[64];

			pPlayer->pev->armorvalue += gSkillData.batteryCapacity;
			pPlayer->pev->armorvalue = Q_min(pPlayer->pev->armorvalue, MAX_NORMAL_BATTERY);

			if (pev->noise) EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, STRING(pev->noise), 1, ATTN_NORM ); //LRC
			else EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );

			MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev );
				WRITE_STRING( STRING(pev->classname) );
			MESSAGE_END();

			
			// Suit reports new power level
			// For some reason this wasn't working in release build -- round it.
			pct = (int)( (float)(pPlayer->pev->armorvalue * 100.0) * (1.0/MAX_NORMAL_BATTERY) + 0.5);
			pct = (pct / 5);
			if (pct > 0)
				pct--;
		
			sprintf( szcharge,"!HEV_%1dP", pct );
			
			//EMIT_SOUND_SUIT(ENT(pev), szcharge);
			pPlayer->SetSuitUpdate(szcharge, FALSE, SUIT_NEXT_IN_30SEC);
			return TRUE;		
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS(item_battery, CItemBattery);


class CItemAntidote : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_antidote.mdl");
		CItem::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_antidote.mdl");
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		pPlayer->SetSuitUpdate("!HEV_DET4", FALSE, SUIT_NEXT_IN_1MIN);
		
		pPlayer->m_rgItems[ITEM_ANTIDOTE] += 1;
		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS(item_antidote, CItemAntidote);


class CItemSecurity : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_security.mdl");
		CItem::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_security.mdl");
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		pPlayer->m_rgItems[ITEM_SECURITY] += 1;
		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS(item_security, CItemSecurity);

class CItemLongJump : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_longjump.mdl");
		CItem::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_longjump.mdl");
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		if ( pPlayer->m_fLongJump )
		{
			return FALSE;
		}

		if ( pPlayer->HasWeapon( WEAPON_SUIT ))
		{
			pPlayer->m_fLongJump = TRUE;// player now has longjump module

			g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), "slj", "1" );

			MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev );
				WRITE_STRING( STRING(pev->classname) );
			MESSAGE_END();

			EMIT_SOUND_SUIT( pPlayer->edict(), "!HEV_A1" );	// Play the longjump sound UNDONE: Kelly? correct sound?
			return TRUE;		
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( item_longjump, CItemLongJump );

#define SF_ONLY_IF_IN_SUIT	BIT( 0 )

class CItemGeneric : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL( edict(), GetModel( ));
		CItem::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL( GetModel( ));
		if( pev->noise )
			PRECACHE_SOUND( (char*)STRING(pev->noise) );
	}
	BOOL MyTouch( CBasePlayer *pPlayer )
	{
		if ( pPlayer->HasWeapon( WEAPON_SUIT ))
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev );
				if( pev->netname != NULL_STRING )
					WRITE_STRING( STRING( pev->netname ));
				else WRITE_STRING( STRING( pev->classname )); 			
			MESSAGE_END();
		}
		else if( FBitSet( pev->spawnflags, SF_ONLY_IF_IN_SUIT ))
			return FALSE;

		if( pev->noise )
			EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, STRING( pev->noise ), 1, ATTN_NORM );

		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS( item_generic, CItemGeneric );
