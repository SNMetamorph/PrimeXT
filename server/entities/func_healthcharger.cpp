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
*/
//-------------------------------------------------------------
// Wall mounted health kit
//-------------------------------------------------------------
#include "func_healthcharger.h"

LINK_ENTITY_TO_CLASS(func_healthcharger, CWallHealth);

BEGIN_DATADESC( CWallHealth )
	DEFINE_FIELD( m_flNextCharge, FIELD_TIME ),
	DEFINE_KEYFIELD( m_iReactivate, FIELD_INTEGER, "dmdelay" ),
	DEFINE_FIELD( m_iJuice, FIELD_INTEGER ),
	DEFINE_FIELD( m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( m_flSoundTime, FIELD_TIME ),
	DEFINE_FUNCTION( Recharge ),
	DEFINE_FUNCTION( Off ),
END_DATADESC()

void CWallHealth::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "dmdelay"))
	{
		m_iReactivate = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CWallHealth::Spawn()
{
	Precache( );

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL( edict(), GetModel() );
	m_iJuice = gSkillData.healthchargerCapacity;
	pev->frame = 0;			

}

void CWallHealth::Precache( void )
{
	PRECACHE_SOUND( "items/medshot4.wav" );
	PRECACHE_SOUND( "items/medshotno1.wav" );
	PRECACHE_SOUND( "items/medcharge4.wav" );
}

void CWallHealth::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	// Make sure that we have a caller
	if (!pActivator)
		return;
	// if it's not a player, ignore
	if ( !pActivator->IsPlayer() )
		return;

	// if there is no juice left, turn it off
	if (m_iJuice <= 0)
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
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/medshotno1.wav", 1.0, ATTN_NORM );
		}
		return;
	}

	SetNextThink( 0.25 );
	SetThink( &CWallHealth::Off);

	// Time to recharge yet?

	if (m_flNextCharge >= gpGlobals->time)
		return;

	// Play the on sound or the looping charging sound
	if (!m_iOn)
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/medshot4.wav", 1.0, ATTN_NORM );
		m_flSoundTime = 0.56 + gpGlobals->time;
	}
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->time))
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_STATIC, "items/medcharge4.wav", 1.0, ATTN_NORM );
	}


	// charge the player
	if ( pActivator->TakeHealth( 1, DMG_GENERIC ) )
	{
		m_iJuice--;
	}

	// govern the rate of charge
	m_flNextCharge = gpGlobals->time + 0.1;
}

void CWallHealth::Recharge(void)
{
	EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/medshot4.wav", 1.0, ATTN_NORM );
	m_iJuice = gSkillData.healthchargerCapacity;
	pev->frame = 0;			
	SetThink( &CBaseEntity::SUB_DoNothing );
}

void CWallHealth::Off(void)
{
	// Stop looping sound.
	if (m_iOn > 1)
		STOP_SOUND( ENT(pev), CHAN_STATIC, "items/medcharge4.wav" );

	m_iOn = 0;

	if ((!m_iJuice) &&  ( ( m_iReactivate = g_pGameRules->FlHealthChargerRechargeTime() ) > 0) )
	{
		SetNextThink( m_iReactivate );
		SetThink( &CWallHealth::Recharge );
	}
	else
		SetThink( &CBaseEntity::SUB_DoNothing );
}

STATE CWallHealth::GetState( void )
{
	if (m_iOn == 2)
		return STATE_IN_USE;
	else if (m_iJuice)
		return STATE_ON;
	else
		return STATE_OFF;
}
