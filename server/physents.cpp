/*
phys_base.cpp - a null physics engine implementation
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include	"extdll.h"
#include  "util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include  "bspfile.h"
#include  "triangleapi.h"
#include  "studio.h"
#include  "func_break.h"
#include  "decals.h"

extern DLL_GLOBAL Vector		g_vecAttackDir;

#define UNSTICK_VELOCITY	100.0f	// FIXME: temporary solution

#define SF_PHYS_BREAKABLE	128
#define SF_PHYS_CROWBAR	256	// instant break if hit with crowbar
#define SF_PHYS_HOLDABLE	512	// item can be picked up by player

////////////// SIMPLE PHYSIC ENTITY //////////////

class CPhysEntity : public CBaseDelay
{
	DECLARE_CLASS( CPhysEntity, CBaseDelay );
public:
	void	TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	int	TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void	Killed( entvars_t *pevAttacker, int iGib );
	void	KeyValue( KeyValueData* pkvd );
	void	Touch( CBaseEntity *pOther );
	void	AutoSetSize( void );
	void	Precache( void );
	void	Spawn( void );
	void	DamageSound( void );

	DECLARE_DATADESC();

	virtual int ObjectCaps( void )
	{
		int flags = CBaseEntity :: ObjectCaps();
		if (FBitSet (pev->spawnflags, SF_PHYS_HOLDABLE))
			flags |= FCAP_HOLDABLE_ITEM;
		if( UTIL_GetModelType( pev->modelindex ) == mod_brush )
			flags &= ~FCAP_ACROSS_TRANSITION;

		return flags;
	}

	virtual BOOL IsBreakable( void )
	{
		if( FBitSet( pev->spawnflags, SF_PHYS_BREAKABLE ))
			return TRUE;
		return FALSE;
	}
	
	void SetObjectCollisionBox( void )
	{
		WorldPhysic->UpdateEntityAABB( this );
	}

	int DamageDecal( int bitsDamageType );

	int	m_idShard;
	int	m_iszGibModel;
	Materials	m_Material;
};

LINK_ENTITY_TO_CLASS( func_physbox, CPhysEntity );
LINK_ENTITY_TO_CLASS( env_physbox, CPhysEntity );		// just an alias for VHE

BEGIN_DATADESC( CPhysEntity )
	DEFINE_FIELD( m_Material, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iszGibModel, FIELD_STRING, "gibmodel" ),
END_DATADESC()

void CPhysEntity :: Precache( void )
{
	PRECACHE_MODEL( GetModel());

	const char *pGibName = NULL;

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

	CBreakable::MaterialSoundPrecache( m_Material );

	if ( m_iszGibModel )
		pGibName = STRING(m_iszGibModel);

	if( pGibName != NULL )
		m_idShard = PRECACHE_MODEL( (char *)pGibName );
}

void CPhysEntity::KeyValue( KeyValueData* pkvd )
{
	if (FStrEq(pkvd->szKeyName, "material"))
	{
		int i = atoi( pkvd->szValue);

		// 0:glass, 1:metal, 2:flesh, 3:wood

		if ((i < 0) || (i >= matLastMaterial))
			m_Material = matWood;
		else
			m_Material = (Materials)i;

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "gibmodel") )
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CPhysEntity :: Spawn( void )
{
	Precache();

	pev->takedamage = DAMAGE_YES;

	if( WorldPhysic->Initialized( ))
	{
		pev->movetype = MOVETYPE_PHYSIC;
		pev->solid = SOLID_CUSTOM;
	}

	// HACK:  matGlass can receive decals, we need the client to know about this
	// so use class to store the material flag
	if( m_Material == matGlass )
	{
		pev->playerclass = 1;
	}

	SET_MODEL( edict(), GetModel( ));
	UTIL_SetOrigin( this, GetLocalOrigin() );

	// set size in case we need have something valid
	// here for studiomodels
	AutoSetSize ();

	// motor!
	m_pUserData = WorldPhysic->CreateBodyFromEntity( this );
}

// automatically set collision box
void CPhysEntity :: AutoSetSize( void )
{
	if( UTIL_GetModelType( pev->modelindex ) == mod_studio )
	{
		studiohdr_t *pstudiohdr;
		pstudiohdr = (studiohdr_t *)GET_MODEL_PTR( edict() );

		if( pstudiohdr == NULL )
		{
			ALERT( at_error, "%s: unable to fetch model pointer!\n", GetModel());
			return;
		}

		mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
		UTIL_SetSize( pev, pseqdesc[pev->sequence].bbmin, pseqdesc[pev->sequence].bbmax );
	}
}

/*
================
TraceAttack
================
*/
void CPhysEntity :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	Vector vecOrigin = ptr->vecEndPos - vecDir * 1;
	float factor = 0.0f;

	if( bitsDamageType & DMG_CLUB )
		factor = 80.0f;
	else if( bitsDamageType & DMG_BULLET )
		factor = 45.0f;
	else if( bitsDamageType & DMG_BLAST )
		factor = 1000.0f;

	if( factor > 0.0f )
		WorldPhysic->AddImpulse( this, vecDir, vecOrigin, factor );

	// random spark if this is a 'computer' object
	if (RANDOM_LONG(0,1) )
	{
		float flVolume = 0.0f;

		switch( m_Material )
		{
		case matComputer:
			UTIL_Sparks( ptr->vecEndPos );

			flVolume = RANDOM_FLOAT ( 0.7 , 1.0 );//random volume range
			switch ( RANDOM_LONG(0,1) )
			{
			case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark5.wav", flVolume, ATTN_NORM); break;
			case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark6.wav", flVolume, ATTN_NORM); break;
			}
			break;
		case matUnbreakableGlass:
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
			break;
		}
	}
}

