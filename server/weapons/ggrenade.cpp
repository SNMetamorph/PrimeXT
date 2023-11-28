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

===== generic grenade.cpp ========================================================

*/

#include "ggrenade.h"
#include "soundent.h"

LINK_ENTITY_TO_CLASS( grenade, CGrenade );

BEGIN_DATADESC( CGrenade )
	DEFINE_FUNCTION( Smoke ),
	DEFINE_FUNCTION( BounceTouch ),
	DEFINE_FUNCTION( SlideTouch ),
	DEFINE_FUNCTION( ExplodeTouch ),
	DEFINE_FUNCTION( DangerSoundThink ),
	DEFINE_FUNCTION( PreDetonate ),
	DEFINE_FUNCTION( Detonate ),
	DEFINE_FUNCTION( DetonateUse ),
	DEFINE_FUNCTION( TumbleThink ),
END_DATADESC()

//
// Grenade Explode
//
void CGrenade::Explode( Vector vecSrc, Vector vecAim )
{
	TraceResult tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + Vector ( 0, 0, -32 ),  ignore_monsters, ENT(pev), & tr);

	Explode( &tr, DMG_BLAST );
}

// UNDONE: temporary scorching for PreAlpha - find a less sleazy permenant solution.
void CGrenade::Explode( TraceResult *pTrace, int bitsDamageType )
{
	float		flRndSound;// sound randomizer

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( pTrace->flFraction != 1.0 )
	{
		SetAbsOrigin( pTrace->vecEndPos + (pTrace->vecPlaneNormal * (pev->dmg - 24) * 0.6f ));
	}

	Vector absOrigin = GetAbsOrigin();

	int iContents = UTIL_PointContents ( absOrigin );
	
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, absOrigin );
		WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( absOrigin.x );	// Send to PAS because of the sound
		WRITE_COORD( absOrigin.y );
		WRITE_COORD( absOrigin.z );

		if( iContents != CONTENTS_WATER )
		{
			WRITE_SHORT( g_sModelIndexFireball );
		}
		else
		{
			WRITE_SHORT( g_sModelIndexWExplosion );
		}
		WRITE_BYTE( (pev->dmg - 50) * .60  ); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, absOrigin, NORMAL_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL; // can't traceline attack owner if this is set

	RadiusDamage ( pev, pevOwner, pev->dmg, CLASS_NONE, bitsDamageType );

	CBaseEntity *pEntity = CBaseEntity::Instance( pTrace->pHit );

	if (pEntity && GET_MODEL_PTR(pEntity->edict())) {
		UTIL_StudioDecalTrace(pTrace, "scorch");
	}
	else {
		UTIL_TraceCustomDecal(pTrace, "scorch", RANDOM_FLOAT(0.0f, 360.0f));
	}

	flRndSound = RANDOM_FLOAT( 0 , 1 );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris1.wav", 0.55, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris2.wav", 0.55, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/debris3.wav", 0.55, ATTN_NORM);	break;
	}

	pev->effects |= EF_NODRAW;
	SetThink( &CGrenade::Smoke );
	SetAbsVelocity( g_vecZero );
	pev->nextthink = gpGlobals->time + 0.3;

	if (iContents != CONTENTS_WATER)
	{
		int sparkCount = RANDOM_LONG(0,3);
		for ( int i = 0; i < sparkCount; i++ )
			Create( "spark_shower", absOrigin, pTrace->vecPlaneNormal, NULL );
	}
}


void CGrenade :: Smoke( void )
{
	Vector absOrigin = GetAbsOrigin();

	if( UTIL_PointContents( absOrigin ) == CONTENTS_WATER )
	{
		UTIL_Bubbles( absOrigin - Vector( 64, 64, 64 ), absOrigin + Vector( 64, 64, 64 ), 100 );
	}
	else
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, absOrigin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( absOrigin.x );
			WRITE_COORD( absOrigin.y );
			WRITE_COORD( absOrigin.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( (pev->dmg - 50) * 0.80 ); // scale * 10
			WRITE_BYTE( 12  ); // framerate
		MESSAGE_END();
	}

	UTIL_Remove( this );
}

void CGrenade::Killed( entvars_t *pevAttacker, int iGib )
{
	Detonate( );
}


// Timed grenade, this think is called when time runs out.
void CGrenade::DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CGrenade::Detonate );
	pev->nextthink = gpGlobals->time;
}

void CGrenade::PreDetonate( void )
{
	CSoundEnt::InsertSound ( bits_SOUND_DANGER, GetAbsOrigin(), 400, 0.3 );

	SetThink( &CGrenade::Detonate );
	pev->nextthink = gpGlobals->time + 1;
}


void CGrenade::Detonate( void )
{
	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	vecSpot = GetAbsOrigin() + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -40 ),  ignore_monsters, ENT(pev), & tr);

	Explode( &tr, DMG_BLAST );
}


