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

===== bmodels.cpp ========================================================

  spawn, think, and use functions for entities that use brush models

*/
#include "pushablemaker.h"

LINK_ENTITY_TO_CLASS( pushablemaker, CPushableMaker );

BEGIN_DATADESC( CPushableMaker )
	DEFINE_KEYFIELD( m_cNumBoxes, FIELD_INTEGER, "boxcount" ),
	DEFINE_FIELD( m_cLiveBoxes, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iMaxLiveBoxes, FIELD_INTEGER, "m_imaxliveboxes" ),
	DEFINE_FIELD( m_flGround, FIELD_FLOAT ),
	DEFINE_FUNCTION( ToggleUse ),
	DEFINE_FUNCTION( CyclicUse ),
	DEFINE_FUNCTION( MakerThink ),
END_DATADESC()

void CPushableMaker :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "boxcount") )
	{
		m_cNumBoxes = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "m_imaxliveboxes") )
	{
		m_iMaxLiveBoxes = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPushable::KeyValue( pkvd );
}

void CPushableMaker :: Spawn( void )
{
	CPushable::Spawn();

	// make dormant
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetBits( pev->effects, EF_NODRAW );
	pev->takedamage = DAMAGE_NO;
	RelinkEntity();

	m_cLiveBoxes = 0;

	if ( !FStringNull ( pev->targetname ))
	{
		if( pev->spawnflags & SF_PUSHABLEMAKER_CYCLIC )
		{
			SetUse( &CPushableMaker::CyclicUse );	// drop one monster each time we fire
		}
		else
		{
			SetUse( &CPushableMaker::ToggleUse );	// so can be turned on/off
		}

		if( FBitSet ( pev->spawnflags, SF_PUSHABLEMAKER_START_ON ) )
		{
			// start making monsters as soon as monstermaker spawns
			m_iState = STATE_ON;
			SetThink( &CPushableMaker::MakerThink );
		}
		else
		{
			// wait to be activated.
			m_iState = STATE_OFF;
			SetThink( &CBaseEntity::SUB_DoNothing );
		}
	}
	else
	{
			// no targetname, just start.
			SetNextThink( m_flDelay );
			m_iState = STATE_ON;
			SetThink( &CPushableMaker::MakerThink );
	}

	m_flGround = 0;
}

//=========================================================
// CPushableMaker - this is the code that drops the box
//=========================================================
void CPushableMaker :: MakePushable( void )
{
	if ( m_iMaxLiveBoxes > 0 && m_cLiveBoxes >= m_iMaxLiveBoxes )
	{
		// not allowed to make a new one yet. Too many live ones out right now.
		return;
	}

	if ( !m_flGround || m_hParent != NULL )
	{
		// set altitude. Now that I'm activated, any breakables, etc should be out from under me. 
		TraceResult tr;

		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector ( 0, 0, 2048 ), ignore_monsters, edict(), &tr );
		m_flGround = tr.vecEndPos.z;
	}

	Vector mins = pev->absmin;
	Vector maxs = pev->absmax;
	maxs.z = GetAbsOrigin().z;
	mins.z = m_flGround;

	CBaseEntity *pList[2];
	int count = UTIL_EntitiesInBox( pList, 2, mins, maxs, 0 );
	if ( count )
	{
		for( int i = 0; i < count; i++ )
		{
			if( pList[i] == this )
				continue;

			if( pList[i]->IsPushable() || pList[i]->IsMonster() || pList[i]->IsPlayer())
			{
				return; // don't build a stack of boxes!
			}
		}
	}

	TraceResult trace;

	UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin(), &trace );

	if( trace.fStartSolid )
	{
		ALERT( at_aiconsole, "pushablemaker: couldn't spawn %s->%s\n", GetTargetname(), GetNetname());
		return;
	}
	
	CPushable *pBox = (CPushable *)CreateEntityByName( "func_pushable" );
	if( !pBox ) return;	// g-cont. ???
	
	// If I have a target, fire!
	if( !FStringNull( pev->target ))
	{
		// delay already overloaded for this entity, so can't call SUB_UseTargets()
		UTIL_FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
	}

	// set parms for our box
	pBox->pev->model = pev->model;
	pBox->pev->spawnflags = pev->spawnflags & ~(SF_PUSHABLEMAKER_START_ON|SF_PUSHABLEMAKER_CYCLIC);
	pBox->pev->renderfx = pev->renderfx;
	pBox->pev->rendermode = pev->rendermode;
	pBox->pev->renderamt = pev->renderamt;
	pBox->pev->rendercolor.x = pev->rendercolor.x;
	pBox->pev->rendercolor.y = pev->rendercolor.y;
	pBox->pev->rendercolor.z = pev->rendercolor.z;
	pBox->pev->friction = pev->friction;
	pBox->pev->effects = pev->effects & ~EF_NODRAW;
	pBox->pev->health = pev->health;
	pBox->pev->skin = pev->skin;
	pBox->m_Explosion = m_Explosion;
	pBox->m_Material = m_Material;
	pBox->m_iszGibModel = m_iszGibModel;
	pBox->m_iszSpawnObject = m_iszSpawnObject;
	pBox->ExplosionSetMagnitude( ExplosionMagnitude() );
	pBox->m_flDelay = m_flDelay;	// BUGBUG: delay already used. Try m_flWait instead?

	pBox->SetAbsAngles( GetAbsAngles() );
	pBox->SetAbsOrigin( GetAbsOrigin() );
	pBox->m_fSetAngles = TRUE;

	DispatchSpawn( pBox->edict() );
	pBox->pev->owner = edict();

	if ( !FStringNull( pev->netname ) )
	{
		// if I have a netname (overloaded), give the child monster that name as a targetname
		pBox->pev->targetname = pev->netname;
	}

	m_cLiveBoxes++;// count this box
	m_cNumBoxes--;

	if ( m_cNumBoxes == 0 )
	{
		// Disable this forever.  Don't kill it because it still gets death notices
		SetThink( NULL );
		SetUse( NULL );
	}
}

//=========================================================
// CyclicUse - drops one monster from the monstermaker
// each time we call this.
//=========================================================
void CPushableMaker::CyclicUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	MakePushable();
}

//=========================================================
// ToggleUse - activates/deactivates the monster maker
//=========================================================
void CPushableMaker :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType ))
		return;

	if ( GetState() == STATE_ON )
	{
		m_iState = STATE_OFF;
		SetThink( NULL );
	}
	else
	{
		m_iState = STATE_ON;
		SetThink( &CPushableMaker::MakerThink );
	}

	SetNextThink( 0 );
}

void CPushableMaker :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// HACKHACK: we can't using callbacks because CBreakable override use function
	if( m_pfnUse ) (this->*m_pfnUse)( pActivator, pCaller, useType, value );
}

//=========================================================
// MakerThink - creates a new monster every so often
//=========================================================
void CPushableMaker :: MakerThink ( void )
{
	SetNextThink( m_flDelay );
	MakePushable();
}

//=========================================================
//=========================================================
void CPushableMaker :: DeathNotice ( entvars_t *pevChild )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveBoxes--;
	pevChild->owner = NULL;
}
