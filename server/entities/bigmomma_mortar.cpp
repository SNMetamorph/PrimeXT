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
// Mortar shot entity
//=========================================================
#include	"bigmomma_mortar.h"

extern int			   gSpitSprite;

LINK_ENTITY_TO_CLASS( bmortar, CBMortar );

BEGIN_DATADESC( CBMortar )
	DEFINE_FIELD( m_maxFrame, FIELD_INTEGER ),
	DEFINE_FUNCTION( Animate ),
END_DATADESC()

void MortarSpray(const Vector &position, const Vector &direction, int spriteModel, int count)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, position);
	WRITE_BYTE(TE_SPRITE_SPRAY);
	WRITE_COORD(position.x);	// pos
	WRITE_COORD(position.y);
	WRITE_COORD(position.z);
	WRITE_COORD(direction.x);	// dir
	WRITE_COORD(direction.y);
	WRITE_COORD(direction.z);
	WRITE_SHORT(spriteModel);	// model
	WRITE_BYTE(count);			// count
	WRITE_BYTE(130);			// speed
	WRITE_BYTE(80);			// noise ( client will divide by 100 )
	MESSAGE_END();
}

// ---------------------------------
//
// Mortar
//
// ---------------------------------

// UNDONE: right now this is pretty much a copy of the squid spit with minor changes to the way it does damage
void CBMortar:: Spawn( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->classname = MAKE_STRING( "bmortar" );
	
	pev->solid = SOLID_BBOX;
	pev->rendermode = kRenderTransAlpha;
	pev->renderamt = 255;

	SET_MODEL(ENT(pev), "sprites/mommaspit.spr");
	pev->frame = 0;
	pev->scale = 0.5;

	UTIL_SetSize( pev, Vector( 0, 0, 0), Vector(0, 0, 0) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	pev->dmgtime = gpGlobals->time + 0.4;
}

void CBMortar::Animate( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( gpGlobals->time > pev->dmgtime )
	{
		pev->dmgtime = gpGlobals->time + 0.2;
		MortarSpray( GetAbsOrigin(), -GetAbsVelocity().Normalize(), gSpitSprite, 3 );
	}
	if ( pev->frame++ )
	{
		if ( pev->frame > m_maxFrame )
		{
			pev->frame = 0;
		}
	}
}

CBMortar *CBMortar::Shoot( edict_t *pOwner, Vector vecStart, Vector vecVelocity )
{
	CBMortar *pSpit = GetClassPtr( (CBMortar *)NULL );
	pSpit->Spawn();
	
	UTIL_SetOrigin( pSpit, vecStart );
	pSpit->SetAbsVelocity( vecVelocity );
	pSpit->pev->owner = pOwner;
	pSpit->pev->scale = 2.5;
	pSpit->SetThink( &CBMortar::Animate );
	pSpit->pev->nextthink = gpGlobals->time + 0.1;

	return pSpit;
}


void CBMortar::Touch( CBaseEntity *pOther )
{
	TraceResult tr;
	int		iPitch;

	// splat sound
	iPitch = RANDOM_FLOAT( 90, 110 );

	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "bullchicken/bc_acid1.wav", 1, ATTN_NORM, 0, iPitch );	

	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "bullchicken/bc_spithit1.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "bullchicken/bc_spithit2.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	}

	if ( pOther->IsBSPModel() )
	{

		// make a splat on the wall
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + GetAbsVelocity() * 10, dont_ignore_monsters, edict(), &tr );
		UTIL_DecalTrace(&tr, "{mommablob");
	}
	else
	{
		tr.vecEndPos = GetAbsOrigin();
		tr.vecPlaneNormal = -GetAbsVelocity().Normalize();
	}

	// make some flecks
	MortarSpray( tr.vecEndPos, tr.vecPlaneNormal, gSpitSprite, 24 );

	entvars_t *pevOwner = NULL;
	if ( pev->owner )
		pevOwner = VARS(pev->owner);

	RadiusDamage( GetAbsOrigin(), pev, pevOwner, gSkillData.bigmommaDmgBlast, gSkillData.bigmommaRadiusBlast, CLASS_NONE, DMG_ACID );
	UTIL_Remove( this );
}