//
// Contact grenade, explode when it touches something
// 
void CGrenade::ExplodeTouch( CBaseEntity *pOther )
{
	TraceResult tr;
	Vector		vecSpot;// trace starts here!

	pev->enemy = pOther->edict();

	vecSpot = GetAbsOrigin() - GetAbsVelocity().Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + GetAbsVelocity().Normalize() * 64, ignore_monsters, edict(), &tr );

	Explode( &tr, DMG_BLAST );
}


void CGrenade::DangerSoundThink( void )
{
	if( !IsInWorld( ))
	{
		UTIL_Remove( this );
		return;
	}

	CSoundEnt::InsertSound( bits_SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5, GetAbsVelocity().Length( ), 0.2 );
	pev->nextthink = gpGlobals->time + 0.2;

	if( pev->waterlevel != 0 )
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity *= 0.5f;
		SetAbsVelocity( vecVelocity );
	}
}


void CGrenade::BounceTouch( CBaseEntity *pOther )
{
	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;

	// only do damage if we're moving fairly fast
	if( m_flNextAttack < gpGlobals->time && GetAbsVelocity().Length() > 100 )
	{
		entvars_t *pevOwner = VARS( pev->owner );

		if( pevOwner )
		{
			TraceResult tr = UTIL_GetGlobalTrace( );
			ClearMultiDamage( );
			pOther->TraceAttack( pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB ); 
			ApplyMultiDamage( pev, pevOwner );
		}

		m_flNextAttack = gpGlobals->time + 1.0; // debounce
	}

	Vector vecTestVelocity;

	// this is my heuristic for modulating the grenade velocity because grenades dropped purely vertical
	// or thrown very far tend to slow down too quickly for me to always catch just by testing velocity. 
	// trimming the Z velocity a bit seems to help quite a bit.
	vecTestVelocity = GetAbsVelocity(); 
	vecTestVelocity.z *= 0.45;

	if( !m_fRegisteredSound && vecTestVelocity.Length() <= 60 )
	{
		//ALERT( at_console, "Grenade Registered!: %f\n", vecTestVelocity.Length() );

		// grenade is moving really slow. It's probably very close to where it will ultimately stop moving. 
		// go ahead and emit the danger sound.
		
		// register a radius louder than the explosion, so we make sure everyone gets out of the way
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, GetAbsOrigin(), pev->dmg / 0.4, 0.3 );
		m_fRegisteredSound = TRUE;
	}

	if( pev->flags & FL_ONGROUND )
	{
		// add a bit of static friction
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity *= 0.8f;
		SetAbsVelocity( vecVelocity );

		pev->sequence = RANDOM_LONG( 1, 1 );
	}
	else
	{
		// play bounce sound
		BounceSound();
	}

	pev->framerate = GetAbsVelocity().Length() / 200.0;

	if( pev->framerate > 1.0 )
		pev->framerate = 1;
	else if( pev->framerate < 0.5 )
		pev->framerate = 0;

}

void CGrenade::SlideTouch( CBaseEntity *pOther )
{
	// don't hit the guy that launched this grenade
	if( pOther->edict() == pev->owner )
		return;

	// SetLocalAvelocity( Vector( 300, 300, 300 ));

	if( pev->flags & FL_ONGROUND )
	{
		// add a bit of static friction
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity *= 0.95f;
		SetAbsVelocity( vecVelocity );

		if( vecVelocity.x != 0 || vecVelocity.y != 0 )
		{
			// maintain sliding sound
		}
	}
	else
	{
		BounceSound();
	}
}

void CGrenade :: BounceSound( void )
{
	switch( RANDOM_LONG( 0, 2 ))
	{
	case 0: EMIT_SOUND( edict(), CHAN_VOICE, "weapons/grenade_hit1.wav", 0.25, ATTN_NORM ); break;
	case 1: EMIT_SOUND( edict(), CHAN_VOICE, "weapons/grenade_hit2.wav", 0.25, ATTN_NORM ); break;
	case 2: EMIT_SOUND( edict(), CHAN_VOICE, "weapons/grenade_hit3.wav", 0.25, ATTN_NORM ); break;
	}
}

void CGrenade :: TumbleThink( void )
{
	if( !IsInWorld( ))
	{
		UTIL_Remove( this );
		return;
	}

	StudioFrameAdvance( );
	SetNextThink( 0.1 );

	if( pev->dmgtime - 1 < gpGlobals->time )
	{
		CSoundEnt::InsertSound( bits_SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * (pev->dmgtime - gpGlobals->time), 400, 0.1 );
	}

	if( pev->dmgtime <= gpGlobals->time )
	{
		SetThink( &CGrenade::Detonate );
	}

	if( pev->waterlevel != 0 )
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity *= 0.5f;
		SetAbsVelocity( vecVelocity );
		pev->framerate = 0.2;
	}
}


void CGrenade:: Spawn( void )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->classname = MAKE_STRING( "grenade" );
	
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/grenade.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	pev->dmg = 100;
	m_fRegisteredSound = FALSE;
}


