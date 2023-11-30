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
#include "util.h"
#include "cbase.h"
#include "client.h"
#include "player.h"
#include "func_door.h"

#define SF_BUTTON_DONTMOVE		BIT( 0 )

#define SF_BUTTON_ONLYDIRECT		BIT( 4 )	// LRC - button can't be used through walls.
#define SF_BUTTON_TOGGLE		BIT( 5 )	// button stays pushed until reactivated
#define SF_BUTTON_SPARK_IF_OFF	BIT( 6 )	// button sparks in OFF state
#define SF_BUTTON_DAMAGED_AT_LASER	BIT( 7 )	// if health is set can be damaged only at env_laser or gauss
#define SF_BUTTON_TOUCH_ONLY		BIT( 8 )	// button only fires as a result of USE key.

class CBaseButton : public CBaseToggle
{
	DECLARE_CLASS( CBaseButton, CBaseToggle );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData* pkvd);

	void ButtonActivate( );

	void ButtonTouch( CBaseEntity *pOther );
	void ButtonSpark ( void );
	void TriggerAndWait( void );
	void ButtonReturn( void );
	void ButtonBackHome( void );
	void ButtonUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	
	enum BUTTON_CODE { BUTTON_NOTHING, BUTTON_ACTIVATE, BUTTON_RETURN };
	BUTTON_CODE ButtonResponseToTouch( void );
	
	DECLARE_DATADESC();

	STATE GetState( void ) { return m_iState; }

	// Buttons that don't take damage can be IMPULSE used
	virtual int ObjectCaps(void);

	BOOL	m_fStayPushed;	// button stays pushed in until touched again?
	BOOL	m_fRotating;	// a rotating button?  default is a sliding button.

	locksound_t m_ls;		// door lock sounds
	
	int	m_iLockedSound;	// ordinals from entity selection
	byte	m_bLockedSentence;	
	int	m_iUnlockedSound;	
	byte	m_bUnlockedSentence;
	int	m_sounds;
};