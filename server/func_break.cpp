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

===== bmodels.cpp ========================================================

  spawn, think, and use functions for entities that use brush models

*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "func_break.h"
#include "decals.h"
#include "explode.h"

extern DLL_GLOBAL Vector		g_vecAttackDir;

// =================== FUNC_Breakable ==============================================

// Just add more items to the bottom of this array and they will automagically be supported
// This is done instead of just a classname in the FGD so we can control which entities can
// be spawned, and still remain fairly flexible
const char *CBreakable::pSpawnObjects[] =
{
	NULL,				// 0
	"item_battery",		// 1
	"item_healthkit",	// 2
	"weapon_9mmhandgun",// 3
	"ammo_9mmclip",		// 4
	"weapon_9mmAR",		// 5
	"ammo_9mmAR",		// 6
	"ammo_ARgrenades",	// 7
	"weapon_shotgun",	// 8
	"ammo_buckshot",	// 9
	"weapon_crossbow",	// 10
	"ammo_crossbow",	// 11
	"weapon_357",		// 12
	"ammo_357",			// 13
	"weapon_rpg",		// 14
	"ammo_rpgclip",		// 15
	"ammo_gaussclip",	// 16
	"weapon_handgrenade",// 17
	"weapon_tripmine",	// 18
	"weapon_satchel",	// 19
	"weapon_snark",		// 20
	"weapon_hornetgun",	// 21
};

void CBreakable::KeyValue( KeyValueData* pkvd )
{
	// UNDONE_WC: explicitly ignoring these fields, but they shouldn't be in the map file!
	if (FStrEq(pkvd->szKeyName, "explosion"))
	{
		if (!stricmp(pkvd->szValue, "directed"))
			m_Explosion = expDirected;
		else if (!stricmp(pkvd->szValue, "random"))
			m_Explosion = expRandom;
		else
			m_Explosion = expRandom;

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "material"))
	{
		int i = atoi( pkvd->szValue);

		// 0:glass, 1:metal, 2:flesh, 3:wood

		if ((i < 0) || (i >= matLastMaterial))
			m_Material = matWood;
		else
			m_Material = (Materials)i;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deadmodel"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shards"))
	{
//		m_iShards = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "gibmodel") )
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spawnobject") )
	{
		int object = atoi( pkvd->szValue );
		if ( object > 0 && object < ARRAYSIZE(pSpawnObjects) )
			m_iszSpawnObject = MAKE_STRING( pSpawnObjects[object] );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "explodemagnitude") )
	{
		ExplosionSetMagnitude( atoi( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "lip") )
		pkvd->fHandled = TRUE;
	else
		BaseClass::KeyValue( pkvd );
}


//
// func_breakable - bmodel that breaks into pieces after taking damage
//
LINK_ENTITY_TO_CLASS( func_breakable, CBreakable );

BEGIN_DATADESC( CBreakable )
	DEFINE_FIELD( m_Material, FIELD_INTEGER ),
	DEFINE_FIELD( m_Explosion, FIELD_INTEGER ),
	DEFINE_FIELD( m_angle, FIELD_FLOAT ),
	DEFINE_FIELD( m_iszGibModel, FIELD_STRING ),
	DEFINE_FIELD( m_iszSpawnObject, FIELD_STRING ),
	DEFINE_FUNCTION( BreakTouch ),
	DEFINE_FUNCTION( Die ),
END_DATADESC()

void CBreakable::Spawn( void )
{
	Precache( );    

	if( FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY ) )
		pev->takedamage	= DAMAGE_NO;
	else
		pev->takedamage	= DAMAGE_YES;
  
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL( edict(), GetModel() );//set size and link into world.

	m_angle = GetLocalAngles().y;
	SetLocalAngles( g_vecZero );

	// g-cont. we need to reapply origin
	UTIL_SetOrigin( this, GetLocalOrigin() );

	// HACK:  matGlass can receive decals, we need the client to know about this
	//  so use class to store the material flag
	if( m_Material == matGlass )
	{
		pev->playerclass = 1;
	}

	SetTouch( &CBreakable::BreakTouch );
	if ( FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY ) )		// Only break on trigger
		SetTouch( NULL );

	// Flag unbreakable glass as "worldbrush" so it will block ALL tracelines
	if ( !IsBreakable() && pev->rendermode != kRenderNormal )
		pev->flags |= FL_WORLDBRUSH;

	if( IsPushable( )) return;

	if( m_hParent )
		m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
	else m_pUserData = WorldPhysic->CreateStaticBodyFromEntity( this );
}

