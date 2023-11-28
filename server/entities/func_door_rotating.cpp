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

#include "func_door_rotating.h"	// for func_traindoor

LINK_ENTITY_TO_CLASS( func_door_rotating, CRotDoor );

void CRotDoor :: Spawn( void )
{
	// set the axis of rotation
	AxisDir( pev );

	if( FBitSet( pev->spawnflags, SF_DOOR_PASSABLE ))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;

	Precache();

	// check for clockwise rotation
	if( FBitSet( pev->spawnflags, SF_DOOR_ROTATE_BACKWARDS ))
		pev->movedir = pev->movedir * -1;
	
	m_vecAngle1 = GetLocalAngles();
	m_vecAngle2 = GetLocalAngles() + pev->movedir * m_flMoveDistance;

	ASSERTSZ( m_vecAngle1 != m_vecAngle2, "rotating door start/end positions are equal" );

	pev->movetype = MOVETYPE_PUSH;
	SET_MODEL( edict(), GetModel() );

	// NOTE: original Half-Life was contain a bug in AngularMove function
	// while m_flWait was equal 0 then object has stopped forever. See code from quake:
	/*
		void AngularMove( Vector vecDest, float flSpeed )
		{
			...
			...
			...
			if( flTravelTime < 0.1f )
			{
				pev->avelocity = g_vecZero;
				pev->nextthink = pev->ltime + 0.1f;
				return;
			}
		}
	*/
	// this block was removed from Half-Life and there no difference
	// between wait = 0 and wait = -1. But in Xash this bug was fixed
	// and level-designer errors is now actual. I'm set m_flWait to -1 for compatibility
	if( m_flWait == 0.0f )
		m_flWait = -1;

	if( pev->speed == 0 )
		pev->speed = 100;
	
	// DOOR_START_OPEN is to allow an entity to be lighted in the closed position
	// but spawn in the open position
	if( FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ))
	{
		// swap pos1 and pos2, put door at pos2, invert movement direction
		Vector vecNewAngles = m_vecAngle2;
		m_vecAngle2 = m_vecAngle1;
		m_vecAngle1 = vecNewAngles;
		pev->movedir = pev->movedir * -1;

		// We've already had our physics setup in BaseClass::Spawn, so teleport to our
		// current position. If we don't do this, our vphysics shadow will not update.
		Teleport( NULL, &m_vecAngle1, NULL );
	}
	else
	{
		SetLocalAngles( m_vecAngle1 );
	}

	m_iState = STATE_OFF;
	RelinkEntity();
	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );

	if( FBitSet( pev->spawnflags, SF_DOOR_USE_ONLY ))
	{
		SetTouch( NULL );
	}
	else
	{
		// touchable button
		SetTouch( &CBaseDoor::DoorTouch );
	}
}

void CRotDoor :: OnChangeParent( void )
{
	matrix4x4 iparent = GetParentToWorldTransform().Invert();
	matrix4x4 angle1 = iparent.ConcatTransforms( matrix4x4( g_vecZero, m_vecAngle1 ));

	angle1.GetAngles( m_vecAngle1 );

	// just recalc angle2 without transforms
	m_vecAngle2 = m_vecAngle1 + pev->movedir * m_flMoveDistance;

	// update angles if needed
	if( GetState() == STATE_ON )
		SetLocalAngles( m_vecAngle2 );
	else if( GetState() == STATE_OFF )
		SetLocalAngles( m_vecAngle1 );
}

void CRotDoor :: OnClearParent( void )
{
	matrix4x4 parent = GetParentToWorldTransform();
	matrix4x4 angle1 = parent.ConcatTransforms( matrix4x4( g_vecZero, m_vecAngle1 ));

	angle1.GetAngles( m_vecAngle1 );

	// just recalc angle2 without transforms
	m_vecAngle2 = m_vecAngle1 + pev->movedir * m_flMoveDistance;

	// update angles if needed
	if( GetState() == STATE_ON )
		SetAbsAngles( m_vecAngle2 );
	else if( GetState() == STATE_OFF )
		SetAbsAngles( m_vecAngle1 );
}

void CRotDoor :: SetToggleState( int state )
{
	if( (STATE)state == STATE_ON )
		SetLocalAngles( m_vecAngle2 );
	else SetLocalAngles( m_vecAngle1 );

	RelinkEntity( TRUE );
}