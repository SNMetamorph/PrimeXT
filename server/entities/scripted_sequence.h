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

#define SF_SCRIPT_WAITTILLSEEN			1
#define SF_SCRIPT_EXITAGITATED			2
#define SF_SCRIPT_REPEATABLE			4
#define SF_SCRIPT_LEAVECORPSE			8
//#define SF_SCRIPT_INTERPOLATE			16 // don't use, old bug
#define SF_SCRIPT_NOINTERRUPT			32
#define SF_SCRIPT_OVERRIDESTATE			64
#define SF_SCRIPT_NOSCRIPTMOVEMENT		128
#define SF_SCRIPT_FIXANIMTRANSITIONS	256

#define SCRIPT_BREAK_CONDITIONS		(bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE)

enum SS_INTERRUPT
{
	SS_INTERRUPT_IDLE = 0,
	SS_INTERRUPT_BY_NAME,
	SS_INTERRUPT_AI,
};

class CCineMonster : public CBaseMonster
{
	DECLARE_CLASS( CCineMonster, CBaseMonster );
public:
	void Spawn( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void Blocked( CBaseEntity *pOther );
	virtual void Touch( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	virtual void Activate( void );

	DECLARE_DATADESC();

	void CineThink( void );
	void Pain( void );
	void Die( void );
	void DelayStart( int state );
	BOOL FindEntity( void );
	virtual void PossessEntity( void );

	void ReleaseEntity( CBaseMonster *pEntity );
	void CancelScript( void );
	virtual BOOL StartSequence( CBaseMonster *pTarget, int iszSeq, BOOL completeOnEmpty );
	virtual BOOL FCanOverrideState ( void );
	void SequenceDone ( CBaseMonster *pMonster );
	virtual void FixScriptMonsterSchedule( CBaseMonster *pMonster );
	BOOL CanInterrupt( void );
	void AllowInterrupt( BOOL fAllow );
	int IgnoreConditions( void );
	void OnRemove( void );

	int	m_iszIdle;		// string index for idle animation
	int	m_iszPlay;		// string index for scripted animation
	int	m_iszEntity;	// entity that is wanted for this script
	int	m_fMoveTo;
	string_t	m_iszFireOnBegin; // entity to fire when the sequence _starts_.
	int	m_iFinishSchedule;
	float	m_flRadius;		// range to search
	float	m_flRepeat;	// repeat rate

	int m_iDelay;
	float m_startTime;

	int m_saved_movetype;
	int m_saved_solid;
	int m_saved_effects;
//	Vector m_vecOrigOrigin;
	BOOL m_interruptable;
};
