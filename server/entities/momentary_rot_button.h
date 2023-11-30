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

#define SF_MOMENTARY_ROT_DOOR			BIT( 0 )
#define SF_MOMENTARY_ROT_BUTTON_AUTO_RETURN	BIT( 4 )

class CMomentaryRotButton : public CBaseToggle
{
	DECLARE_CLASS( CMomentaryRotButton, CBaseToggle );
public:
	void	Spawn ( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	ButtonUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UseMoveDone( void );
	void	ReturnMoveDone( void );
	void	UpdateThink( void );
	void	SetPositionMoveDone( void );
	STATE	GetState( void ) { return m_iState; }
	void	UpdateSelf( float value, bool bPlaySound );
	void	PlaySound( void );
	void	UpdateTarget( float value );
	float	GetPos( const Vector &vecAngles );
	void	UpdateAllButtons( void );
	void	UpdateButton( void );
	void	SetPosition( float value );

	DECLARE_DATADESC();

	static CMomentaryRotButton *Instance(edict_t *pent);

	virtual float GetPosition(void);

	virtual int ObjectCaps(void);

	int	m_sounds;
	int	m_lastUsed;
	int	m_direction;
	bool	m_bUpdateTarget;	// used when jiggling so that we don't jiggle the target (door, etc)
	float	m_startPosition;
	float	m_returnSpeed;
	Vector	m_start;
	Vector	m_end;
};