const char *CBreakable::pSoundsWood[] = 
{
	"debris/wood1.wav",
	"debris/wood2.wav",
	"debris/wood3.wav",
};

const char *CBreakable::pSoundsFlesh[] = 
{
	"debris/flesh1.wav",
	"debris/flesh2.wav",
	"debris/flesh3.wav",
	"debris/flesh5.wav",
	"debris/flesh6.wav",
	"debris/flesh7.wav",
};

const char *CBreakable::pSoundsMetal[] = 
{
	"debris/metal1.wav",
	"debris/metal2.wav",
	"debris/metal3.wav",
};

const char *CBreakable::pSoundsConcrete[] = 
{
	"debris/concrete1.wav",
	"debris/concrete2.wav",
	"debris/concrete3.wav",
};


const char *CBreakable::pSoundsGlass[] = 
{
	"debris/glass1.wav",
	"debris/glass2.wav",
	"debris/glass3.wav",
};

const char **CBreakable::MaterialSoundList( Materials precacheMaterial, int &soundCount )
{
	const char	**pSoundList = NULL;

    switch ( precacheMaterial ) 
	{
	case matWood:
		pSoundList = pSoundsWood;
		soundCount = ARRAYSIZE(pSoundsWood);
		break;
	case matFlesh:
		pSoundList = pSoundsFlesh;
		soundCount = ARRAYSIZE(pSoundsFlesh);
		break;
	case matComputer:
	case matUnbreakableGlass:
	case matGlass:
		pSoundList = pSoundsGlass;
		soundCount = ARRAYSIZE(pSoundsGlass);
		break;

	case matMetal:
		pSoundList = pSoundsMetal;
		soundCount = ARRAYSIZE(pSoundsMetal);
		break;

	case matCinderBlock:
	case matRocks:
		pSoundList = pSoundsConcrete;
		soundCount = ARRAYSIZE(pSoundsConcrete);
		break;
	
	
	case matCeilingTile:
	case matNone:
	default:
		soundCount = 0;
		break;
	}

	return pSoundList;
}

void CBreakable::MaterialSoundPrecache( Materials precacheMaterial )
{
	const char	**pSoundList;
	int			i, soundCount = 0;

	pSoundList = MaterialSoundList( precacheMaterial, soundCount );

	for ( i = 0; i < soundCount; i++ )
	{
		PRECACHE_SOUND( (char *)pSoundList[i] );
	}
}

void CBreakable::MaterialSoundRandom( edict_t *pEdict, Materials soundMaterial, float volume )
{
	const char	**pSoundList;
	int			soundCount = 0;

	pSoundList = MaterialSoundList( soundMaterial, soundCount );

	if ( soundCount )
		EMIT_SOUND( pEdict, CHAN_BODY, pSoundList[ RANDOM_LONG(0,soundCount-1) ], volume, 1.0 );
}


void CBreakable::Precache( void )
{
	const char *pGibName;

	switch (m_Material) 
	{
	case matWood:
		pGibName = "models/woodgibs.mdl";
		
		PRECACHE_SOUND("debris/bustcrate1.wav");
		PRECACHE_SOUND("debris/bustcrate2.wav");
		break;
	case matFlesh:
		pGibName = "models/fleshgibs.mdl";
		
		PRECACHE_SOUND("debris/bustflesh1.wav");
		PRECACHE_SOUND("debris/bustflesh2.wav");
		break;
	case matComputer:
		PRECACHE_SOUND("buttons/spark5.wav");
		PRECACHE_SOUND("buttons/spark6.wav");
		pGibName = "models/computergibs.mdl";
		
		PRECACHE_SOUND("debris/bustmetal1.wav");
		PRECACHE_SOUND("debris/bustmetal2.wav");
		break;

	case matUnbreakableGlass:
	case matGlass:
		pGibName = "models/glassgibs.mdl";
		
		PRECACHE_SOUND("debris/bustglass1.wav");
		PRECACHE_SOUND("debris/bustglass2.wav");
		break;
	case matMetal:
		pGibName = "models/metalplategibs.mdl";
		
		PRECACHE_SOUND("debris/bustmetal1.wav");
		PRECACHE_SOUND("debris/bustmetal2.wav");
		break;
	case matCinderBlock:
		pGibName = "models/cindergibs.mdl";
		
		PRECACHE_SOUND("debris/bustconcrete1.wav");
		PRECACHE_SOUND("debris/bustconcrete2.wav");
		break;
	case matRocks:
		pGibName = "models/rockgibs.mdl";
		
		PRECACHE_SOUND("debris/bustconcrete1.wav");
		PRECACHE_SOUND("debris/bustconcrete2.wav");
		break;
	case matCeilingTile:
		pGibName = "models/ceilinggibs.mdl";
		
		PRECACHE_SOUND ("debris/bustceiling.wav");  
		break;
	}
	MaterialSoundPrecache( m_Material );
	if ( m_iszGibModel )
		pGibName = STRING(m_iszGibModel);

	m_idShard = PRECACHE_MODEL( (char *)pGibName );

	// Precache the spawn item's data
	if ( m_iszSpawnObject )
		UTIL_PrecacheOther( (char *)STRING( m_iszSpawnObject ) );
}

