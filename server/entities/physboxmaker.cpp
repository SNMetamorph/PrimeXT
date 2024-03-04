/*
physboxmaker.cpp - simple entity for spawning env_physbox entities
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "physboxmaker.h"

LINK_ENTITY_TO_CLASS( physboxmaker, CPhysBoxMaker );
LINK_ENTITY_TO_CLASS( physenvmaker, CPhysBoxMaker );	// just an alias

BEGIN_DATADESC( CPhysBoxMaker )
	DEFINE_KEYFIELD( m_cNumBoxes, FIELD_INTEGER, "boxcount" ),
	DEFINE_FIELD( m_cLiveBoxes, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iMaxLiveBoxes, FIELD_INTEGER, "m_imaxliveboxes" ),
	DEFINE_FIELD( m_flGround, FIELD_FLOAT ),
	DEFINE_FUNCTION( ToggleUse ),
	DEFINE_FUNCTION( CyclicUse ),
	DEFINE_FUNCTION( MakerThink ),
END_DATADESC()

void CPhysBoxMaker :: KeyValue( KeyValueData *pkvd )
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
		CPhysEntity::KeyValue( pkvd );
}

void CPhysBoxMaker :: Spawn( void )
{
	SET_MODEL( edict(), GetModel( ));
	UTIL_SetOrigin( this, GetLocalOrigin() );

	// make dormant
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	SetBits( pev->effects, EF_NODRAW );
	pev->takedamage = DAMAGE_NO;
	RelinkEntity();

	m_cLiveBoxes = 0;

	if ( !FStringNull ( pev->targetname ))
	{
		if( pev->spawnflags & SF_PHYSBOXMAKER_CYCLIC )
		{
			SetUse( &CPhysBoxMaker::CyclicUse );	// drop one monster each time we fire
		}
		else
		{
			SetUse( &CPhysBoxMaker::ToggleUse );	// so can be turned on/off
		}

		if( FBitSet ( pev->spawnflags, SF_PHYSBOXMAKER_START_ON ) )
		{
			// start making monsters as soon as monstermaker spawns
			m_iState = STATE_ON;
			SetThink( &CPhysBoxMaker::MakerThink );
		}
		else
		{
			// wait to be activated.
			m_iState = STATE_OFF;
			SetThink( &CPhysBoxMaker::SUB_DoNothing );
		}
	}
	else
	{
			// no targetname, just start.
			SetNextThink( m_flDelay );
			m_iState = STATE_ON;
			SetThink( &CPhysBoxMaker::MakerThink );
	}

	m_flGround = 0;
}

//=========================================================
// CPhysBoxMaker - this is the code that drops the box
//=========================================================
void CPhysBoxMaker :: MakePhysBox( void )
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

			if( pList[i]->IsPushable() || pList[i]->IsMonster() || pList[i]->IsPlayer() || pList[i]->IsRigidBody( ))
			{
				return; // don't build a stack of boxes!
			}
		}
	}

	CPhysEntity *pBox = GetClassPtr( (CPhysEntity *)NULL );
	if( !pBox ) return;	// g-cont. ???
	
	// If I have a target, fire!
	if( !FStringNull( pev->target ))
	{
		// delay already overloaded for this entity, so can't call SUB_UseTargets()
		UTIL_FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
	}

	// set parms for our box
	pBox->pev->model = pev->model;
	pBox->pev->classname = MAKE_STRING( "func_physbox" );
	pBox->pev->spawnflags = pev->spawnflags & ~(SF_PHYSBOXMAKER_START_ON|SF_PHYSBOXMAKER_CYCLIC);
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
	pBox->m_Material = m_Material;
	pBox->m_iszGibModel = m_iszGibModel;
	pBox->m_flDelay = m_flDelay;	// BUGBUG: delay already used. Try m_flWait instead?

	pBox->SetAbsOrigin( GetAbsOrigin() );
	pBox->SetAbsAngles( GetAbsAngles() );
	pBox->m_fSetAngles = TRUE;
	DispatchSpawn( pBox->edict( ));
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
void CPhysBoxMaker::CyclicUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	MakePhysBox();
}

//=========================================================
// ToggleUse - activates/deactivates the monster maker
//=========================================================
void CPhysBoxMaker :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
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
		SetThink( &CPhysBoxMaker::MakerThink );
	}

	SetNextThink( 0 );
}

void CPhysBoxMaker :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// HACKHACK: we can't using callbacks because CBreakable override use function
	if( m_pfnUse ) (this->*m_pfnUse)( pActivator, pCaller, useType, value );
}

//=========================================================
// MakerThink - creates a new monster every so often
//=========================================================
void CPhysBoxMaker :: MakerThink ( void )
{
	SetNextThink( m_flDelay );
	MakePhysBox();
}

//=========================================================
//=========================================================
void CPhysBoxMaker :: DeathNotice ( entvars_t *pevChild )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveBoxes--;
	pevChild->owner = NULL;
}
