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

===== h_cycler.cpp ========================================================

  The Halflife Cycler Monsters

*/

#include "cycler_wreckage.h"

BEGIN_DATADESC( CWreckage )
	DEFINE_FIELD( m_flStartTime, FIELD_TIME ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( cycler_wreckage, CWreckage );

void CWreckage::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->takedamage		= 0;
	pev->effects		= 0;

	pev->frame			= 0;
	pev->nextthink		= gpGlobals->time + 0.1;

	if (pev->model)
	{
		PRECACHE_MODEL( (char *)STRING(pev->model) );
		SET_MODEL( ENT(pev), STRING(pev->model) );
	}
	// pev->scale = 5.0;

	m_flStartTime		= gpGlobals->time;
}

void CWreckage::Precache( )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
}

void CWreckage::Think( void )
{
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.2;

	if (pev->dmgtime)
	{
		if (pev->dmgtime < gpGlobals->time)
		{
			UTIL_Remove( this );
			return;
		}
		else if (RANDOM_FLOAT( 0, pev->dmgtime - m_flStartTime ) > pev->dmgtime - gpGlobals->time)
		{
			return;
		}
	}
	
	Vector VecSrc;
	
	VecSrc.x = RANDOM_FLOAT( pev->absmin.x, pev->absmax.x );
	VecSrc.y = RANDOM_FLOAT( pev->absmin.y, pev->absmax.y );
	VecSrc.z = RANDOM_FLOAT( pev->absmin.z, pev->absmax.z );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, VecSrc );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( VecSrc.x );
		WRITE_COORD( VecSrc.y );
		WRITE_COORD( VecSrc.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( RANDOM_LONG(0,49) + 50 ); // scale * 10
		WRITE_BYTE( RANDOM_LONG(0, 3) + 8  ); // framerate
	MESSAGE_END();
}