// play shard sound when func_breakable takes damage.
// the more damage, the louder the shard sound.


void CBreakable::DamageSound( void )
{
	int pitch;
	float fvol;
	char *rgpsz[6];
	int i;
	int material = m_Material;

	if (RANDOM_LONG(0,2))
		pitch = PITCH_NORM;
	else
		pitch = 95 + RANDOM_LONG(0,34);

	fvol = RANDOM_FLOAT(0.75, 1.0);

	if (material == matComputer && RANDOM_LONG(0,1))
		material = matMetal;

	switch (material)
	{
	case matComputer:
	case matGlass:
	case matUnbreakableGlass:
		rgpsz[0] = "debris/glass1.wav";
		rgpsz[1] = "debris/glass2.wav";
		rgpsz[2] = "debris/glass3.wav";
		i = 3;
		break;

	case matWood:
		rgpsz[0] = "debris/wood1.wav";
		rgpsz[1] = "debris/wood2.wav";
		rgpsz[2] = "debris/wood3.wav";
		i = 3;
		break;

	case matMetal:
		rgpsz[0] = "debris/metal1.wav";
		rgpsz[1] = "debris/metal3.wav";
		rgpsz[2] = "debris/metal2.wav";
		i = 2;
		break;

	case matFlesh:
		rgpsz[0] = "debris/flesh1.wav";
		rgpsz[1] = "debris/flesh2.wav";
		rgpsz[2] = "debris/flesh3.wav";
		rgpsz[3] = "debris/flesh5.wav";
		rgpsz[4] = "debris/flesh6.wav";
		rgpsz[5] = "debris/flesh7.wav";
		i = 6;
		break;

	case matRocks:
	case matCinderBlock:
		rgpsz[0] = "debris/concrete1.wav";
		rgpsz[1] = "debris/concrete2.wav";
		rgpsz[2] = "debris/concrete3.wav";
		i = 3;
		break;

	case matNone:
	case matCeilingTile:
		// UNDONE: no ceiling tile shard sound yet
		i = 0;
		break;
	}

	if (i)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, rgpsz[RANDOM_LONG(0,i-1)], fvol, ATTN_NORM, 0, pitch);
}

void CBreakable::BreakTouch( CBaseEntity *pOther )
{
	float flDamage;
	entvars_t*	pevToucher = pOther->pev;
	
	// only players can break these right now
	if ( !pOther->IsPlayer() || !IsBreakable() )
	{
		return;
	}

	if ( FBitSet ( pev->spawnflags, SF_BREAK_TOUCH ) )
	{// can be broken when run into 
		flDamage = pevToucher->velocity.Length() * 0.01;

		if (flDamage >= pev->health)
		{
			SetTouch( NULL );
			TakeDamage(pevToucher, pevToucher, flDamage, DMG_CRUSH);

			// do a little damage to player if we broke glass or computer
			pOther->TakeDamage( pev, pev, flDamage/4, DMG_SLASH );
		}
	}

	if ( FBitSet ( pev->spawnflags, SF_BREAK_PRESSURE ) && pevToucher->absmin.z >= pev->maxs.z - 2 )
	{// can be broken when stood upon
		
		// play creaking sound here.
		DamageSound();

		SetThink( &CBreakable::Die );
		SetTouch( NULL );
		
		if ( m_flDelay == 0 )
		{// !!!BUGBUG - why doesn't zero delay work?
			m_flDelay = 0.1;
		}
		SetNextThink( m_flDelay );
	}
}


