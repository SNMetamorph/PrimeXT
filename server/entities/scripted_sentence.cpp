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
***/

#include "scripted_sentence.h"

LINK_ENTITY_TO_CLASS( scripted_sentence, CScriptedSentence );

BEGIN_DATADESC( CScriptedSentence )
	DEFINE_KEYFIELD( m_iszSentence, FIELD_STRING, "sentence" ),
	DEFINE_KEYFIELD( m_iszEntity, FIELD_STRING, "entity" ),
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
	DEFINE_KEYFIELD( m_flDuration, FIELD_FLOAT, "duration" ),
	DEFINE_KEYFIELD( m_flRepeat, FIELD_FLOAT, "refire" ),
	DEFINE_FIELD( m_flAttenuation, FIELD_FLOAT ),
	DEFINE_FIELD( m_flVolume, FIELD_FLOAT ),
	DEFINE_FIELD( m_active, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_iszListener, FIELD_STRING, "listener" ),
	DEFINE_FUNCTION( FindThink ),
	DEFINE_FUNCTION( DelayThink ),
END_DATADESC()

void CScriptedSentence :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "sentence"))
	{
		m_iszSentence = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "entity"))
	{
		m_iszEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "duration"))
	{
		m_flDuration = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		m_flRadius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "refire"))
	{
		m_flRepeat = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if(FStrEq(pkvd->szKeyName, "attenuation"))
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if(FStrEq(pkvd->szKeyName, "volume"))
	{
		m_flVolume = atof( pkvd->szValue ) * 0.1;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "listener"))
	{
		m_iszListener = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CScriptedSentence :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_OFF )
	{
//		ALERT( at_console, "disable sentence: %s\n", STRING(m_iszSentence) );
		SetThink( NULL );
		SetNextThink( -1 );
	}
	else
	{
		if ( !m_active )
			return;

//		ALERT( at_console, "Firing sentence: %s\n", STRING(m_iszSentence) );
		SetThink( &CScriptedSentence::FindThink );
		pev->nextthink = gpGlobals->time;
	}
}

void CScriptedSentence :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	
	m_active = TRUE;
	// if no targetname, start now
	if ( !pev->targetname )
	{
		SetThink( &CScriptedSentence::FindThink );
		pev->nextthink = gpGlobals->time + 1.0;
	}

	switch( pev->impulse )
	{
	case 1: // Medium radius
		m_flAttenuation = ATTN_STATIC;
		break;
	
	case 2:	// Large radius
		m_flAttenuation = ATTN_NORM;
		break;

	case 3:	//EVERYWHERE
		m_flAttenuation = ATTN_NONE;
		break;
	
	default:
	case 0: // Small radius
		m_flAttenuation = ATTN_IDLE;
		break;
	}
	pev->impulse = 0;

	// No volume, use normal
	if ( m_flVolume <= 0 )
		m_flVolume = 1.0;
}

void CScriptedSentence :: FindThink( void )
{
	CBaseMonster *pMonster = FindMonster();
	if ( pMonster )
	{
		StartSentence( pMonster );
		if ( pev->spawnflags & SF_SENTENCE_ONCE )
			UTIL_Remove( this );
		SetThink( &CScriptedSentence::DelayThink );
		pev->nextthink = gpGlobals->time + m_flDuration + m_flRepeat;
		m_active = FALSE;
//		ALERT( at_console, "%s: found monster %s\n", STRING(m_iszSentence), STRING(m_iszEntity) );
	}
	else
	{
		CBaseEntity *pEntity = FindEntity();

		if( pEntity )
		{
			const char *pszSentence = STRING(m_iszSentence);

			if ( pszSentence[0] == '!' )
				EMIT_SOUND_DYN( pEntity->edict(), CHAN_VOICE, pszSentence, m_flVolume, m_flAttenuation, 0, PITCH_NORM );
			else SENTENCEG_PlayRndSz( pEntity->edict(), pszSentence, m_flVolume, m_flAttenuation, 0, PITCH_NORM );
			if ( pev->spawnflags & SF_SENTENCE_ONCE )
				UTIL_Remove( this );
			SetThink( &CScriptedSentence::DelayThink );
			pev->nextthink = gpGlobals->time + m_flDuration + m_flRepeat;
			m_active = FALSE;
		}
		else
		{
	//		ALERT( at_console, "%s: can't find monster %s\n", STRING(m_iszSentence), STRING(m_iszEntity) );
			pev->nextthink = gpGlobals->time + m_flRepeat + 0.5;
		}
	}
}

