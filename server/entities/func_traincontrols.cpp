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

#include "func_traincontrols.h"

LINK_ENTITY_TO_CLASS( func_traincontrols, CFuncTrainControls );

BEGIN_DATADESC( CFuncTrainControls )
	DEFINE_FUNCTION( Find ),
END_DATADESC()

void CFuncTrainControls :: Find( void )
{
	CBaseEntity *pTarget = NULL;

	do 
	{
		pTarget = UTIL_FindEntityByTargetname( pTarget, GetTarget( ));
	} while ( pTarget && !FClassnameIs( pTarget, "func_tracktrain" ));

	if( !pTarget )
	{
		ALERT( at_error, "TrackTrainControls: No train %s\n", GetTarget( ));
		UTIL_Remove( this );
		return;
	}

	// UNDONE: attach traincontrols with parent system if origin-brush is present?
	CFuncTrackTrain *ptrain = (CFuncTrackTrain *)pTarget;
	ptrain->SetControls( this );
	UTIL_Remove( this );
}

void CFuncTrainControls :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( edict(), GetModel() );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	RelinkEntity( TRUE );
	
	SetThink( &CFuncTrainControls::Find );
	SetNextThink( 0.0f );
}
