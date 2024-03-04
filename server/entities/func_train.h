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
#include "plats.h"

//
//====================== TRAIN code ==================================================
//

#define SF_TRAIN_WAIT_RETRIGGER	BIT( 0 )
#define SF_TRAIN_SETORIGIN		BIT( 1 )
#define SF_TRAIN_START_ON		BIT( 2 )	// Train is initially moving
#define SF_TRAIN_PASSABLE		BIT( 3 )	// Train is not solid -- used to make water trains

#define SF_TRAIN_REVERSE		BIT( 31 )	// hidden flag

class CTrainSequence;

class CFuncTrain : public CBasePlatTrain
{
	DECLARE_CLASS( CFuncTrain, CBasePlatTrain );
public:
	void Spawn( void );
	void Precache( void );
	void Activate( void );
	void OverrideReset( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	STATE GetState( void ) { return m_iState; }

	Vector CalcPosition( CBaseEntity *pTarg )
	{
		Vector nextPos = pTarg->GetLocalOrigin();
		if( !FBitSet( pev->spawnflags, SF_TRAIN_SETORIGIN ))
			nextPos -= (pev->mins + pev->maxs) * 0.5f;
		return nextPos;
	}

	void Wait( void );
	void Next( void );
	void SoundSetup( void );
	void Stop( void );

	void StartSequence( CTrainSequence *pSequence );
	void StopSequence( void );

	DECLARE_DATADESC();

	CTrainSequence	*m_pSequence;
	EHANDLE		m_hCurrentTarget;
	BOOL		m_activated;
};
