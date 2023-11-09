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

#include "crossbow_bolt.h"

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
	SetParent( 0 );
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
