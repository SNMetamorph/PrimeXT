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

#include "spark_shower.h"

LINK_ENTITY_TO_CLASS( spark_shower, CShower );

void CShower::Spawn( void )
{
	Vector velocity = RANDOM_FLOAT( 200, 300 ) * GetAbsAngles();
	velocity.x += RANDOM_FLOAT(-100.f,100.f);
	velocity.y += RANDOM_FLOAT(-100.f,100.f);

	if ( velocity.z >= 0 )
		velocity.z += 200;
	else
		velocity.z -= 200;

	SetAbsVelocity( velocity );

	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.5;
	pev->nextthink = gpGlobals->time + 0.1;
	pev->solid = SOLID_NOT;
	SET_MODEL( edict(), "models/grenade.mdl");	// Need a model, just use the grenade, we don't draw it anyway
	UTIL_SetSize(pev, g_vecZero, g_vecZero );
	pev->effects |= EF_NODRAW;
	pev->speed = RANDOM_FLOAT( 0.5, 1.5 );

	SetAbsAngles( g_vecZero );
}

void CShower::Think( void )
{
	UTIL_Sparks( GetAbsOrigin() );

	pev->speed -= 0.1;
	if ( pev->speed > 0 )
		SetNextThink( 0.1 );
	else
		UTIL_Remove( this );
	pev->flags &= ~FL_ONGROUND;
}

void CShower::Touch( CBaseEntity *pOther )
{
	if ( pev->flags & FL_ONGROUND )
		SetAbsVelocity( GetAbsVelocity() * 0.1 );
	else
		SetAbsVelocity( GetAbsVelocity() * 0.6 );

	if ( (GetAbsVelocity().x * GetAbsVelocity().x + GetAbsVelocity().y * GetAbsVelocity().y ) < 10.0 )
		pev->speed = 0;
}