CGrenade *CGrenade::ShootContact( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CGrenade *pGrenade = GetClassPtr( (CGrenade *)NULL );
	pGrenade->Spawn();
	// contact grenades arc lower
	pGrenade->pev->gravity = 0.5;// lower gravity since grenade is aerodynamic and engine doesn't know it.
	UTIL_SetOrigin( pGrenade, vecStart );
	pGrenade->SetLocalVelocity( vecVelocity );
	pGrenade->SetLocalAngles( UTIL_VecToAngles( pGrenade->GetLocalVelocity( )));
	pGrenade->pev->owner = ENT(pevOwner);
	
	// make monsters afaid of it while in the air
	pGrenade->SetThink( &CGrenade::DangerSoundThink );
	pGrenade->SetNextThink( 0 );
	
	// Tumble in air
	Vector avelocity( RANDOM_FLOAT ( 100, 500 ), 0, 0 );
	pGrenade->SetLocalAvelocity( avelocity );
	
	// Explode on contact
	pGrenade->SetTouch( &CGrenade::ExplodeTouch );

	pGrenade->pev->dmg = gSkillData.plrDmgM203Grenade;

	return pGrenade;
}


CGrenade * CGrenade:: ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time )
{
	CGrenade *pGrenade = GetClassPtr( (CGrenade *)NULL );
	pGrenade->Spawn();

	UTIL_SetOrigin( pGrenade, vecStart );
	pGrenade->SetAbsVelocity( vecVelocity );
	pGrenade->SetLocalAngles( UTIL_VecToAngles( pGrenade->GetAbsVelocity( )));
	pGrenade->pev->owner = ENT( pevOwner );
	
	pGrenade->SetTouch( &CGrenade::BounceTouch );	// Bounce if touched
	
	// Take one second off of the desired detonation time and set the think to PreDetonate. PreDetonate
	// will insert a DANGER sound into the world sound list and delay detonation for one second so that 
	// the grenade explodes after the exact amount of time specified in the call to ShootTimed(). 

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink( &CGrenade::TumbleThink );
	pGrenade->SetNextThink( 0.1 );

	if( time < 0.1 )
	{
		pGrenade->SetNextThink( 0 );
		pGrenade->SetLocalVelocity( g_vecZero );
	}
		
	pGrenade->pev->sequence = RANDOM_LONG( 3, 6 );
	pGrenade->pev->framerate = 1.0;

	pGrenade->pev->gravity = 0.5;
	pGrenade->pev->friction = 0.8;

	SET_MODEL( pGrenade->edict(), "models/w_grenade.mdl" );
	pGrenade->pev->dmg = 100;

	return pGrenade;
}

CGrenade * CGrenade :: ShootSatchelCharge( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CGrenade *pGrenade = GetClassPtr( (CGrenade *)NULL );
	pGrenade->pev->movetype = MOVETYPE_BOUNCE;
	pGrenade->pev->classname = MAKE_STRING( "grenade" );
	
	pGrenade->pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pGrenade->pev), "models/grenade.mdl");	// Change this to satchel charge model

	UTIL_SetSize(pGrenade->pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	pGrenade->pev->dmg = 200;
	UTIL_SetOrigin( pGrenade, vecStart );
	pGrenade->SetAbsVelocity( vecVelocity );
	pGrenade->SetAbsAngles( g_vecZero );
	pGrenade->pev->owner = ENT(pevOwner);
	
	// Detonate in "time" seconds
	pGrenade->SetThink( &CBaseEntity::SUB_DoNothing );
	pGrenade->SetUse( &CGrenade::DetonateUse );
	pGrenade->SetTouch( &CGrenade::SlideTouch );
	pGrenade->pev->spawnflags = SF_DETONATE;

	pGrenade->pev->friction = 0.9;

	return pGrenade;
}

void CGrenade :: UseSatchelCharges( entvars_t *pevOwner, SATCHELCODE code )
{
	edict_t *pentFind;
	edict_t *pentOwner;

	if ( !pevOwner )
		return;

	CBaseEntity	*pOwner = CBaseEntity::Instance( pevOwner );

	pentOwner = pOwner->edict();

	pentFind = FIND_ENTITY_BY_CLASSNAME( NULL, "grenade" );
	while ( !FNullEnt( pentFind ) )
	{
		CBaseEntity *pEnt = Instance( pentFind );
		if ( pEnt )
		{
			if ( FBitSet( pEnt->pev->spawnflags, SF_DETONATE ) && pEnt->pev->owner == pentOwner )
			{
				if ( code == SATCHEL_DETONATE )
					pEnt->Use( pOwner, pOwner, USE_ON, 0 );
				else	// SATCHEL_RELEASE
					pEnt->pev->owner = NULL;
			}
		}
		pentFind = FIND_ENTITY_BY_CLASSNAME( pentFind, "grenade" );
	}
}