/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

#include "apache_rocket.h"

LINK_ENTITY_TO_CLASS( hvr_rocket, CApacheHVR );

BEGIN_DATADESC( CApacheHVR )
	DEFINE_FIELD( m_vecForward, FIELD_VECTOR ),
	DEFINE_FUNCTION( IgniteThink ),
	DEFINE_FUNCTION( AccelerateThink ),
END_DATADESC()

void CApacheHVR :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/HVR.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	RelinkEntity( TRUE );

	SetThink( &CApacheHVR::IgniteThink );
	SetTouch( &CApacheHVR::ExplodeTouch );

	UTIL_MakeVectors( GetAbsAngles() );
	m_vecForward = gpGlobals->v_forward;
	pev->gravity = 0.5;

	pev->nextthink = gpGlobals->time + 0.1;

	pev->dmg = 150;
}


void CApacheHVR :: Precache( void )
{
	PRECACHE_MODEL("models/HVR.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND ("weapons/rocket1.wav");
}

void CApacheHVR :: IgniteThink( void  )
{
	// pev->movetype = MOVETYPE_TOSS;

	// pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 1, 0.5 );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 15 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	// set to accelerate
	SetThink( &CApacheHVR::AccelerateThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CApacheHVR :: AccelerateThink( void  )
{
	// check world boundaries
	if ( !IsInWorld( FALSE ))
	{
		UTIL_Remove( this );
		return;
	}

	// accelerate
	float flSpeed = GetAbsVelocity().Length();
	if (flSpeed < 1800)
	{
		SetAbsVelocity( GetAbsVelocity() + m_vecForward * 200 );
	}

	// re-aim
	SetAbsAngles( UTIL_VecToAngles( GetAbsVelocity() ));

	SetNextThink( 0.1 );
}