void CScriptedSentence :: DelayThink( void )
{
	m_active = TRUE;
	if ( !pev->targetname )
		pev->nextthink = gpGlobals->time + 0.1;
	SetThink( &CScriptedSentence::FindThink );
}

BOOL CScriptedSentence :: AcceptableSpeaker( CBaseMonster *pMonster )
{
	if ( pMonster )
	{
		if ( pev->spawnflags & SF_SENTENCE_FOLLOWERS )
		{
			if ( pMonster->m_hTargetEnt == NULL || !FClassnameIs(pMonster->m_hTargetEnt->pev, "player") )
				return FALSE;
		}
		BOOL override;
		if ( pev->spawnflags & SF_SENTENCE_INTERRUPT )
			override = TRUE;
		else
			override = FALSE;
		if ( pMonster->CanPlaySentence( override ) )
			return TRUE;
	}
	return FALSE;
}

CBaseMonster *CScriptedSentence :: FindMonster( void )
{
	edict_t *pentTarget;
	CBaseMonster *pMonster;


	pentTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(m_iszEntity));
	pMonster = NULL;

	while (!FNullEnt(pentTarget))
	{
		pMonster = GetMonsterPointer( pentTarget );
		if ( pMonster != NULL )
		{
			if ( AcceptableSpeaker( pMonster ) )
				return pMonster;
//			ALERT( at_console, "%s (%s), not acceptable\n", STRING(pMonster->pev->classname), STRING(pMonster->pev->targetname) );
		}
		pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, STRING(m_iszEntity));
	}
	
	CBaseEntity *pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, GetAbsOrigin(), m_flRadius )) != NULL)
	{
		if (FClassnameIs( pEntity->pev, STRING(m_iszEntity)))
		{
			if ( FBitSet( pEntity->pev->flags, FL_MONSTER ))
			{
				pMonster = pEntity->MyMonsterPointer( );
				if ( AcceptableSpeaker( pMonster ) )
					return pMonster;
			}
		}
	}
	
	return NULL;
}

CBaseEntity *CScriptedSentence :: FindEntity( void )
{
	edict_t *pentTarget;
	CBaseEntity *pEntity;


	pentTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(m_iszEntity));
	pEntity = NULL;

	while (!FNullEnt(pentTarget))
	{
		pEntity = Instance( pentTarget );
		if ( pEntity != NULL && pEntity->pev->modelindex )
		{
			return pEntity;
		}
		pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, STRING(m_iszEntity));
	}
	
	pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, GetAbsOrigin(), m_flRadius )) != NULL)
	{
		if (FClassnameIs( pEntity->pev, STRING(m_iszEntity)))
		{
			if( pEntity->pev->modelindex )
				return pEntity;
		}
	}
	
	return NULL;
}

BOOL CScriptedSentence :: StartSentence( CBaseMonster *pTarget )
{
	if ( !pTarget )
	{
		ALERT( at_aiconsole, "Not Playing sentence %s\n", STRING(m_iszSentence) );
		return NULL;
	}

	BOOL bConcurrent = FALSE;
	if ( !(pev->spawnflags & SF_SENTENCE_CONCURRENT) )
		bConcurrent = TRUE;

	CBaseEntity *pListener = NULL;
	if (!FStringNull(m_iszListener))
	{
		float radius = m_flRadius;

		if ( FStrEq( STRING(m_iszListener ), "player" ) )
			radius = 4096;	// Always find the player

		pListener = UTIL_FindEntityGeneric( STRING( m_iszListener ), pTarget->GetAbsOrigin(), radius );
	}

	pTarget->PlayScriptedSentence( STRING(m_iszSentence), m_flDuration,  m_flVolume, m_flAttenuation, bConcurrent, pListener );
	ALERT( at_aiconsole, "Playing sentence %s (%.1f)\n", STRING(m_iszSentence), m_flDuration );
	SUB_UseTargets( NULL, USE_TOGGLE, 0 );
	return TRUE;
}
