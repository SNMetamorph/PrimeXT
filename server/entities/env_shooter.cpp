/*
env_shooter.cpp - improved version of gibshooter with more features
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

#include "env_shooter.h"
#include "monsters.h"
#include "shot.h"
#include "func_break.h"

BEGIN_DATADESC( CEnvShooter )
	DEFINE_KEYFIELD( m_iszTouch, FIELD_STRING, "m_iszTouch" ),
	DEFINE_KEYFIELD( m_iszTouchOther, FIELD_STRING, "m_iszTouchOther" ),
	DEFINE_KEYFIELD( m_iPhysics, FIELD_INTEGER, "m_iPhysics" ),
	DEFINE_KEYFIELD( m_fFriction, FIELD_FLOAT, "m_fFriction" ),
	DEFINE_KEYFIELD( m_vecSize, FIELD_VECTOR, "m_vecSize" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_shooter, CEnvShooter );

void CEnvShooter :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "shootmodel"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shootsounds"))
	{
		int iNoise = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		switch( iNoise )
		{
		case 0:
			m_iGibMaterial = matGlass;
			break;
		case 1:
			m_iGibMaterial = matWood;
			break;
		case 2:
			m_iGibMaterial = matMetal;
			break;
		case 3:
			m_iGibMaterial = matFlesh;
			break;
		case 4:
			m_iGibMaterial = matRocks;
			break;
		default:
		case -1:
			m_iGibMaterial = matNone;
			break;
		}
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouch"))
	{
		m_iszTouch = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouchOther"))
	{
		m_iszTouchOther = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPhysics"))
	{
		m_iPhysics = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fFriction"))
	{
		m_fFriction = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_vecSize"))
	{
		UTIL_StringToVector( m_vecSize, pkvd->szValue);
		m_vecSize = m_vecSize / 2;
		pkvd->fHandled = TRUE;
	}
	else
	{
		CGibShooter::KeyValue( pkvd );
	}
}

void CEnvShooter :: Precache ( void )
{
	m_iGibModelIndex = PRECACHE_MODEL( GetModel() );

	CBreakable::MaterialSoundPrecache( (Materials)m_iGibMaterial );
}

void CEnvShooter::Spawn( void )
{
	int iBody = pev->body;
	BaseClass::Spawn();
	pev->body = iBody;
}

CBaseEntity *CEnvShooter :: CreateGib( void )
{
	if( m_iPhysics <= 1 )
	{
		// normal gib or sticky gib
		CGib *pGib = GetClassPtr( (CGib *)NULL );

		pGib->Spawn( GetModel() );

		if( m_iPhysics == 1 )
		{
			// sticky gib
			pGib->pev->movetype = MOVETYPE_TOSS;
			pGib->pev->solid = SOLID_BBOX;
			pGib->SetTouch( &CGib :: StickyGibTouch );
		}

		if( m_iBloodColor )
			pGib->m_bloodColor = m_iBloodColor;
		else pGib->m_bloodColor = DONT_BLEED;
		pGib->m_material = m_iGibMaterial;

		pGib->pev->rendermode = pev->rendermode;
		pGib->pev->renderamt = pev->renderamt;
		pGib->pev->rendercolor = pev->rendercolor;
		pGib->pev->renderfx = pev->renderfx;
		pGib->pev->effects = pev->effects & ~EF_NODRAW;
		pGib->pev->scale = pev->scale;
		pGib->pev->skin = pev->skin;

		// g-cont. random body select
		if( pGib->pev->body == -1 )
		{
			int numBodies = MODEL_FRAMES( pGib->pev->modelindex );
			pGib->pev->body = RANDOM_LONG( 0, numBodies - 1 );
		}
		else if( pev->body > 0 )
			pGib->pev->body = RANDOM_LONG( 0, pev->body - 1 );

		// g-cont. random skin select
		if( pGib->pev->skin == -1 )
		{
			studiohdr_t *pstudiohdr = (studiohdr_t *)(GET_MODEL_PTR( ENT(pGib->pev) ));

			// NOTE: this code works fine only under XashXT because GoldSource doesn't merge texture and model files
			// into one model_t slot. So this will not working with models which keep textures seperate
			if( pstudiohdr && pstudiohdr->numskinfamilies > 0 )
			{
				pGib->pev->skin = RANDOM_LONG( 0, pstudiohdr->numskinfamilies - 1 );
			}
			else pGib->pev->skin = 0; // reset to default
		}

		return pGib;
	}
          
	// special shot
	CShot *pShot = GetClassPtr( (CShot*)NULL );
	pShot->pev->classname = MAKE_STRING( "shot" );
	pShot->pev->solid = SOLID_SLIDEBOX;
	pShot->SetLocalAvelocity( GetLocalAvelocity( ));
	SET_MODEL( ENT( pShot->pev ), STRING( pev->model ));
	UTIL_SetSize( pShot->pev, -m_vecSize, m_vecSize );
	pShot->pev->renderamt = pev->renderamt;
	pShot->pev->rendermode = pev->rendermode;
	pShot->pev->rendercolor = pev->rendercolor;
	pShot->pev->renderfx = pev->renderfx;
	pShot->pev->netname = m_iszTouch;
	pShot->pev->message = m_iszTouchOther;
	pShot->pev->effects = pev->effects & ~EF_NODRAW;
	pShot->pev->skin = pev->skin;
	pShot->pev->body = pev->body;
	pShot->pev->scale = pev->scale;
	pShot->pev->frame = pev->frame;
	pShot->pev->framerate = pev->framerate;
	pShot->pev->friction = m_fFriction;

	// g-cont. random body select
	if( pShot->pev->body == -1 )
	{
		int numBodies = MODEL_FRAMES( pShot->pev->modelindex );
		pShot->pev->body = RANDOM_LONG( 0, numBodies - 1 );
	}
	else if( pev->body > 0 )
		pShot->pev->body = RANDOM_LONG( 0, pev->body - 1 );

	// g-cont. random skin select
	if( pShot->pev->skin == -1 )
	{
		studiohdr_t *pstudiohdr = (studiohdr_t *)(GET_MODEL_PTR( ENT(pShot->pev) ));

		// NOTE: this code works fine only under XashXT because GoldSource doesn't merge texture and model files
		// into one model_t slot. So this will not working with models which keep textures seperate
		if( pstudiohdr && pstudiohdr->numskinfamilies > 0 )
		{
			pShot->pev->skin = RANDOM_LONG( 0, pstudiohdr->numskinfamilies - 1 );
		}
		else pShot->pev->skin = 0; // reset to default
	}

	switch( m_iPhysics )
	{
	case 2:	
		pShot->pev->movetype = MOVETYPE_NOCLIP;
		pShot->pev->solid = SOLID_NOT;
		break;
	case 3:
		pShot->pev->movetype = MOVETYPE_FLYMISSILE;
		break;
	case 4:
		pShot->pev->movetype = MOVETYPE_BOUNCEMISSILE;
		break;
	case 5:
		pShot->pev->movetype = MOVETYPE_TOSS;
		break;
	case 6:
		// FIXME: tune NxGroupMask for each body to avoid collision with this particles
		// because so small bodies can be pushed through floor
		pShot->pev->solid = SOLID_NOT;
		if( WorldPhysic->Initialized( ))
			pShot->pev->movetype = MOVETYPE_PHYSIC;
		pShot->SetAbsOrigin( GetAbsOrigin( ));
		pShot->m_pUserData = WorldPhysic->CreateBodyFromEntity( pShot );
		break;
	default:
		pShot->pev->movetype = MOVETYPE_BOUNCE;
		break;
	}

	if( pShot->pev->framerate && UTIL_GetModelType( pShot->pev->modelindex ) == mod_sprite )
	{
		pShot->m_maxFrame = (float)MODEL_FRAMES( pShot->pev->modelindex ) - 1;
		if( pShot->m_maxFrame > 1.0f )
		{
			if( m_flGibLife )
			{
				pShot->pev->dmgtime = gpGlobals->time + m_flGibLife;
				pShot->SetThink( &CSprite::AnimateUntilDead );
			}
			else
			{
				pShot->SetThink( &CSprite::AnimateThink );
			}

			pShot->SetNextThink( 0 );
			pShot->m_lastTime = gpGlobals->time;
			return pShot;
		}
	}

	// if it's not animating
	if( m_flGibLife )
	{
		pShot->SetThink( &CBaseEntity::SUB_Remove );
		pShot->SetNextThink( m_flGibLife );
	}

	return pShot;
}
