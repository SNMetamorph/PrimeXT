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
#include "eiface.h"
#include "util.h"
#include "gamerules.h"
#include "cbase.h"
#include "player.h"

class CRuleEntity : public CBaseEntity
{
	DECLARE_CLASS( CRuleEntity, CBaseEntity );
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();

	void	SetMaster( int iszMaster ) { m_iszMaster = iszMaster; }

protected:
	BOOL	CanFireForActivator( CBaseEntity *pActivator );

private:
	string_t	m_iszMaster;
};

// 
// CRulePointEntity -- base class for all rule "point" entities (not brushes)
//
class CRulePointEntity : public CRuleEntity
{
	DECLARE_CLASS( CRulePointEntity, CRuleEntity );
public:
	void Spawn( void );
};

// 
// CRuleBrushEntity -- base class for all rule "brush" entities (not brushes)
// Default behavior is to set up like a trigger, invisible, but keep the model for volume testing
//
class CRuleBrushEntity : public CRuleEntity
{
	DECLARE_CLASS( CRuleBrushEntity, CRuleEntity );
public:
	void	Spawn( void );
};
