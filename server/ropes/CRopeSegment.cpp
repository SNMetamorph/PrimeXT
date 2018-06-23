/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "CRope.h"
#include "CRopeSegment.h"

BEGIN_DATADESC( CRopeSegment )
	DEFINE_FIELD( m_iszModelName, FIELD_STRING ),
	DEFINE_FIELD( m_bCauseDamage, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bCanBeGrabbed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_Data.restLength, FIELD_FLOAT ),
	DEFINE_FIELD( m_Data.mPosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_Data.mVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_Data.mForce, FIELD_VECTOR ),
	DEFINE_FIELD( m_Data.mExternalForce, FIELD_VECTOR ),
	DEFINE_FIELD( m_Data.mApplyExternalForce, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_Data.mMassReciprocal, FIELD_FLOAT ),
	DEFINE_FIELD( m_pMasterRope, FIELD_CLASSPTR ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( rope_segment, CRopeSegment );

CRopeSegment :: CRopeSegment()
{
	m_iszModelName = MAKE_STRING( "models/rope16.mdl" );
}

void CRopeSegment :: Precache()
{
	BaseClass::Precache();

	PRECACHE_MODEL( STRING( m_iszModelName ));
	PRECACHE_SOUND( "items/grab_rope.wav" );
}

void CRopeSegment :: Spawn( void )
{
	Precache();

	SET_MODEL( edict(), STRING( m_iszModelName ));

	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_TRIGGER;
	RelinkEntity( FALSE );

	if( m_iszModelName != NULL_STRING )
	{
		if( UTIL_GetModelType( pev->modelindex ) == mod_studio )
		{
			Vector origin, angles;
			GetAttachment( 0, origin, angles );
			m_Data.restLength = ( origin - GetAbsOrigin()).Length();
		}
		else
		{
			// debug, default
			m_Data.restLength = 15.0f;
		}
	}

	// use head_hull
	UTIL_SetSize( pev, Vector( -16, -16, -18 ), Vector( 16, 16, 18 ));

	SetNextThink( 0.5 );
}

void CRopeSegment :: Touch( CBaseEntity* pOther )
{
	if( pOther->IsPlayer() )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)pOther;

		// Electrified wires deal damage. - Solokiller
		if( m_bCauseDamage )
		{
			if( gpGlobals->time >= pev->dmgtime )
			{
				if( pev->dmg < 0 ) pOther->TakeHealth( -pev->dmg, DMG_GENERIC );
				else pOther->TakeDamage( pev, pev, pev->dmg, DMG_SHOCK );
				pev->dmgtime = gpGlobals->time + 0.5f;
			}
		}

		if( GetMasterRope()->IsAcceptingAttachment() && !pPlayer->IsOnRope( ))
		{
			if( m_bCanBeGrabbed )
			{
				pOther->SetAbsOrigin( m_Data.mPosition );

				pPlayer->SetOnRopeState( true );
				pPlayer->SetRope( GetMasterRope() );
				GetMasterRope()->AttachObjectToSegment( this );

				const Vector& vecVelocity = pOther->GetAbsVelocity();

				if( vecVelocity.Length() > 0.5 )
				{
					// Apply some external force to move the rope. - Solokiller
					m_Data.mApplyExternalForce = true;
					m_Data.mExternalForce = m_Data.mExternalForce + vecVelocity * 750;
				}

				if( GetMasterRope()->IsSoundAllowed( ))
				{
					EMIT_SOUND( edict(), CHAN_BODY, "items/grab_rope.wav", 1.0, ATTN_NORM );
				}
			}
			else
			{
				// this segment cannot be grabbed, so grab the highest one if possible. - Solokiller
				CRope *pRope = GetMasterRope();

				CRopeSegment* pSegment;

				if( pRope->GetNumSegments() <= 4 )
				{
					// fewer than 5 segments exist, so allow grabbing the last one. - Solokiller
					pSegment = pRope->GetSegments()[pRope->GetNumSegments() - 1];
					pSegment->SetCanBeGrabbed( true );
				}
				else
				{
					pSegment = pRope->GetSegments()[4];
				}

				pSegment->Touch( pOther );
			}
		}
	}
}

CRopeSegment* CRopeSegment :: CreateSegment( string_t iszModelName, float flRecipMass )
{
	CRopeSegment *pSegment = GetClassPtr(( CRopeSegment *)NULL );

	pSegment->SetClassname( "rope_segment" );
	pSegment->m_iszModelName = iszModelName;
	pSegment->Spawn();

	pSegment->m_bCauseDamage = false;
	pSegment->m_bCanBeGrabbed = true;
	pSegment->m_Data.mMassReciprocal = flRecipMass;

	return pSegment;
}

void CRopeSegment :: SetMasterRope( CRope* pRope )
{
	m_pMasterRope = pRope;

	pev->renderfx = pRope->pev->renderfx;
	pev->rendermode = pRope->pev->rendermode;
	pev->rendercolor = pRope->pev->rendercolor;
	pev->renderamt = pRope->pev->renderamt;
	pev->scale = pRope->pev->scale;
	pev->dmg = pRope->pev->dmg;
}

void CRopeSegment :: ApplyExternalForce( const Vector& vecForce )
{
	m_Data.mApplyExternalForce = true;
	m_Data.mExternalForce += vecForce;
}

void CRopeSegment :: SetCauseDamageOnTouch( const bool bCauseDamage )
{
	m_bCauseDamage = bCauseDamage;
}

void CRopeSegment :: SetCanBeGrabbed( const bool bCanBeGrabbed )
{
	m_bCanBeGrabbed = bCanBeGrabbed;
}
