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
#include "maprules.h"

// CGameScore / game_score	-- award points to player / team 
//	Points +/- total
//	Flag: Allow negative scores					SF_SCORE_NEGATIVE
//	Flag: Award points to team in teamplay		SF_SCORE_TEAM

#define SF_SCORE_NEGATIVE			0x0001
#define SF_SCORE_TEAM				0x0002

class CGameScore : public CRulePointEntity
{
	DECLARE_CLASS( CGameScore, CRulePointEntity );
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	inline	int	Points( void ) { return pev->frags; }
	inline	BOOL	AllowNegativeScore( void ) { return pev->spawnflags & SF_SCORE_NEGATIVE; }
	inline	BOOL	AwardToTeam( void ) { return pev->spawnflags & SF_SCORE_TEAM; }

	inline	void	SetPoints( int points ) { pev->frags = points; }
};
