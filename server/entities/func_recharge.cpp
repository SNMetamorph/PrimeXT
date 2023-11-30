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

===== h_battery.cpp ========================================================

  battery-related code

*/

#include "func_recharge.h"

BEGIN_DATADESC( CRecharge )
	DEFINE_FIELD( m_flNextCharge, FIELD_TIME ),
	DEFINE_KEYFIELD( m_iReactivate, FIELD_INTEGER, "dmdelay" ),
	DEFINE_FIELD( m_iJuice, FIELD_INTEGER ),
	DEFINE_FIELD( m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( m_flSoundTime, FIELD_TIME ),
	DEFINE_FUNCTION( Recharge ),
	DEFINE_FUNCTION( Off ),
END_DATADESC()

LINK_ENTITY_TO_CLASS(func_recharge, CRecharge);


void CRecharge::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "dmdelay"))
	{
		m_iReactivate = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CRecharge::Spawn()
{
	Precache( );

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL( edict(), GetModel() );
	m_iJuice = gSkillData.suitchargerCapacity;
	pev->frame = 0;			
}

void CRecharge::Precache()
{
	PRECACHE_SOUND( "items/suitcharge1.wav" );
	PRECACHE_SOUND( "items/suitchargeno1.wav" );
	PRECACHE_SOUND( "items/suitchargeok1.wav" );
}

void CRecharge::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	// if it's not a player, ignore
	if( !pActivator || !pActivator->IsPlayer( ))
		return;

	// if there is no juice left, turn it off
	if( m_iJuice <= 0 )
	{
		pev->frame = 1;			
		Off();
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	// if the player doesn't have the suit, or there is no juice left, make the deny noise
	if(( m_iJuice <= 0 ) || !pPlayer->HasWeapon( WEAPON_SUIT ))
	{
		if (m_flSoundTime <= gpGlobals->time)
		{
			m_flSoundTime = gpGlobals->time + 0.62;
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/suitchargeno1.wav", 0.85, ATTN_NORM );
		}
		return;
	}

	SetNextThink( 0.25 );
	SetThink( &CRecharge::Off );

	// Time to recharge yet?
	if (m_flNextCharge >= gpGlobals->time)
		return;

	m_hActivator = pActivator;

	// Play the on sound or the looping charging sound
	if (!m_iOn)
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/suitchargeok1.wav", 0.85, ATTN_NORM );
		m_flSoundTime = 0.56 + gpGlobals->time;
	}
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->time))
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_STATIC, "items/suitcharge1.wav", 0.85, ATTN_NORM );
	}


	// charge the player
	if (m_hActivator->pev->armorvalue < 100)
	{
		m_iJuice--;
		m_hActivator->pev->armorvalue += 1;

		if (m_hActivator->pev->armorvalue > 100)
			m_hActivator->pev->armorvalue = 100;
	}

	// govern the rate of charge
	m_flNextCharge = gpGlobals->time + 0.1;
}

void CRecharge::Recharge(void)
{
	m_iJuice = gSkillData.suitchargerCapacity;
	pev->frame = 0;			
	SetThink( &CBaseEntity::SUB_DoNothing );
}

void CRecharge::Off(void)
{
	// Stop looping sound.
	if (m_iOn > 1)
		STOP_SOUND( ENT(pev), CHAN_STATIC, "items/suitcharge1.wav" );

	m_iOn = 0;

	if ((!m_iJuice) && (( m_iReactivate = g_pGameRules->FlHEVChargerRechargeTime() ) > 0) )
	{
		SetNextThink( m_iReactivate );
		SetThink( &CRecharge::Recharge );
	}
	else
		SetThink( &CBaseEntity::SUB_DoNothing );
}

STATE CRecharge::GetState( void )
{
	if (m_iOn == 2)
		return STATE_IN_USE;
	else if (m_iJuice)
		return STATE_ON;
	else
		return STATE_OFF;
}
