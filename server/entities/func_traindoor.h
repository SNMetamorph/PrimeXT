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

#include "func_door.h"

typedef enum
{
	TD_CLOSED,
	TD_SHIFT_UP,
	TD_SLIDING_UP,
	TD_OPENED,
	TD_SLIDING_DOWN,
	TD_SHIFT_DOWN
} TRAINDOOR_STATE;

#define SF_TRAINDOOR_INVERSE			BIT( 0 )
#define SF_TRAINDOOR_OPEN_IN_MOVING		BIT( 1 )
#define SF_TRAINDOOR_ONOFF_MODE		BIT( 2 )

class CFuncTrackTrain;

class CBaseTrainDoor : public CBaseToggle
{
	DECLARE_CLASS( CBaseTrainDoor, CBaseToggle );
public:
	void Spawn( void );
	void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Blocked( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return ((CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION)) | FCAP_SET_MOVEDIR; }
	virtual bool IsDoorControl( void ) { return !FBitSet( pev->spawnflags, SF_TRAINDOOR_OPEN_IN_MOVING ) && GetState() != STATE_OFF; }

	DECLARE_DATADESC();

	// local functions
	void FindTrain( void );
	void DoorGoUp( void );
	void DoorGoDown( void );
	void DoorHitTop( void );
	void DoorSlideUp( void );
	void DoorSlideDown( void );
	void DoorSlideWait( void );		// wait before sliding
	void DoorHitBottom( void );
	void ActivateTrain( void );
	
	int		m_iMoveSnd;	// sound a door makes while moving
	int		m_iStopSnd;	// sound a door makes when it stops

	CFuncTrackTrain	*m_pTrain;	// my train pointer
	Vector		m_vecOldAngles;

	TRAINDOOR_STATE	door_state;
	virtual void	OverrideReset( void );
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE	GetState ( void );
};