void CPhysEntity :: Touch( CBaseEntity *pOther )
{
	if( pOther == g_pWorld )
		return;

	// is entity standing on this pushable ?
	if( FBitSet( pOther->pev->flags, FL_ONGROUND ) && pOther->GetGroundEntity() == this )
		return;

	Vector vecVelocity = g_vecZero;

	if( pOther->IsPlayer() )
	{
		if( !( pOther->pev->button & (IN_FORWARD|IN_MOVERIGHT|IN_MOVELEFT|IN_BACK)) )
			return;

		if( gpGlobals->trace_plane_normal.z > 0.7f )
			return;	// if we standing on

		// get the quad part of frametime to apply impulse
		vecVelocity = pOther->GetAbsVelocity() * (1.0f / gpGlobals->frametime) * 0.25f;
		if( vecVelocity.z < 0.0f || vecVelocity.z > 0.0f )
			vecVelocity.z = 0; // don't stress the solver

		if( FBitSet( pOther->pev->flags, FL_STUCKED ) && vecVelocity.Length() < UNSTICK_VELOCITY )
		{
			// NOTE: don't entity origin because it's center of mass!
			Vector vecDir = (Center() - pOther->GetAbsOrigin()).Normalize();
			vecVelocity = vecDir * UNSTICK_VELOCITY * (1.0f / gpGlobals->frametime) * 0.25f;
		}
	}

	if( vecVelocity == g_vecZero )
		return;

	WorldPhysic->SetLinearMomentum( this, vecVelocity );
}

int CPhysEntity :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector	vecTemp;

	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if ( pevAttacker == pevInflictor )	
	{
		vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );
		
		// if a client hit the breakable with a crowbar, and breakable is crowbar-sensitive, break it now.
		if ( FBitSet ( pevAttacker->flags, FL_CLIENT ) &&
				 FBitSet ( pev->spawnflags, SF_PHYS_CROWBAR ) && (bitsDamageType & DMG_CLUB))
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
		Killed( pevAttacker, GIB_ALWAYS );
		return 0;
	}

	// Make a shard noise each time func breakable is hit.
	// Don't play shard noise if cbreakable actually died.

	DamageSound();

	return 1;
}

void CPhysEntity :: DamageSound( void )
{
	int pitch;
	float fvol;
	char *rgpsz[6];
	int i;
	int material = m_Material;

//	if (RANDOM_LONG(0,1))
//		return;

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

	case matCeilingTile:
		// UNDONE: no ceiling tile shard sound yet
		i = 0;
		break;
	}

	if (i)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, rgpsz[RANDOM_LONG(0,i-1)], fvol, ATTN_NORM, 0, pitch);
}

int CPhysEntity :: DamageDecal( int bitsDamageType )
{
	if ( m_Material == matGlass  )
		return DECAL_GLASSBREAK1 + RANDOM_LONG(0,2);

	if ( m_Material == matUnbreakableGlass )
		return DECAL_BPROOF1;

	return CBaseEntity::DamageDecal( bitsDamageType );
}

void CPhysEntity :: Killed( entvars_t *pevAttacker, int iGib )
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
		WRITE_COORD( 0 ); 
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );

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

	// Don't fire something that could fire myself
	pev->targetname = 0;

	pev->solid = SOLID_NOT;
	// Fire targets on break
	SUB_UseTargets( NULL, USE_TOGGLE, 0 );

	SetThink( SUB_Remove );
	SetNextThink( 0.1 );
}

// pushablemaker spawnflags
#define SF_PHYSBOXMAKER_START_ON	1 // start active ( if has targetname )
#define SF_PHYSBOXMAKER_CYCLIC	4 // drop one monster every time fired.

class CPhysBoxMaker : public CPhysEntity
{
	DECLARE_CLASS( CPhysBoxMaker, CPhysEntity );
public:
	void Spawn( void );
	void KeyValue( KeyValueData* pkvd);
	void ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void CyclicUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MakerThink( void );
	void DeathNotice ( entvars_t *pevChild );// monster maker children use this to tell the monster maker that they have died.
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	void MakePhysBox( void );

	DECLARE_DATADESC();

	int m_cNumBoxes;	// max number of monsters this ent can create

