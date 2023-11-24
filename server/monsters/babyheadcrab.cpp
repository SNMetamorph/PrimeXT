/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// babyheadcrab.cpp - more tiny, jumpy alien parasite
//=========================================================

#include "babyheadcrab.h"

extern Schedule_t	slHCRangeAttack1Fast[];

LINK_ENTITY_TO_CLASS( monster_babycrab, CBabyCrab );

const char *CBabyCrab::pIdleSounds[] =
{
	"headcrab/hc_idle1.wav",
	"headcrab/hc_idle2.wav",
	"headcrab/hc_idle3.wav",
};
const char *CBabyCrab::pAlertSounds[] =
{
	"headcrab/hc_alert1.wav",
};
const char *CBabyCrab::pPainSounds[] =
{
	"headcrab/hc_pain1.wav",
	"headcrab/hc_pain2.wav",
	"headcrab/hc_pain3.wav",
};
const char *CBabyCrab::pAttackSounds[] =
{
	"headcrab/hc_attack1.wav",
	"headcrab/hc_attack2.wav",
	"headcrab/hc_attack3.wav",
};

const char *CBabyCrab::pDeathSounds[] =
{
	"headcrab/hc_die1.wav",
	"headcrab/hc_die2.wav",
};

const char *CBabyCrab::pBiteSounds[] =
{
	"headcrab/hc_headbite.wav",
};

void CBabyCrab :: Spawn( void )
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/baby_headcrab.mdl");
	UTIL_SetSize(pev, Vector(-12, -12, 0), Vector(12, 12, 24));

	pev->solid		= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->rendermode		= kRenderTransTexture;
	pev->renderamt		= 192;

	if (!pev->health) pev->health	= gSkillData.headcrabHealth * 0.25;	// less health than full grown
	pev->view_ofs		= Vector ( 0, 0, 20 );// position of the eyes relative to monster's origin.
	pev->yaw_speed		= 5;//!!! should we put this in the monster's changeanim function since turn rates may vary with state/anim?
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	MonsterInit();
}

void CBabyCrab :: Precache( void )
{
	PRECACHE_SOUND_ARRAY(pIdleSounds);
	PRECACHE_SOUND_ARRAY(pAlertSounds);
	PRECACHE_SOUND_ARRAY(pPainSounds);
	PRECACHE_SOUND_ARRAY(pAttackSounds);
	PRECACHE_SOUND_ARRAY(pDeathSounds);
	PRECACHE_SOUND_ARRAY(pBiteSounds);

	PRECACHE_MODEL( "models/baby_headcrab.mdl" );
}


void CBabyCrab :: SetYawSpeed ( void )
{
	pev->yaw_speed = 120;
}


BOOL CBabyCrab :: CheckRangeAttack1( float flDot, float flDist )
{
	if ( pev->flags & FL_ONGROUND )
	{
		if ( pev->groundentity && (pev->groundentity->v.flags & (FL_CLIENT|FL_MONSTER)) )
			return TRUE;

		// A little less accurate, but jump from closer
		if ( flDist <= 180 && flDot >= 0.55 )
			return TRUE;
	}

	return FALSE;
}


Schedule_t* CBabyCrab :: GetScheduleOfType ( int Type )
{
	switch( Type )
	{
		case SCHED_FAIL:	// If you fail, try to jump!
			if ( m_hEnemy != NULL )
				return slHCRangeAttack1Fast;
		break;

		case SCHED_RANGE_ATTACK1:
		{
			return slHCRangeAttack1Fast;
		}
		break;
	}

	return CHeadCrab::GetScheduleOfType( Type );
}