//
// Smash the our breakable object
//

// Break when triggered
void CBreakable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( IsBreakable() )
	{
		Vector angles = GetLocalAngles();
		angles.y = m_angle;
		SetLocalAngles( angles );
		UTIL_MakeVectors( GetLocalAngles( ));
		g_vecAttackDir = gpGlobals->v_forward;

		Die();
	}
}


void CBreakable::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	// random spark if this is a 'computer' object
	if (RANDOM_LONG(0,1) )
	{
		switch( m_Material )
		{
			case matComputer:
			{
				UTIL_Sparks( ptr->vecEndPos );

				float flVolume = RANDOM_FLOAT ( 0.7 , 1.0 );//random volume range
				switch ( RANDOM_LONG(0,1) )
				{
					case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark5.wav", flVolume, ATTN_NORM);	break;
					case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark6.wav", flVolume, ATTN_NORM);	break;
				}
			}
			break;
			
			case matUnbreakableGlass:
				UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
			break;
		}
	}

	CBaseDelay::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

//=========================================================
// Special takedamage for func_breakable. Allows us to make
// exceptions that are breakable-specific
// bitsDamageType indicates the type of damage sustained ie: DMG_CRUSH
//=========================================================
int CBreakable :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector	vecTemp;

	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if ( pevAttacker == pevInflictor )	
	{
		vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );
		
		// if a client hit the breakable with a crowbar, and breakable is crowbar-sensitive, break it now.
		if ( FBitSet ( pevAttacker->flags, FL_CLIENT ) &&
				 FBitSet ( pev->spawnflags, SF_BREAK_CROWBAR ) && (bitsDamageType & DMG_CLUB))
			flDamage = pev->health;
	}
	else
	// an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );
	}
	
	if (!IsBreakable())
		return 0;

	// Breakables take double damage from the crowbar
	if ( bitsDamageType & DMG_CLUB )
		flDamage *= 2;

	// Boxes / glass / etc. don't take much poison damage, just the impact of the dart - consider that 10%
	if ( bitsDamageType & DMG_POISON )
		flDamage *= 0.1;

// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();
		
// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		Die();
		return 0;
	}

	// Make a shard noise each time func breakable is hit.
	// Don't play shard noise if cbreakable actually died.

	DamageSound();

	return 1;
}