	int m_cLiveBoxes;	// how many monsters made by this monster maker that are currently alive
	int m_iMaxLiveBoxes;// max number of pushables that this maker may have out at one time.

	float m_flGround;	// z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child
};

LINK_ENTITY_TO_CLASS( physboxmaker, CPhysBoxMaker );
LINK_ENTITY_TO_CLASS( physenvmaker, CPhysBoxMaker );	// just an alias

BEGIN_DATADESC( CPhysBoxMaker )
	DEFINE_KEYFIELD( m_cNumBoxes, FIELD_INTEGER, "boxcount" ),
	DEFINE_FIELD( m_cLiveBoxes, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iMaxLiveBoxes, FIELD_INTEGER, "m_imaxliveboxes" ),
	DEFINE_FIELD( m_flGround, FIELD_FLOAT ),
	DEFINE_FUNCTION( ToggleUse ),
	DEFINE_FUNCTION( CyclicUse ),
	DEFINE_FUNCTION( MakerThink ),
END_DATADESC()

void CPhysBoxMaker :: KeyValue( KeyValueData *pkvd )
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
		CPhysEntity::KeyValue( pkvd );
}

void CPhysBoxMaker :: Spawn( void )
{
	SET_MODEL( edict(), GetModel( ));
	UTIL_SetOrigin( this, GetLocalOrigin() );

	// make dormant
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetBits( pev->effects, EF_NODRAW );
	pev->takedamage = DAMAGE_NO;
	RelinkEntity();

	m_cLiveBoxes = 0;

	if ( !FStringNull ( pev->targetname ))
	{
		if( pev->spawnflags & SF_PHYSBOXMAKER_CYCLIC )
		{
			SetUse( CyclicUse );	// drop one monster each time we fire
		}
		else
		{
			SetUse( ToggleUse );	// so can be turned on/off
		}

		if( FBitSet ( pev->spawnflags, SF_PHYSBOXMAKER_START_ON ) )
		{
			// start making monsters as soon as monstermaker spawns
			m_iState = STATE_ON;
			SetThink( MakerThink );
		}
		else
		{
			// wait to be activated.
			m_iState = STATE_OFF;
			SetThink( SUB_DoNothing );
		}
	}
	else
	{
			// no targetname, just start.
			SetNextThink( m_flDelay );
			m_iState = STATE_ON;
			SetThink ( MakerThink );
	}

	m_flGround = 0;
}

//=========================================================
// CPhysBoxMaker - this is the code that drops the box
//=========================================================
void CPhysBoxMaker :: MakePhysBox( void )
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

			if( pList[i]->IsPushable() || pList[i]->IsMonster() || pList[i]->IsPlayer() || pList[i]->IsRigidBody( ))
			{
				return; // don't build a stack of boxes!
			}
		}
	}

	CPhysEntity *pBox = GetClassPtr( (CPhysEntity *)NULL );
	if( !pBox ) return;	// g-cont. ???
	
	// If I have a target, fire!
	if( !FStringNull( pev->target ))
	{
		// delay already overloaded for this entity, so can't call SUB_UseTargets()
		UTIL_FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
	}

	// set parms for our box
	pBox->pev->model = pev->model;
	pBox->pev->classname = MAKE_STRING( "func_physbox" );
	pBox->pev->spawnflags = pev->spawnflags & ~(SF_PHYSBOXMAKER_START_ON|SF_PHYSBOXMAKER_CYCLIC);
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
	pBox->m_Material = m_Material;
	pBox->m_iszGibModel = m_iszGibModel;
	pBox->m_flDelay = m_flDelay;	// BUGBUG: delay already used. Try m_flWait instead?

	pBox->SetAbsOrigin( GetAbsOrigin() );
	pBox->SetAbsAngles( GetAbsAngles() );
	pBox->m_fSetAngles = TRUE;
	DispatchSpawn( pBox->edict( ));
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
void CPhysBoxMaker::CyclicUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	MakePhysBox();
}

//=========================================================
// ToggleUse - activates/deactivates the monster maker
//=========================================================
void CPhysBoxMaker :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType ))
		return;

	if ( GetState() == STATE_ON )
	{
		m_iState = STATE_OFF;
		SetThink ( NULL );
	}
	else
	{
		m_iState = STATE_ON;
		SetThink( MakerThink );
	}

	SetNextThink( 0 );
}

void CPhysBoxMaker :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// HACKHACK: we can't using callbacks because CBreakable override use function
	if( m_pfnUse ) (this->*m_pfnUse)( pActivator, pCaller, useType, value );
}

//=========================================================
// MakerThink - creates a new monster every so often
//=========================================================
void CPhysBoxMaker :: MakerThink ( void )
{
	SetNextThink( m_flDelay );
	MakePhysBox();
}

//=========================================================
//=========================================================
void CPhysBoxMaker :: DeathNotice ( entvars_t *pevChild )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveBoxes--;
	pevChild->owner = NULL;
}
