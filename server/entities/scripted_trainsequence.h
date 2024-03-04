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
//====================== TRAINSEQ code ==================================================
//
#define DIRECTION_NONE		0
#define DIRECTION_FORWARDS		1
#define DIRECTION_BACKWARDS		2
#define DIRECTION_STOP		3
#define DIRECTION_DESTINATION		4

#define SF_TRAINSEQ_REMOVE		BIT( 1 )
#define SF_TRAINSEQ_DIRECT		BIT( 2 )
#define SF_TRAINSEQ_DEBUG		BIT( 3 )

class CFuncTrain;

class CTrainSequence : public CBaseEntity
{
	DECLARE_CLASS( CTrainSequence, CBaseEntity );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	STATE GetState( void ) { return (m_pTrain) ? STATE_ON : STATE_OFF; }

	void EndThink( void );
	void StopSequence( void );
	void ArrivalNotify( void );

	DECLARE_DATADESC();

	string_t		m_iszEntity;
	string_t		m_iszDestination;
	string_t		m_iszTerminate;
	int		m_iDirection;

	CFuncTrain	*m_pTrain;
	CBaseEntity	*m_pDestination;
};
