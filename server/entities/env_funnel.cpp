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

#include "env_funnel.h"

#define SF_FUNNEL_REVERSE		1 // funnel effect repels particles instead of attracting them.

LINK_ENTITY_TO_CLASS( env_funnel, CEnvFunnel );

void CEnvFunnel :: Precache ( void )
{
	if (pev->netname)
		m_iSprite = PRECACHE_MODEL ( (char*)STRING(pev->netname) );
	else
		m_iSprite = PRECACHE_MODEL ( "sprites/flare6.spr" );
}

void CEnvFunnel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector absOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_LARGEFUNNEL );
		WRITE_COORD( absOrigin.x );
		WRITE_COORD( absOrigin.y );
		WRITE_COORD( absOrigin.z );
		WRITE_SHORT( m_iSprite );

		if ( pev->spawnflags & SF_FUNNEL_REVERSE )// funnel flows in reverse?
		{
			WRITE_SHORT( 1 );
		}
		else
		{
			WRITE_SHORT( 0 );
		}


	MESSAGE_END();

	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

void CEnvFunnel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
}