void CBreakable::Die( void )
{
	Vector vecSpot;// shard origin
	Vector vecVelocity;// shard velocity
	CBaseEntity *pEntity = NULL;
	char cFlag = 0;
	int pitch;
	float fvol;

	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	
	pitch = 95 + RANDOM_LONG(0,29);

	if (pitch > 97 && pitch < 103)
		pitch = 100;

	// The more negative pev->health, the louder
	// the sound should be.

	fvol = RANDOM_FLOAT(0.85, 1.0) + (abs(pev->health) / 100.0);

	if (fvol > 1.0)
		fvol = 1.0;


	switch (m_Material)
	{
	case matGlass:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustglass1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustglass2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_GLASS;
		break;

	case matWood:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustcrate1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustcrate2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_WOOD;
		break;

	case matComputer:
	case matMetal:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustmetal1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustmetal2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_METAL;
		break;

	case matFlesh:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustflesh1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustflesh2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_FLESH;
		break;

	case matRocks:
	case matCinderBlock:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustconcrete1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustconcrete2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_CONCRETE;
		break;

	case matCeilingTile:
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "debris/bustceiling.wav", fvol, ATTN_NORM, 0, pitch);
		break;
	}

	// tell owner ( if any ) that we're dead.This is mostly for PushableMaker functionality.
	CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
	if ( pOwner )
	{
		pOwner->DeathNotice( pev );
	}    
		
	if( m_Explosion == expDirected )
	{
		vecVelocity = g_vecAttackDir * 200;
	}
	else
	{
		vecVelocity.x = 0;
		vecVelocity.y = 0;
		vecVelocity.z = 0;
	}

	vecSpot = GetAbsOrigin() + (pev->mins + pev->maxs) * 0.5f;
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE( TE_BREAKMODEL);

		// position
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );

		// size
		WRITE_COORD( pev->size.x);
		WRITE_COORD( pev->size.y);
		WRITE_COORD( pev->size.z);

		// velocity
		WRITE_COORD( vecVelocity.x ); 
		WRITE_COORD( vecVelocity.y );
		WRITE_COORD( vecVelocity.z );

		// randomization
		WRITE_BYTE( 10 ); 

		// Model
		WRITE_SHORT( m_idShard );	//model id#

		// # of shards
		WRITE_BYTE( 0 );	// let client decide

		// duration
		WRITE_BYTE( 25 );// 2.5 seconds

		// flags
		WRITE_BYTE( cFlag );
	MESSAGE_END();

	float size = pev->size.x;
	if ( size < pev->size.y )
		size = pev->size.y;
	if ( size < pev->size.z )
		size = pev->size.z;

	// !!! HACK  This should work!
	// Build a box above the entity that looks like an 8 pixel high sheet
	Vector mins = pev->absmin;
	Vector maxs = pev->absmax;
	mins.z = pev->absmax.z;
	maxs.z += 8;

	// BUGBUG -- can only find 256 entities on a breakable -- should be enough
	CBaseEntity *pList[256];
	int count = UTIL_EntitiesInBox( pList, 256, mins, maxs, FL_ONGROUND );
	if ( count )
	{
		for ( int i = 0; i < count; i++ )
		{
			ClearBits( pList[i]->pev->flags, FL_ONGROUND );
			pList[i]->pev->groundentity = NULL;
		}
	}

	// Don't fire something that could fire myself
	pev->targetname = 0;

	pev->solid = SOLID_NOT;
	// Fire targets on break
	SUB_UseTargets( NULL, USE_TOGGLE, 0 );

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( 0.1 );

	if ( m_iszSpawnObject )
		CBaseEntity::Create( (char *)STRING(m_iszSpawnObject), VecBModelOrigin(pev), GetAbsAngles(), edict() );


	if ( Explodable() )
	{
		ExplosionCreate( Center(), GetAbsAngles(), edict(), ExplosionMagnitude(), TRUE );
	}
}

BOOL CBreakable :: IsBreakable( void ) 
{ 
	return m_Material != matUnbreakableGlass;
}

int CBreakable :: DamageDecal( int bitsDamageType )
{
	if ( m_Material == matGlass  )
		return DECAL_GLASSBREAK1 + RANDOM_LONG(0,2);

	if ( m_Material == matUnbreakableGlass )
		return DECAL_BPROOF1;

	return CBaseEntity::DamageDecal( bitsDamageType );
}


class CPushable : public CBreakable
{
	DECLARE_CLASS( CPushable, CBreakable );
public:
	void	Spawn ( void );
	void	Precache( void );
	void	Touch ( CBaseEntity *pOther );
	void	Move( CBaseEntity *pMover, int push );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	OnClearParent( void ) { ClearGroundEntity (); }

	virtual int ObjectCaps( void )
	{
		int flags = (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION);

		if (FBitSet (pev->spawnflags, SF_PUSH_HOLDABLE))
			flags |= FCAP_HOLDABLE_ITEM;
		else
			flags |= FCAP_CONTINUOUS_USE;

		return flags;
	}

	inline float MaxSpeed( void ) { return m_maxSpeed; }
	
	// breakables use an overridden takedamage
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	virtual BOOL IsPushable( void ) { return TRUE; }
	
	DECLARE_DATADESC();

	static char *m_soundNames[3];
	int	m_lastSound;	// no need to save/restore, just keeps the same sound from playing twice in a row
	float	m_maxSpeed;
	float	m_soundTime;
};

BEGIN_DATADESC( CPushable )
	DEFINE_FIELD( m_maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_soundTime, FIELD_TIME ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_pushable, CPushable );

char *CPushable :: m_soundNames[3] = { "debris/pushbox1.wav", "debris/pushbox2.wav", "debris/pushbox3.wav" };

