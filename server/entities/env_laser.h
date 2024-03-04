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
#include "beam.h"

#define MAX_REFLECTED_BEAMS			48

class CSprite;

class CLaser : public CBeam
{
	DECLARE_CLASS( CLaser, CBeam );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Activate( void );

	void TurnOn( void );
	void TurnOff( void );
	virtual STATE GetState( void ) { return (pev->effects & EF_NODRAW) ? STATE_OFF : STATE_ON; };

	void	FireAtPoint( const Vector &startpos, TraceResult &point );
	bool	ShouldReflect( TraceResult &point );
	void	StrikeThink( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();

	int	m_iProjection;
	int	m_iStoppedBy;

	CSprite	*m_pSprite;
	string_t	m_iszSpriteName;
	Vector	m_firePosition;
protected:
	CBeam	*m_pReflectedBeams[MAX_REFLECTED_BEAMS];
};
