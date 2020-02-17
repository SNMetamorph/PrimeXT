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
// sprite monster like in doom, wolf3d. Just for fun
// 		UNDER CONSTRUCTION
//=========================================================
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"

typedef enum
{
	STATE_IDLE = 0,
	STATE_RUN,
	STATE_ATTACK,
	STATE_PAIN,
	STATE_KILLED,
	STATE_GIBBED,
	MAX_ANIMS
} AISTATE;

typedef enum
{
	ATTACK_NONE = 0,
	ATTACK_STRAIGHT,
	ATTACK_SLIDING,
	ATTACK_MELEE,
	ATTACK_MISSILE
} ATTACKSTATE;

// distance ranges
typedef enum
{
	RANGE_MELEE = 0,
	RANGE_NEAR,
	RANGE_MID,
	RANGE_FAR
} RANGETYPE;

#define SF_FLYING_MONSTER	BIT( 0 )

class CSpriteMonster : public CBaseDelay
{
	DECLARE_CLASS( CSpriteMonster, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );

	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	RANGETYPE TargetRange( CBaseEntity *pTarg );
	BOOL TargetVisible( CBaseEntity *pTarg );
	void AttackFinished( float flFinishTime );
	void AI_Run( float flDist );
	void InitThink( void );
	void MonsterThink( void );
	BOOL FindTarget( void );
	BOOL InFront( CBaseEntity *pTarg );
	BOOL MonsterCheckAnyAttack( void );
	BOOL MonsterCheckAttack( void );
	void FoundTarget( void );
	void HuntTarget( void );
	void MonsterIdle( void );
	void MonsterSight( void );
	void MonsterRun( void );

	virtual STATE GetState( void ) { return (pev->effects & EF_NODRAW) ? STATE_OFF : STATE_ON; };

	DECLARE_DATADESC();

	AISTATE		m_iAIState;
	ATTACKSTATE	m_iAttackState;

	Vector		anims[MAX_ANIMS];

	float		m_flMoveDistance;	// member laste move distance. Used for strafe
	BOOL		m_fLeftY;

	float		m_flSightTime;
	EHANDLE		m_hSightEntity;
	int		m_iRefireCount;

	float		m_flEnemyYaw;
	RANGETYPE		m_iEnemyRange;
	BOOL		m_fEnemyInFront;
	BOOL		m_fEnemyVisible;
	float		m_flSearchTime;

	float		m_flAttackFinished;
	edict_t		*oldenemy;
};

LINK_ENTITY_TO_CLASS( monster_sprite, CSpriteMonster );

BEGIN_DATADESC( CSpriteMonster )
	DEFINE_FIELD( m_iAIState, FIELD_INTEGER ),
	DEFINE_FIELD( m_iAttackState, FIELD_INTEGER ),
	DEFINE_AUTO_ARRAY( anims, FIELD_VECTOR ),
	DEFINE_FIELD( m_flMoveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_fLeftY, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fEnemyInFront, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fEnemyVisible, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hSightEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iRefireCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_flEnemyYaw, FIELD_FLOAT ),
	DEFINE_FIELD( m_iEnemyRange, FIELD_INTEGER ),
	DEFINE_FIELD( m_flSearchTime, FIELD_TIME ),
	DEFINE_FIELD( m_flSightTime, FIELD_TIME ),
	DEFINE_FIELD( m_flAttackFinished, FIELD_TIME ),
	DEFINE_FIELD( oldenemy, FIELD_EDICT ),
	DEFINE_FUNCTION( InitThink ),
	DEFINE_FUNCTION( MonsterThink ),
END_DATADESC()

void CSpriteMonster::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->frame = anims[STATE_IDLE].x;

	pev->movetype = MOVETYPE_NONE;

	Precache();
	SET_MODEL( edict(), GetModel() );

	pev->rendercolor = Vector( 255, 255, 255 );
	SetThink( &CSpriteMonster::InitThink );
	SetNextThink( 0.1f );
	pev->scale = 1.5;
}

void CSpriteMonster::Precache( void )
{
	PRECACHE_MODEL( GetModel() );
}