void CPushable :: Spawn( void )
{
	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		BaseClass::Spawn();
	else
		Precache( );

	pev->movetype = MOVETYPE_PUSHSTEP;

	if( FBitSet( pev->spawnflags, SF_PUSH_BSPCOLLISION ))
		pev->solid = SOLID_BSP;
	else 
		pev->solid = SOLID_BBOX;

	SET_MODEL( edict(), STRING(pev->model) );

	if ( pev->friction > 399 )
		pev->friction = 399;

	m_maxSpeed = 400 - pev->friction;
	SetBits( pev->flags, FL_FLOAT );
	pev->friction = 0;

	Vector origin = GetLocalOrigin();
	origin.z += 1; // Pick up off of the floor
	UTIL_SetOrigin( this, origin );

	// Multiply by area of the box's cross-section (assume 1000 units^3 standard volume)
	pev->skin = ( pev->skin * (pev->maxs.x - pev->mins.x) * (pev->maxs.y - pev->mins.y) ) * 0.0005;
	m_soundTime = 0;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
}


void CPushable :: Precache( void )
{
	for ( int i = 0; i < 3; i++ )
		PRECACHE_SOUND( m_soundNames[i] );

	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		BaseClass::Precache( );
}


void CPushable :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "size") )
	{
		int bbox = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;

		switch( bbox )
		{
		case 0:	// Point
			UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
			break;

		case 2: // Big Hull!?!?	!!!BUGBUG Figure out what this hull really is
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN*2, VEC_DUCK_HULL_MAX*2);
			break;

		case 3: // Player duck
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
			break;

		default:
		case 1: // Player
			UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
			break;
		}

	}
	else if ( FStrEq(pkvd->szKeyName, "buoyancy") )
	{
		pev->skin = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}


// Pull the func_pushable
void CPushable :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
	{
		if ( pev->spawnflags & SF_PUSH_BREAKABLE )
			this->CBreakable::Use( pActivator, pCaller, useType, value );
		return;
	}

	if ( pActivator->GetAbsVelocity() != g_vecZero )
		Move( pActivator, 0 );
}


void CPushable :: Touch( CBaseEntity *pOther )
{
	if( pOther == g_pWorld )
		return;

	Move( pOther, 1 );
}


void CPushable :: Move( CBaseEntity *pOther, int push )
{
	entvars_t*	pevToucher = pOther->pev;
	int playerTouch = 0;

	// Now crossbow bolts, grenades roaches and other stuff with null size can't moving pushables anyway
	if( pOther->IsPointSized( ))
		return;

	// Is entity standing on this pushable ?
	if( FBitSet( pevToucher->flags, FL_ONGROUND ) && pOther->GetGroundEntity() == this )
	{
		// Only push if floating
		if( pev->waterlevel > 0 )
		{
			Vector vecVelocity = GetAbsVelocity();
			vecVelocity.z += pOther->GetAbsVelocity().z * 0.1;
			SetAbsVelocity( vecVelocity );
		}
		return;
	}

	// g-cont. fix pushable acceleration bug
	if ( pOther->IsPlayer() )
	{
		// Don't push unless the player is pushing forward and NOT use (pull)
		if ( push && !(pevToucher->button & (IN_FORWARD|IN_MOVERIGHT|IN_MOVELEFT|IN_BACK)) )
			return;
		if ( !push && !(pevToucher->button & (IN_BACK)) ) return;
			playerTouch = 1;
	}

	TraceResult tr = UTIL_GetGlobalTrace();

	float factor = DotProduct( pevToucher->velocity.Normalize(), tr.vecPlaneNormal );

	if( push && factor >= -0.3f )
		return;	// just tocuhing not pushed

	if ( playerTouch )
	{
		if ( !(pevToucher->flags & FL_ONGROUND) )	// Don't push away from jumping/falling players unless in water
		{
			if ( pev->waterlevel < 1 )
				return;
			else 
				factor = 0.1;
		}
		else
			factor = 1;
	}
	else 
		factor = 0.25;

	Vector vecAbsVelocity = GetAbsVelocity();

	vecAbsVelocity.x += pevToucher->velocity.x * factor;
	vecAbsVelocity.y += pevToucher->velocity.y * factor;

	float length = sqrt( vecAbsVelocity.x * vecAbsVelocity.x + vecAbsVelocity.y * vecAbsVelocity.y );
	if ( push && (length > MaxSpeed()) )
	{
		vecAbsVelocity.x = (vecAbsVelocity.x * MaxSpeed() / length );
		vecAbsVelocity.y = (vecAbsVelocity.y * MaxSpeed() / length );
	}

	SetAbsVelocity( vecAbsVelocity );

	if ( playerTouch )
	{
		Vector vecToucherVelocity = pOther->GetAbsVelocity();
		vecToucherVelocity.x = vecAbsVelocity.x;
		vecToucherVelocity.y = vecAbsVelocity.y;
		pOther->SetAbsVelocity( vecToucherVelocity );

		if(( gpGlobals->time - m_soundTime ) > 0.7f )
		{
			m_soundTime = gpGlobals->time;
			if( length > 0 && FBitSet( pev->flags, FL_ONGROUND ))
			{
				m_lastSound = RANDOM_LONG(0,2);
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound], 0.5, ATTN_NORM);
			}
			else
				STOP_SOUND( ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound] );
		}
	}
}

