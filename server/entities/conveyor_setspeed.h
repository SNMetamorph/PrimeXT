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

#define SF_CONVSPEED_DEBUG		BIT( 0 )

class CConveyorSetSpeed : public CBaseDelay
{
	DECLARE_CLASS( CConveyorSetSpeed, CBaseDelay );
public:
	void		Spawn( void );
	void		KeyValue( KeyValueData *pkvd );
	BOOL		EvaluateSpeed( BOOL bStartup );
	void 		UpdateSpeed( void );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE	GetState( void ) { return m_iState; }

	DECLARE_DATADESC();

	float		m_flTime;
};
