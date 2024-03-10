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

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define SF_SENTENCE_ONCE		0x0001
#define SF_SENTENCE_FOLLOWERS		0x0002	// only say if following player
#define SF_SENTENCE_INTERRUPT		0x0004	// force talking except when dead
#define SF_SENTENCE_CONCURRENT	0x0008	// allow other people to keep talking

class CScriptedSentence : public CBaseToggle
{
	DECLARE_CLASS( CScriptedSentence, CBaseToggle );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void FindThink( void );
	void DelayThink( void );
	int ObjectCaps( void ) { return (CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }

	DECLARE_DATADESC();

	CBaseMonster *FindMonster( void );
	CBaseEntity *FindEntity( void );
	BOOL AcceptableSpeaker( CBaseMonster *pMonster );
	BOOL StartSentence( CBaseMonster *pTarget );
private:
	string_t	m_iszSentence;		// string index for idle animation
	string_t	m_iszEntity;	// entity that is wanted for this sentence
	float	m_flRadius;		// range to search
	float	m_flDuration;	// How long the sentence lasts
	float	m_flRepeat;	// repeat rate
	float	m_flAttenuation;
	float	m_flVolume;
	BOOL	m_active;
	string_t	m_iszListener;	// name of entity to look at while talking
};
