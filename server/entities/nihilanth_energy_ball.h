/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

#pragma once

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "effects.h"
#include "nihilanth.h"

class CNihilanthHVR : public CBaseMonster
{
	DECLARE_CLASS( CNihilanthHVR, CBaseMonster );
public:
	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );

	void CircleInit( CBaseEntity *pTarget );
	void AbsorbInit( void );
	void TeleportInit( CNihilanth *pOwner, CBaseEntity *pEnemy, CBaseEntity *pTarget, CBaseEntity *pTouch );
	void GreenBallInit( void );
	void ZapInit( CBaseEntity *pEnemy );

	void HoverThink( void );
	BOOL CircleTarget( Vector vecTarget );
	void DissipateThink( void );

	void ZapThink( void );
	void TeleportThink( void );
	void TeleportTouch( CBaseEntity *pOther );
	
	void RemoveTouch( CBaseEntity *pOther );
	void BounceTouch( CBaseEntity *pOther );
	void ZapTouch( CBaseEntity *pOther );

	CBaseEntity *RandomClassname( const char *szName );

	void MovetoTarget( Vector vecTarget );
	virtual void Crawl( void );

	void Zap( void );
	void Teleport( void );

	float m_flIdealVel;
	Vector m_vecIdeal;
	CNihilanth *m_pNihilanth;
	EHANDLE m_hTouch;
	int m_nFrames;
};