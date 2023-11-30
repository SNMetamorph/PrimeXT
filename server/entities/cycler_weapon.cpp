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
/*

===== h_cycler.cpp ========================================================

  The Halflife Cycler Monsters

*/

#include "cycler_weapon.h"

LINK_ENTITY_TO_CLASS( cycler_weapon, CWeaponCycler );
LINK_ENTITY_TO_CLASS( weapon_question, CWeaponCycler );	// saverestore issues

BEGIN_DATADESC( CWeaponCycler )
	DEFINE_FIELD( m_iPlayerModel, FIELD_MODELNAME ),
	DEFINE_FIELD( m_iViewModel, FIELD_MODELNAME ),
	DEFINE_FIELD( m_iWorldModel, FIELD_MODELNAME ),
END_DATADESC()

void CWeaponCycler::Spawn( )
{
	// g-cont. this alias need for right slot switching because all selectable items must be preceed with "weapon_" or "item_"
	pev->classname = MAKE_STRING( "weapon_question" );
	m_iId = WEAPON_CYCLER;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_NONE;

	char basemodel[80], v_path[80], p_path[80], w_path[80];

	strncpy( basemodel, (char *)STRING(pev->model), sizeof( basemodel ) - 1 ); 

	for( int i = 0; i < (int)strlen( basemodel ); i++ )
	{
		int c = basemodel[i]; 

		if(( c == 'v' || c == 'p' || c == 'w' ) && basemodel[i+1] == '_' )
		{
			basemodel[i] = 'v';
			strcpy( v_path, basemodel );
			basemodel[i] = 'p';
			strcpy( p_path, basemodel );
			basemodel[i] = 'w';
			strcpy( w_path, basemodel );

			// create wepon model pathes
			m_iPlayerModel = ALLOC_STRING( p_path );
			m_iWorldModel = ALLOC_STRING( w_path );
			m_iViewModel = ALLOC_STRING( v_path );
			break;
		}
	}

	if( m_iPlayerModel && m_iWorldModel && m_iViewModel )
	{
		PRECACHE_MODEL( (char *)STRING(m_iPlayerModel) );
		PRECACHE_MODEL( (char *)STRING(m_iWorldModel) );
		PRECACHE_MODEL( (char *)STRING(m_iViewModel) );

		// set right world model
		pev->model = m_iWorldModel;

		SET_MODEL( ENT(pev), STRING(pev->model) );
	}
	else
	{
		// fallback to default relationship
		PRECACHE_MODEL( (char *)STRING(pev->model) );
		SET_MODEL( ENT(pev), STRING(pev->model) );

		// setup viewmodel
		m_iViewModel = pev->model;
          }
	m_iClip = -1;
	FallInit();
}

void CWeaponCycler::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "deploy"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holster"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "primary"))
	{
		pev->sequence = atoi(pkvd->szValue);
		m_bActiveAnims.primary = strlen(pkvd->szKeyName) > 0;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "secondary"))
	{
		pev->team = atoi(pkvd->szValue);
		m_bActiveAnims.secondary = strlen(pkvd->szKeyName) > 0;
		pkvd->fHandled = TRUE;
	}
	else CBasePlayerWeapon::KeyValue( pkvd );
}

int CWeaponCycler::GetItemInfo(ItemInfo *p)
{
	p->pszName = "weapon_question";	// need for right HUD displaying
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_CYCLER;
	p->iWeight = -1;

	return 1;
}

BOOL CWeaponCycler::Deploy( )
{
	return DefaultDeploy( (char *)STRING( m_iViewModel ), (char *)STRING( m_iPlayerModel ), pev->impulse, "onehanded" );
}

void CWeaponCycler::Holster( )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
	SendWeaponAnim( pev->button );
}

void CWeaponCycler::PrimaryAttack()
{
	if (m_bActiveAnims.primary)
	{
		SendWeaponAnim(pev->sequence);
		m_flNextPrimaryAttack = gpGlobals->time + 3.0;
	}
}

void CWeaponCycler::SecondaryAttack( void )
{
	if (m_bActiveAnims.secondary)
	{
		SendWeaponAnim(pev->team);
		m_flNextSecondaryAttack = gpGlobals->time + 3.0;
	}
}