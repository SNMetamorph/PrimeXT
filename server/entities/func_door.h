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

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "client.h"
#include "player.h"
#include "func_door.h"
#include "trains.h"

// doors
#define SF_DOOR_START_OPEN			BIT( 0 )
#define SF_DOOR_ROTATE_BACKWARDS		BIT( 1 )
#define SF_DOOR_ONOFF_MODE			BIT( 2 )	// was Door don't link from quake. This feature come from Spirit. Thx Laurie
#define SF_DOOR_PASSABLE			BIT( 3 )
#define SF_DOOR_ONEWAY			BIT( 4 )
#define SF_DOOR_NO_AUTO_RETURN		BIT( 5 )
#define SF_DOOR_ROTATE_Z			BIT( 6 )
#define SF_DOOR_ROTATE_X			BIT( 7 )
#define SF_DOOR_USE_ONLY			BIT( 8 )	// door must be opened by player's use button.
#define SF_DOOR_NOMONSTERS			BIT( 9 )	// monster can't open

#define SF_DOOR_SILENT			BIT( 31 )

class CBaseDoor : public CBaseToggle
{
	DECLARE_CLASS( CBaseDoor, CBaseToggle );
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int	GetDoorMovementGroup( CBaseDoor *pDoorList[], int listMax );
	void	Blocked( CBaseEntity *pOther );
	void	OnChangeParent( void );
	void	OnClearParent( void );
	void	Activate( void );

	virtual int ObjectCaps( void ) 
	{ 
		if( FBitSet( pev->spawnflags, SF_DOOR_USE_ONLY ))
			return (BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)) | FCAP_IMPULSE_USE | FCAP_SET_MOVEDIR;
		return (BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)) | FCAP_SET_MOVEDIR;
	};

	DECLARE_DATADESC();

	virtual void SetToggleState( int state );
	virtual bool IsRotatingDoor() { return false; }
	virtual STATE GetState ( void ) { return m_iState; };

	BOOL DoorActivate( void );
	void DoorGoUp( void );
	void DoorGoDown( void );
	void DoorHitTop( void );
	void DoorHitBottom( void );
	void DoorTouch( CBaseEntity *pOther );
	void ChainTouch( CBaseEntity *pOther );
	void ChainUse( USE_TYPE useType, float value );

	byte	m_bHealthValue;		// some doors are medi-kit doors, they give players health
	int	m_iMoveSnd;		// sound a door makes while moving
	int	m_iStopSnd;		// sound a door makes when it stops

	locksound_t m_ls;			// door lock sounds
	
	int	m_iLockedSound;		// ordinals from entity selection
	byte	m_bLockedSentence;	
	int	m_iUnlockedSound;	
	byte	m_bUnlockedSentence;

	string_t	m_iChainTarget;		// feature come from hl2
	bool	m_bDoorGroup;
	bool	m_bDoorTouched;		// don't save\restore this
};