int CPushable::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		return CBreakable::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	return 1;
}

// pushablemaker spawnflags
#define SF_PUSHABLEMAKER_START_ON	1 // start active ( if has targetname )
#define SF_PUSHABLEMAKER_CYCLIC	4 // drop one monster every time fired.

class CPushableMaker : public CPushable
{
	DECLARE_CLASS( CPushableMaker, CPushable );
public:
	void Spawn( void );
	void KeyValue( KeyValueData* pkvd);
	void ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void CyclicUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MakerThink( void );
	void DeathNotice ( entvars_t *pevChild );// monster maker children use this to tell the monster maker that they have died.
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	void MakePushable( void );

	DECLARE_DATADESC();

	int m_cNumBoxes;	// max number of monsters this ent can create

	int m_cLiveBoxes;	// how many monsters made by this monster maker that are currently alive
	int m_iMaxLiveBoxes;// max number of pushables that this maker may have out at one time.

	float m_flGround;	// z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child
};

LINK_ENTITY_TO_CLASS( pushablemaker, CPushableMaker );

BEGIN_DATADESC( CPushableMaker )
	DEFINE_KEYFIELD( m_cNumBoxes, FIELD_INTEGER, "boxcount" ),
	DEFINE_FIELD( m_cLiveBoxes, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iMaxLiveBoxes, FIELD_INTEGER, "m_imaxliveboxes" ),
	DEFINE_FIELD( m_flGround, FIELD_FLOAT ),
	DEFINE_FUNCTION( ToggleUse ),
	DEFINE_FUNCTION( CyclicUse ),
	DEFINE_FUNCTION( MakerThink ),
END_DATADESC()

void CPushableMaker :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "boxcount") )
	{
		m_cNumBoxes = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "m_imaxliveboxes") )
	{
		m_iMaxLiveBoxes = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPushable::KeyValue( pkvd );
}

void CPushableMaker :: Spawn( void )
{
	CPushable::Spawn();

	// make dormant
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetBits( pev->effects, EF_NODRAW );
	pev->takedamage = DAMAGE_NO;
	RelinkEntity();

	m_cLiveBoxes = 0;

	if ( !FStringNull ( pev->targetname ))
	{
		if( pev->spawnflags & SF_PUSHABLEMAKER_CYCLIC )
		{
			SetUse( &CPushableMaker::CyclicUse );	// drop one monster each time we fire
		}
		else
		{
			SetUse( &CPushableMaker::ToggleUse );	// so can be turned on/off
		}

		if( FBitSet ( pev->spawnflags, SF_PUSHABLEMAKER_START_ON ) )
		{
			// start making monsters as soon as monstermaker spawns
			m_iState = STATE_ON;
			SetThink( &CPushableMaker::MakerThink );
		}
		else
		{
			// wait to be activated.
			m_iState = STATE_OFF;
			SetThink( &CBaseEntity::SUB_DoNothing );
		}
	}
	else
	{
			// no targetname, just start.
			SetNextThink( m_flDelay );
			m_iState = STATE_ON;
			SetThink( &CPushableMaker::MakerThink );
	}

	m_flGround = 0;
}

