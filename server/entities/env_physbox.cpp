/*
env_physbox.cpp - simple entity with rigid body physics
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

#include "env_physbox.h"

extern DLL_GLOBAL Vector		g_vecAttackDir;

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

	SetLocalAngles( m_vecTempAngles );
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

	// call base class method also
	CBaseDelay::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
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

int CPhysEntity::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector	vecTemp;

	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if (pevAttacker == pevInflictor)	
	{
		vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );
		// if a client hit the breakable with a crowbar, and breakable is crowbar-sensitive, break it now.
		if (FBitSet(pevAttacker->flags, FL_CLIENT) &&
			FBitSet(pev->spawnflags, SF_PHYS_CROWBAR) &&
			(bitsDamageType & DMG_CLUB))
		{
			flDamage = pev->health;
		}
	}
	else
	{
		// an actual missile was involved.
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

const char* CPhysEntity :: DamageDecal( int bitsDamageType )
{
	switch (m_Material)
	{
		case matGlass:
		case matUnbreakableGlass:
			return "shot_glass";
		case matWood:
			return "shot_wood";
		case matMetal:
			return "shot_metal";
	}

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

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( 0.1 );
}

int CPhysEntity :: ObjectCaps( void )
{
	int flags = CBaseEntity :: ObjectCaps() | FCAP_HOLD_ANGLES;
	if (FBitSet (pev->spawnflags, SF_PHYS_HOLDABLE))
		flags |= FCAP_HOLDABLE_ITEM;
	if( UTIL_GetModelType( pev->modelindex ) == mod_brush )
		flags &= ~FCAP_ACROSS_TRANSITION;

	return flags;
}

BOOL CPhysEntity :: IsBreakable( void )
{
	if( FBitSet( pev->spawnflags, SF_PHYS_BREAKABLE ))
		return TRUE;
	return FALSE;
}
	
void CPhysEntity :: SetObjectCollisionBox( void )
{
	WorldPhysic->UpdateEntityAABB( this );
}
