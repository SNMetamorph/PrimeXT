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

#include "func_rot_button.h"

LINK_ENTITY_TO_CLASS( func_rot_button, CRotButton );

void CRotButton::Precache( void )
{
	const char *pszSound;

	pszSound = UTIL_ButtonSound( m_sounds );
	pev->noise = UTIL_PrecacheSound( pszSound );
}

void CRotButton::Spawn( void )
{
	Precache();

	// set the axis of rotation
	AxisDir( pev );

	// check for clockwise rotation
	if( FBitSet( pev->spawnflags, SF_ROTBUTTON_ROTATE_BACKWARDS ))
		pev->movedir = pev->movedir * -1;

	pev->movetype = MOVETYPE_PUSH;
	
	if( FBitSet( pev->spawnflags, SF_ROTBUTTON_PASSABLE ))
		pev->solid = SOLID_NOT;
	else pev->solid = SOLID_BSP;

	// shared code use this flag as BUTTON_DONTMOVE so we need to clear it here
	ClearBits( pev->spawnflags, SF_ROTBUTTON_PASSABLE );
	ClearBits( pev->spawnflags, SF_BUTTON_SPARK_IF_OFF );
	ClearBits( pev->spawnflags, SF_BUTTON_DAMAGED_AT_LASER );

	SET_MODEL( edict(), GetModel() );
	
	if( pev->speed == 0 )
		pev->speed = 40;

	if( m_flWait == 0 )
		m_flWait = 1;

	if( pev->health > 0 )
	{
		pev->takedamage = DAMAGE_YES;
	}

	m_iState = STATE_OFF;
	m_vecAngle1 = GetLocalAngles();
	m_vecAngle2 = GetLocalAngles() + pev->movedir * m_flMoveDistance;

	ASSERTSZ( m_vecAngle1 != m_vecAngle2, "rotating button start/end positions are equal" );

	m_fStayPushed = (m_flWait == -1) ? TRUE : FALSE;
	m_fRotating = TRUE;

	// if the button is flagged for USE button activation only, take away it's touch function and add a use function
	if( !FBitSet( pev->spawnflags, SF_BUTTON_TOUCH_ONLY ))
	{
		SetTouch( NULL );
		SetUse( &CBaseButton::ButtonUse );
	}
	else
	{	
		// touchable button
		SetTouch( &CBaseButton::ButtonTouch );
	}

	UTIL_SetOrigin( this, GetLocalOrigin( ));
	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity( this );
}