void CSpriteMonster :: KeyValue( KeyValueData *pkvd )
{
	int	range[2];

	if( FStrEq( pkvd->szKeyName, "idle" ))
	{
		UTIL_StringToIntArray( range, 2, pkvd->szValue );
		anims[STATE_IDLE].x = range[0];
		anims[STATE_IDLE].y = range[1];
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "run" ))
	{
		UTIL_StringToIntArray( range, 2, pkvd->szValue );
		anims[STATE_RUN].x = range[0];
		anims[STATE_RUN].y = range[1];
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "attack" ))
	{
		UTIL_StringToIntArray( range, 2, pkvd->szValue );
		anims[STATE_ATTACK].x = range[0];
		anims[STATE_ATTACK].y = range[1];
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "pain" ))
	{
		UTIL_StringToIntArray( range, 2, pkvd->szValue );
		anims[STATE_PAIN].x = range[0];
		anims[STATE_PAIN].y = range[1];
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "killed" ))
	{
		UTIL_StringToIntArray( range, 2, pkvd->szValue );
		anims[STATE_KILLED].x = range[0];
		anims[STATE_KILLED].y = range[1];
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "gibbed" ))
	{
		UTIL_StringToIntArray( range, 2, pkvd->szValue );
		anims[STATE_GIBBED].x = range[0];
		anims[STATE_GIBBED].y = range[1];
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

/*
=============
TargetRange

returns the range catagorization of an entity reletive to self
0	melee range, will become hostile even if back is turned
1	visibility and infront, or visibility and show hostile
2	infront and show hostile
3	only triggered by damage
=============
*/
RANGETYPE CSpriteMonster :: TargetRange( CBaseEntity *pTarg )
{
	Vector spot1 = EyePosition();
	Vector spot2 = pTarg->EyePosition();
	
	float dist = (spot1 - spot2).Length();
	if (dist < 120)
		return RANGE_MELEE;
	if (dist < 500)
		return RANGE_NEAR;
	if (dist < 1000)
		return RANGE_MID;
	return RANGE_FAR;
}

/*
=============
TargetVisible

returns 1 if the entity is visible to self, even if not InFront ()
=============
*/
BOOL CSpriteMonster :: TargetVisible( CBaseEntity *pTarg )
{
	TraceResult tr;

	Vector spot1 = EyePosition();
	Vector spot2 = pTarg->EyePosition();

	// see through other monsters
	UTIL_TraceLine(spot1, spot2, ignore_monsters, ignore_glass, ENT(pev), &tr);
	
	if (tr.fInOpen && tr.fInWater)
		return FALSE;		// sight line crossed contents

	if (tr.flFraction == 1.0)
		return TRUE;
	return FALSE;
}

/*
=============
InFront

returns 1 if the entity is in front (in sight) of self
=============
*/
BOOL CSpriteMonster :: InFront( CBaseEntity *pTarg )
{
	UTIL_MakeVectors (pev->angles);
	Vector dir = (pTarg->GetAbsOrigin() - GetAbsOrigin()).Normalize();

	float flDot = DotProduct(dir, gpGlobals->v_forward);

	if (flDot > 0.3)
	{
		return TRUE;
	}
	return FALSE;
}

/*
===========
FindTarget

Self is currently not attacking anything, so try to find a target

Returns TRUE if an enemy was sighted

When a player fires a missile, the point of impact becomes a fakeplayer so
that monsters that see the impact will respond as if they had seen the
player.

To avoid spending too much time, only a single client (or fakeclient) is
checked each frame.  This means multi player games will have slightly
slower noticing monsters.
============
*/
BOOL CSpriteMonster :: FindTarget( void )
{
	CBaseEntity *pTarget;
	RANGETYPE	range;

	if (m_flSightTime >= (gpGlobals->time - 0.1f))
	{
		pTarget = m_hSightEntity;
		if (FNullEnt( pTarget ) || (pTarget->pev->enemy == pev->enemy))
			return FALSE;
	}
	else
	{
		pTarget = UTIL_FindClientInPVS( ENT(pev) );
		if (FNullEnt( pTarget ))
			return FALSE; // current check entity isn't in PVS
	}

	if (pTarget->pev->health <= 0)
		return FALSE; // g-cont. target is died

	if (pTarget->edict() == pev->enemy)
		return FALSE;

	if (pTarget->pev->flags & FL_NOTARGET)
		return FALSE;

	range = TargetRange (pTarget);
	if (range == RANGE_FAR)
		return FALSE;
		
	if (!TargetVisible (pTarget))
		return FALSE;

	if (range == RANGE_NEAR)
	{
		if (pTarget->m_flShowHostile < gpGlobals->time && !InFront(pTarget) )
			return FALSE;
	}
	else if (range == RANGE_MID)
	{
		if ( !InFront(pTarget))
			return FALSE;
	}
	
	// got one
	pev->enemy = pTarget->edict();

	if (!FClassnameIs( pev->enemy, "player"))
	{
		pev->enemy = pev->enemy->v.enemy;
		if (!pev->enemy || !FClassnameIs( pev->enemy, "player"))
		{
			pev->enemy = NULL;
			return FALSE;
		}
	}

	FoundTarget ();

	return TRUE;
}

void CSpriteMonster :: FoundTarget( void )
{
	if (pev->enemy && FClassnameIs( pev->enemy, "player"))
	{	
		// let other monsters see this monster for a while
		m_hSightEntity = this;
		m_flSightTime = gpGlobals->time;
	}
	
	m_flShowHostile = gpGlobals->time + 1.0; // wake up other monsters

	MonsterSight ();	// monster see enemy!
	HuntTarget ();
}

void CSpriteMonster :: AttackFinished( float flFinishTime )
{
	m_iRefireCount = 0; // refire count for nightmare

	m_flAttackFinished = gpGlobals->time + flFinishTime;
}

void CSpriteMonster :: HuntTarget ( void )
{
	m_pGoalEnt = CBaseEntity::Instance(pev->enemy);
	m_iAIState = STATE_RUN;

	pev->ideal_yaw = UTIL_VecToYaw (pev->enemy->v.origin - GetAbsOrigin());

	SetThink( &CSpriteMonster::MonsterThink );
	SetNextThink( 0.1 );

	MonsterRun();	// change activity
	AttackFinished (1);	// wait a while before first attack
}

BOOL CSpriteMonster :: MonsterCheckAnyAttack( void )
{
	if (!m_fEnemyVisible)
		return FALSE;
	return MonsterCheckAttack();
}

void CSpriteMonster :: AI_Run( float flDist )
{
	Vector	delta;

	m_flMoveDistance = flDist;

	// see if the enemy is dead
	if (!pev->enemy || pev->enemy->v.health <= 0)
	{
		pev->enemy = NULL;

		// FIXME: look all around for other targets
		if (oldenemy != NULL && oldenemy->v.health > 0)
		{
			pev->enemy = oldenemy;
			HuntTarget ();
		}
		else
		{
			MonsterIdle();
			return;
		}
	}

	m_flShowHostile = gpGlobals->time + 1.0; // wake up other monsters
	CBaseEntity *pEnemy = CBaseEntity::Instance( pev->enemy );

	// check knowledge of enemy
	m_fEnemyVisible = TargetVisible(pEnemy);

	if (m_fEnemyVisible)
		m_flSearchTime = gpGlobals->time + 5.0;

	// look for other coop players
	if (gpGlobals->coop && m_flSearchTime < gpGlobals->time)
	{
		if (FindTarget ())
			return;
	}

	m_fEnemyInFront = InFront(pEnemy);
	m_iEnemyRange = TargetRange(pEnemy);
	m_flEnemyYaw = UTIL_VecToYaw(pEnemy->GetAbsOrigin() - GetAbsOrigin());
	
	if (m_iAttackState == ATTACK_MISSILE)
	{
//		AI_Run_Missile();
		return;
	}
	if (m_iAttackState == ATTACK_MELEE)
	{
//		AI_Run_Melee();
		return;
	}

	if (MonsterCheckAnyAttack ())
		return;					// beginning an attack
		
	// head straight in
//	MoveToGoal (flDist);	// done in C code...
}

void CSpriteMonster :: MonsterThink( void )
{
	if( !( m_iAIState >= STATE_KILLED && pev->frame >= anims[m_iAIState].y ))
	{
		// animate now
		pev->frame += 1.0f;	// 10 fps

		// animation hit ends
		if( pev->frame > anims[m_iAIState].y )
		{
			pev->frame = anims[m_iAIState].x;
			if( m_iAIState == STATE_PAIN )
				MonsterRun(); // change activity
		}
	}

	if( m_iAIState == STATE_IDLE )
	{
		FindTarget ();
	}
	else if( m_iAIState == STATE_RUN )
	{
		AI_Run( 8.0f );
	}
	else if( m_iAIState == STATE_ATTACK )
	{
//		MonsterAttack();
	}

	SetNextThink( 0.1f );
}

void CSpriteMonster :: MonsterIdle( void )
{
	m_iAIState = STATE_IDLE;
	pev->frame = anims[m_iAIState].x;
}

void CSpriteMonster :: MonsterSight( void )
{
}

void CSpriteMonster :: MonsterRun( void )
{
	m_iAIState = STATE_RUN;
	pev->frame = anims[m_iAIState].x;
}

BOOL CSpriteMonster :: MonsterCheckAttack( void )
{
	return FALSE;
}

void CSpriteMonster :: InitThink( void )
{
	if( !FBitSet( pev->spawnflags, SF_FLYING_MONSTER ))
	{
		pev->origin.z += 1;
		UTIL_DropToFloor ( this );

		if( !pev->yaw_speed )
			pev->yaw_speed = 20;
	}
	else
	{
		pev->flags |= (FL_FLY|FL_MONSTER);

		if( !pev->yaw_speed )
			pev->yaw_speed = 10;
	}

	pev->takedamage = DAMAGE_AIM;
	pev->ideal_yaw = pev->angles.y;
	pev->flags |= FL_MONSTER;

	pev->view_ofs = pev->maxs;
	MonsterIdle();

	// run AI for monster
	SetThink( &CSpriteMonster::MonsterThink );
	MonsterThink();
}

void CSpriteMonster::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
}