//=========================================================
// CPushableMaker - this is the code that drops the box
//=========================================================
void CPushableMaker :: MakePushable( void )
{
	if ( m_iMaxLiveBoxes > 0 && m_cLiveBoxes >= m_iMaxLiveBoxes )
	{
		// not allowed to make a new one yet. Too many live ones out right now.
		return;
	}

	if ( !m_flGround || m_hParent != NULL )
	{
		// set altitude. Now that I'm activated, any breakables, etc should be out from under me. 
		TraceResult tr;

		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector ( 0, 0, 2048 ), ignore_monsters, edict(), &tr );
		m_flGround = tr.vecEndPos.z;
	}

	Vector mins = pev->absmin;
	Vector maxs = pev->absmax;
	maxs.z = GetAbsOrigin().z;
	mins.z = m_flGround;

	CBaseEntity *pList[2];
	int count = UTIL_EntitiesInBox( pList, 2, mins, maxs, 0 );
	if ( count )
	{
		for( int i = 0; i < count; i++ )
		{
			if( pList[i] == this )
				continue;

			if( pList[i]->IsPushable() || pList[i]->IsMonster() || pList[i]->IsPlayer())
			{
				return; // don't build a stack of boxes!
			}
		}
	}

	TraceResult trace;

	UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), &trace );

	if( trace.fStartSolid )
	{
		ALERT( at_aiconsole, "pushablemaker: couldn't spawn %s->%s\n", GetTargetname(), GetNetname());
		return;
	}
	
	CPushable *pBox = (CPushable *)CreateEntityByName( "func_pushable" );
	if( !pBox ) return;	// g-cont. ???
	
	// If I have a target, fire!
	if( !FStringNull( pev->target ))
	{
		// delay already overloaded for this entity, so can't call SUB_UseTargets()
		UTIL_FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
	}

	// set parms for our box
	pBox->pev->model = pev->model;
	pBox->pev->spawnflags = pev->spawnflags & ~(SF_PUSHABLEMAKER_START_ON|SF_PUSHABLEMAKER_CYCLIC);
	pBox->pev->renderfx = pev->renderfx;
	pBox->pev->rendermode = pev->rendermode;
	pBox->pev->renderamt = pev->renderamt;
	pBox->pev->rendercolor.x = pev->rendercolor.x;
	pBox->pev->rendercolor.y = pev->rendercolor.y;
	pBox->pev->rendercolor.z = pev->rendercolor.z;
	pBox->pev->friction = pev->friction;
	pBox->pev->effects = pev->effects & ~EF_NODRAW;
	pBox->pev->health = pev->health;
	pBox->pev->skin = pev->skin;
	pBox->m_Explosion = m_Explosion;
	pBox->m_Material = m_Material;
	pBox->m_iszGibModel = m_iszGibModel;
	pBox->m_iszSpawnObject = m_iszSpawnObject;
	pBox->ExplosionSetMagnitude( ExplosionMagnitude() );
	pBox->m_flDelay = m_flDelay;	// BUGBUG: delay already used. Try m_flWait instead?

	pBox->SetAbsAngles( GetAbsAngles() );
	pBox->SetAbsOrigin( GetAbsOrigin() );
	pBox->m_fSetAngles = TRUE;

	DispatchSpawn( pBox->edict() );
	pBox->pev->owner = edict();

	if ( !FStringNull( pev->netname ) )
	{
		// if I have a netname (overloaded), give the child monster that name as a targetname
		pBox->pev->targetname = pev->netname;
	}

	m_cLiveBoxes++;// count this box
	m_cNumBoxes--;

	if ( m_cNumBoxes == 0 )
	{
		// Disable this forever.  Don't kill it because it still gets death notices
		SetThink( NULL );
		SetUse( NULL );
	}
}

//=========================================================
// CyclicUse - drops one monster from the monstermaker
// each time we call this.
//=========================================================
void CPushableMaker::CyclicUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	MakePushable();
}

//=========================================================
// ToggleUse - activates/deactivates the monster maker
//=========================================================
void CPushableMaker :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType ))
		return;

	if ( GetState() == STATE_ON )
	{
		m_iState = STATE_OFF;
		SetThink( NULL );
	}
	else
	{
		m_iState = STATE_ON;
		SetThink( &CPushableMaker::MakerThink );
	}

	SetNextThink( 0 );
}

void CPushableMaker :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// HACKHACK: we can't using callbacks because CBreakable override use function
	if( m_pfnUse ) (this->*m_pfnUse)( pActivator, pCaller, useType, value );
}

//=========================================================
// MakerThink - creates a new monster every so often
//=========================================================
void CPushableMaker :: MakerThink ( void )
{
	SetNextThink( m_flDelay );
	MakePushable();
}

//=========================================================
//=========================================================
void CPushableMaker :: DeathNotice ( entvars_t *pevChild )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveBoxes--;
	pevChild->owner = NULL;
}
