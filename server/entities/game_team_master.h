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

//
// CGameTeamMaster / game_team_master	-- "Masters" like multisource, but based on the team of the activator
// Only allows mastered entity to fire if the team matches my team
//
// team index (pulled from server team list "mp_teamlist"
// Flag: Remove on Fire
// Flag: Any team until set?		-- Any team can use this until the team is set (otherwise no teams can use it)
//

#define SF_TEAMMASTER_FIREONCE			0x0001
#define SF_TEAMMASTER_ANYTEAM			0x0002

class CGameTeamMaster : public CRulePointEntity
{
	DECLARE_CLASS( CGameTeamMaster, CRulePointEntity );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL IsTriggered( CBaseEntity *pActivator );
	const char *TeamID( void );
	inline BOOL RemoveOnFire( void ) { return (pev->spawnflags & SF_TEAMMASTER_FIREONCE) ? TRUE : FALSE; }
	inline BOOL AnyTeam( void ) { return (pev->spawnflags & SF_TEAMMASTER_ANYTEAM) ? TRUE : FALSE; }

private:
	BOOL	TeamMatch( CBaseEntity *pActivator );

	int	m_teamIndex;
	USE_TYPE	triggerType;
};
