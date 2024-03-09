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
#include "func_tracktrain.h"

#define SF_ACTIVATE_TRAIN		BIT( 0 )
#define SF_TRAINSPEED_DEBUG		BIT( 1 )

class CTrainSetSpeed : public CBaseDelay
{
	DECLARE_CLASS( CTrainSetSpeed, CBaseDelay );
public:
	void		Spawn( void );
	void		KeyValue( KeyValueData *pkvd );
	void 		Find( void );
	void 		UpdateSpeed( void );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE	GetState( void ) { return m_iState; }

	DECLARE_DATADESC();

	CFuncTrackTrain	*m_pTrain;
	int		m_iMode;
	float		m_flTime;
	float		m_flInterval;
};
