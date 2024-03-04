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

#include "func_platrot.h"

LINK_ENTITY_TO_CLASS( func_platrot, CFuncPlatRot );

BEGIN_DATADESC( CFuncPlatRot )
	DEFINE_FIELD( m_end, FIELD_VECTOR ),
	DEFINE_FIELD( m_start, FIELD_VECTOR ),
END_DATADESC()

void CFuncPlatRot :: SetupRotation( void )
{
	if( m_vecFinalAngle.x != 0 ) // this plat rotates too!
	{
		CBaseToggle :: AxisDir( pev );
		m_start = GetLocalAngles();
		m_end = GetLocalAngles() + pev->movedir * m_vecFinalAngle.x;
	}
	else
	{
		m_start = g_vecZero;
		m_end = g_vecZero;
	}

	if( !FStringNull( pev->targetname )) // Start at top
	{
		SetLocalAngles( m_end );
	}
}

void CFuncPlatRot :: Spawn( void )
{
	BaseClass :: Spawn();
	SetupRotation();
}

void CFuncPlatRot :: GoDown( void )
{
	BaseClass :: GoDown();
	RotMove( m_start, GetMoveDoneTime() );
}

//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncPlatRot :: HitBottom( void )
{
	BaseClass :: HitBottom();
	SetLocalAvelocity( g_vecZero );
	SetLocalAngles( m_start );
}

//
// Platform is at bottom, now starts moving up
//
void CFuncPlatRot :: GoUp( void )
{
	BaseClass :: GoUp();
	RotMove( m_end, GetMoveDoneTime() );
}

//
// Platform has hit top.  Pauses, then starts back down again.
//
void CFuncPlatRot :: HitTop( void )
{
	BaseClass :: HitTop();
	SetLocalAvelocity( g_vecZero );
	SetLocalAngles( m_end );
}

void CFuncPlatRot :: RotMove( Vector &destAngle, float time )
{
	// set destdelta to the vector needed to move
	Vector vecDestDelta = destAngle - GetLocalAngles();

	// Travel time is so short, we're practically there already;  so make it so.
	if( time >= 0.1f )
	{
		SetLocalAvelocity( vecDestDelta * ( 1.0 / time ));
	}
	else
	{
		SetLocalAvelocity( vecDestDelta );
		SetMoveDoneTime( 1 );
	}
}
