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
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include  <assert.h>
#include  <utlarray.h>
#include	"saverestore.h"
#include	"client.h"
#include	"decals.h"
#include	"nodes.h"
#include	"gamerules.h"
#include	"weapons.h"
#include	"game.h"
#include "user_messages.h"
#include "beam.h"

//#define USE_ENGINE_TOUCH_TRIGGERS

extern DLL_GLOBAL Vector		g_vecAttackDir;
extern CGraph			WorldGraph;
static CUtlArray<CBaseEntity *>	g_TeleportStack;

CBaseEntity::CBaseEntity() :
	m_pUserData(nullptr)
{
}

void CBaseEntity :: DontThink( void )
{
	pev->nextthink = 0;
}

void CBaseEntity :: SetNextThink( float delay )
{
	pev->nextthink = gpGlobals->time + delay;
}

//-----------------------------------------------------------------------------
// methods to acess of local physic params
//-----------------------------------------------------------------------------
const Vector& CBaseEntity :: GetLocalOrigin( void ) const
{
	return m_vecOrigin;
}

const Vector& CBaseEntity :: GetLocalAngles( void ) const
{
	return m_vecAngles;
}

const Vector &CBaseEntity::GetLocalVelocity( void ) const
{
	return m_vecVelocity;
}

const Vector &CBaseEntity::GetLocalAvelocity( void ) const
{
	return m_vecAvelocity;
}

//-----------------------------------------------------------------------------
// methods to acess of global physic params
//-----------------------------------------------------------------------------
const Vector& CBaseEntity :: GetAbsOrigin( void ) const
{
	if( pev->flags & FL_ABSTRANSFORM )
	{
		const_cast<CBaseEntity*>(this)->CalcAbsolutePosition();
	}

	return pev->origin;
}

const Vector& CBaseEntity :: GetAbsAngles( void ) const
{
	if( pev->flags & FL_ABSTRANSFORM )
	{
		const_cast<CBaseEntity*>(this)->CalcAbsolutePosition();
	}

	return pev->angles;
}

const Vector &CBaseEntity :: GetAbsVelocity( void ) const
{
	if( pev->flags & FL_ABSVELOCITY )
	{
		const_cast<CBaseEntity*>(this)->CalcAbsoluteVelocity();
	}

	return pev->velocity;
}

const Vector& CBaseEntity :: GetAbsAvelocity( void ) const
{
	if( pev->flags & FL_ABSAVELOCITY )
	{
		const_cast<CBaseEntity*>(this)->CalcAbsoluteAvelocity();
	}

	return pev->avelocity;
}

void CBaseEntity :: CheckAngles( void )
{
	if( pev->angles == g_vecZero || UTIL_CanRotate( this ))
		return;

	if( pev->movetype == MOVETYPE_PUSH )
		ALERT( at_error, "Entity %s [%i] with model %s missed 'origin' brush. Rotation ignored\n", GetClassname(), entindex(), GetModel());

	pev->angles = g_vecZero;
}

//-----------------------------------------------------------------------------
// These methods recompute local versions as well as set abs versions
//-----------------------------------------------------------------------------
void CBaseEntity::SetAbsOrigin( const Vector& absOrigin )
{
	// this is necessary to get the other fields of m_local
	CalcAbsolutePosition();

	// all children are invalid, but we are not
	InvalidatePhysicsState( FL_ABSTRANSFORM );
	ClearBits( pev->flags, FL_ABSTRANSFORM );

	pev->origin = pev->oldorigin = absOrigin;
	m_local.SetOrigin( absOrigin );

	CBaseEntity *pParent = m_hParent;
	Vector vecNewOrigin;

	if( !pParent )
	{
		vecNewOrigin = absOrigin;
	}
	else
	{
		matrix4x4 parentSpace = GetParentToWorldTransform();

		// moveparent case: transform the abs position into local space
		vecNewOrigin = parentSpace.VectorITransform( absOrigin );
	}

	m_vecOrigin = vecNewOrigin;
}

void CBaseEntity::SetAbsAngles( const Vector& absAngles )
{
	// this is necessary to get the other fields of m_local
	CalcAbsolutePosition();

	// all children are invalid, but we are not
	// this will cause the velocities of all children to need recomputation
	InvalidatePhysicsState( FL_ABSTRANSFORM, FL_ABSVELOCITY|FL_ABSAVELOCITY );
	ClearBits( pev->flags, FL_ABSTRANSFORM );

	pev->angles = absAngles;

	CheckAngles();

	m_local = matrix4x4( pev->origin, pev->angles, 1.0f );

	CBaseEntity *pParent = m_hParent;
	Vector vecNewAngles;

	if( !pParent )
	{
		vecNewAngles = absAngles;
	}
	else
	{
		// moveparent case: transform the abs transform into local space
		matrix4x4 iparentSpace = pParent->EntityToWorldTransform().Invert();
		matrix4x4 local = iparentSpace.ConcatTransforms( m_local );
		local.GetAngles( vecNewAngles );
	}

	m_vecAngles = vecNewAngles;
}

void CBaseEntity::SetAbsVelocity( const Vector &vecAbsVelocity )
{
	// the abs velocity won't be dirty since we're setting it here
	// all children are invalid, but we are not
	InvalidatePhysicsState( FL_ABSVELOCITY );
	ClearBits( pev->flags, FL_ABSVELOCITY );

	pev->velocity = vecAbsVelocity;

	CBaseEntity *pParent = m_hParent;

	if( !pParent )
	{
		m_vecVelocity = vecAbsVelocity;
		return;
	}

	// first subtract out the parent's abs velocity to get a relative
	// velocity measured in world space
	Vector relVelocity = vecAbsVelocity - pParent->GetAbsVelocity();

	// transform relative velocity into parent space
	m_vecVelocity = pParent->EntityToWorldTransform().VectorIRotate( relVelocity );
}

void CBaseEntity :: SetAbsAvelocity( const Vector &vecAbsAvelocity )
{
	// the abs velocity won't be dirty since we're setting it here
	// all children are invalid, but we are not
	InvalidatePhysicsState( FL_ABSAVELOCITY );
	ClearBits( pev->flags, FL_ABSAVELOCITY );

	pev->avelocity = vecAbsAvelocity;

	CBaseEntity *pParent = m_hParent;

	if( !pParent )
	{
		m_vecAvelocity = vecAbsAvelocity;
		return;
	}

	// DAMN! we can't transform avelocity because it's not angle and not vector!!!
	// i've just leave unchanged
#if 0
	// this transforms the local ang velocity into world space
	matrix4x4	avel = matrix4x4( g_vecZero, vecAbsAvelocity );
	matrix4x4 iparentSpace = pParent->GetParentToWorldTransform().Invert();
	matrix4x4 worldAvel = iparentSpace.ConcatTransforms( avel );
	worldAvel.GetAngles( pev->avelocity );	// but.. this is no more avelocity? this is some angles

	// g-cont. trying to get avelocity length, normalize, rotate by parent space and apply length again?
#endif
}

//-----------------------------------------------------------------------------
// Invalidates the abs state of all children
//-----------------------------------------------------------------------------
void CBaseEntity::InvalidatePhysicsState( int flags, int childflags )
{
	pev->flags |= flags;
	flags |= childflags;

	for( CBaseEntity *pChild = m_hChild; pChild != NULL; pChild = pChild->m_hNextChild )
	{
		pChild->InvalidatePhysicsState( flags );
	}
}

//-----------------------------------------------------------------------------
// entity transforms from local to world and back
//-----------------------------------------------------------------------------
matrix4x4 CBaseEntity :: GetParentToWorldTransform( void )
{
	CBaseEntity *pParent = m_hParent;

	matrix4x4	temp;

	if( !pParent )
	{
		temp.Identity();
		return temp;
	}

#if 0	// UNDONE
	if( m_iParentAttachment != 0 )
	{
		Vector vOrigin, vAngles;

		CBaseAnimating *pAnimating = pParent->GetBaseAnimating();
		if( pAnimating && pAnimating->GetAttachment( m_iParentAttachment, vOrigin, vAngles ))
		{
			// NOTE: we need to set sv_allow_studio_attachment_angles to 1
			// if we want give valid attachment angles here
			temp = matrix4x4( vOrigin, vAngles );
			return temp;
		}
	}
#endif	
	// allow to attach at end of the laser beam
	if( pParent->pev->flags & FL_CUSTOMENTITY )
	{
		CBeam *pBeam = (CBeam *)pParent;
		Vector beamDir = (pBeam->GetAbsEndPos() - pBeam->GetAbsStartPos()).Normalize();
		return matrix4x4( pBeam->GetAbsEndPos() + beamDir * -8.0f, g_vecZero );
	}

	// TODO: do revision, some cases may be wrong
	if( m_iParentFlags != 0 )
	{
		matrix4x4	m = pParent->EntityToWorldTransform();

		// this an incredible stupid way but...
		Vector vecAngles, vecOrigin;

		m.GetOrigin( vecOrigin );
		m.GetAngles( vecAngles );

		if( m_iParentFlags & PARENT_FROZEN_POS_X )
			vecOrigin.x = m_vecOrigin.x;

		if( m_iParentFlags & PARENT_FROZEN_POS_Y )
			vecOrigin.y = m_vecOrigin.y;

		if( m_iParentFlags & PARENT_FROZEN_POS_Z )
			vecOrigin.z = m_vecOrigin.z;

		if( m_iParentFlags & PARENT_FROZEN_ROT_X )
			vecAngles.x = m_vecAngles.x;

		if( m_iParentFlags & PARENT_FROZEN_ROT_Y )
			vecAngles.y = m_vecAngles.y;

		if( m_iParentFlags & PARENT_FROZEN_ROT_Z )
			vecAngles.z = m_vecAngles.z;

		return matrix4x4( vecOrigin, vecAngles, 1.0f );
	}

	// If we fall through to here, then just use the move parent's abs origin and angles.
	return pParent->EntityToWorldTransform();
}

//-----------------------------------------------------------------------------
// Purpose: Calculates the absolute position of an edict in the world
// assumes the parent's absolute origin has already been calculated
//-----------------------------------------------------------------------------
void CBaseEntity::CalcAbsolutePosition( void )
{
	if( !FBitSet( pev->flags, FL_ABSTRANSFORM ))
		return;

	ClearBits( pev->flags, FL_ABSTRANSFORM ); // waiting for next invalidate

	// plop the entity->parent matrix into m_local
	m_local = matrix4x4( m_vecOrigin, m_vecAngles, 1.0f );

	CBaseEntity *pParent = m_hParent;

	if( !pParent )
	{
		// no move parent, so just copy existing values
		pev->origin = pev->oldorigin = m_vecOrigin;
		pev->angles = m_vecAngles;

		CheckAngles();
		return;
	}

	// concatenate with our parent's transform
	matrix4x4	parentSpace = GetParentToWorldTransform();

	// g-cont. probably our local matrix is now contain world orientation
	// and keep this state until next call of SetLocalAngles or SetLocalOrigin
	m_local = parentSpace.ConcatTransforms( m_local );

	// pull our absolute position out of the matrix
	m_local.GetOrigin( pev->origin );

	// if we have any angles, we have to extract our absolute angles from our matrix
	if( m_vecAngles != g_vecZero )
		m_local.GetAngles( pev->angles );
	else if( pParent->IsPlayer() )
		pev->angles = Vector( 0.0f, pParent->pev->angles.y, 0.0f );
	else // just copy our parent's absolute angles
		pev->angles = pParent->GetAbsAngles();

	CheckAngles();
}

void CBaseEntity :: CalcAbsoluteVelocity( void )
{
	if( !FBitSet( pev->flags, FL_ABSVELOCITY ))
		return;

	ClearBits( pev->flags, FL_ABSVELOCITY ); // waiting for next invalidate

	CBaseEntity *pParent = m_hParent;

	if( !pParent )
	{
		pev->velocity = m_vecVelocity;
		return;
	}

	// this transforms the local velocity into world space
	pev->velocity = pParent->EntityToWorldTransform().VectorRotate( m_vecVelocity );

	// now add in the parent abs velocity
	pev->velocity += pParent->GetAbsVelocity();
}

// FIXME: While we're using (dPitch, dYaw, dRoll) as our local angular velocity
// representation, we can't actually solve this problem
void CBaseEntity :: CalcAbsoluteAvelocity( void )
{
	if( !FBitSet( pev->flags, FL_ABSAVELOCITY ))
		return;

	ClearBits( pev->flags, FL_ABSAVELOCITY ); // waiting for next invalidate

	CBaseEntity *pParent = m_hParent;

	if( !pParent )
	{
		pev->avelocity = m_vecAvelocity;
		return;
	}

	// DAMN! we can't transform avelocity because it's not angle and not vector!!!
	// i've just leave unchanged
#if 0
	// this transforms the local ang velocity into world space
	matrix4x4	avel = matrix4x4( g_vecZero, m_vecAvelocity );
	matrix4x4 parentSpace = pParent->GetParentToWorldTransform();
	matrix4x4 worldAvel = parentSpace.ConcatTransforms( avel );
	worldAvel.GetAngles( pev->avelocity );	// but.. this is no more avelocity? this is some angles

	// g-cont. trying to get avelocity length, normalize, rotate by parent space and apply length again?
#endif
}

Vector CBaseEntity::GetScale() const
{
	// storing scale in pev->startpos hack is specific for env_static
	if (Q_strcmp(GetClassname(), "env_static") == 0)
	{
		if (pev->startpos.Length() > 0.001f) {
			return pev->startpos;
		}
	}
	return pev->scale > 0.001f ? vec3_t(pev->scale) : vec3_t(1.0f);
}

void CBaseEntity::ApplyLocalVelocityImpulse( const Vector &vecImpulse )
{
	// NOTE: Don't have to use GetVelocity here because local values
	// are always guaranteed to be correct, unlike abs values which may 
	// require recomputation
	if( vecImpulse != g_vecZero )
	{
		InvalidatePhysicsState( FL_ABSVELOCITY );
		m_vecVelocity += vecImpulse;
	}
}

void CBaseEntity::ApplyAbsVelocityImpulse( const Vector &vecImpulse )
{
	// NOTE: Have to use GetAbsVelocity here to ensure it's the correct value
	if( vecImpulse != g_vecZero )
	{
		Vector vecResult = GetAbsVelocity() + vecImpulse;
		SetAbsVelocity( vecResult );
	}
}

void CBaseEntity::GetVectors( Vector *pForward, Vector *pRight, Vector *pUp ) const
{
	// this call is necessary to cause m_rgflCoordinateFrame to be recomputed
	const matrix4x4 &entityToWorld = EntityToWorldTransform();

	if( pForward != NULL )
	{
		*pForward = entityToWorld.GetForward();
	}

	if( pRight != NULL )
	{
		*pRight = entityToWorld.GetRight();
	}

	if( pUp != NULL )
	{
		*pUp = entityToWorld.GetUp();
	}
}

//-----------------------------------------------------------------------------
// Methods that modify local physics state, and let us know to compute abs state later
//-----------------------------------------------------------------------------
void CBaseEntity :: SetLocalOrigin( const Vector& origin )
{
//	if( m_vecOrigin == origin ) return;

	InvalidatePhysicsState( FL_ABSTRANSFORM );
	m_vecOrigin = origin;
}

void CBaseEntity::SetLocalAngles( const Vector& angles )
{
//	if( m_vecAngles == angles ) return;

	// This will cause the velocities of all children to need recomputation
	InvalidatePhysicsState( FL_ABSTRANSFORM, FL_ABSVELOCITY|FL_ABSAVELOCITY );
	m_vecAngles = angles;
}

void CBaseEntity :: SetLocalVelocity( const Vector &vecVelocity )
{
//	if( m_vecVelocity == vecVelocity ) return;

	InvalidatePhysicsState( FL_ABSVELOCITY );
	m_vecVelocity = vecVelocity;
}

void CBaseEntity::SetLocalAvelocity( const Vector &vecAvelocity )
{
//	if( m_vecAvelocity == vecAvelocity ) return;

	InvalidatePhysicsState( FL_ABSAVELOCITY );
	m_vecAvelocity = vecAvelocity;
}

/*
====================
TouchLinks
====================
*/
void CBaseEntity::TouchLinks( edict_t *ent, const Vector &entmins, const Vector &entmaxs, const Vector *pPrevOrigin, areanode_t *node )
{
	edict_t *touch;
	link_t *l, *next;
	CBaseEntity *pTouch, *pEnt;
	Vector test, offset;

	pEnt = CBaseEntity::Instance( ent );

	// touch linked edicts
	for( l = node->trigger_edicts.next; l != &node->trigger_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );
		pTouch = CBaseEntity::Instance( touch );

		if( !pTouch )
		{
			if( !touch->free )
			{
				ALERT( at_error, "bad entity in trigger list!\n" );
				SetBits( touch->v.flags, FL_KILLME ); // let the engine kill invalid entity
			}
			break;
		}

		if( pTouch == pEnt || pTouch->pev->solid != SOLID_TRIGGER ) // disabled ?
			continue;

		if( pTouch->pev->groupinfo && pEnt->pev->groupinfo )
		{
			if(( !g_groupop && !(pTouch->pev->groupinfo & pEnt->pev->groupinfo)) ||
			(g_groupop == 1 && (pTouch->pev->groupinfo & pEnt->pev->groupinfo)))
				continue;
		}

		if( !BoundsIntersect( entmins, entmaxs, touch->v.absmin, touch->v.absmax ))
			continue;

		// check brush triggers accuracy
		if( UTIL_GetModelType( pTouch->pev->modelindex ) == mod_brush )
		{
			if( pPrevOrigin && pTouch->IsPortal( ))
			{
				TraceResult tr;

				// BUGBUG: this is valid only for zero-sized objects
				// This code helps the fast moving objects a moving through thin triggers (e.g. portals)
				UTIL_TraceModel( *pPrevOrigin, pEnt->GetAbsOrigin(), point_hull, touch, &tr );
				if( tr.flFraction == 1.0f )
					continue;
			}
			else
			{
				// force to select bsp-hull
				hull_t *hull = UTIL_HullForBsp( pTouch, pEnt->pev->mins, pEnt->pev->maxs, offset );

				// support for rotational triggers
				if( UTIL_CanRotate( pTouch ) && pTouch->GetAbsAngles() != g_vecZero )
				{
					matrix4x4	matrix( offset, pTouch->GetAbsAngles() );
					test = matrix.VectorITransform( pEnt->GetAbsOrigin() );
				}
				else
				{
					// offset the test point appropriately for this hull.
					test = pEnt->GetAbsOrigin() - offset;
				}

				// test hull for intersection with this model
				if( UTIL_HullPointContents( hull, hull->firstclipnode, test ) != CONTENTS_SOLID )
					continue;
			}
		}

      		gpGlobals->time = PHYSICS_TIME();
		DispatchTouch( touch, ent );
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;
	
	if( entmaxs[node->axis] > node->dist )
		TouchLinks( ent, entmins, entmaxs, pPrevOrigin, node->children[0] );
	if( entmins[node->axis] < node->dist )
		TouchLinks( ent, entmins, entmaxs, pPrevOrigin, node->children[1] );
}

/*
====================
ClipLinks

same as TouchLinks but for triggers
====================
*/
void CBaseEntity::ClipLinks( edict_t *ent, const Vector &entmins, const Vector &entmaxs, areanode_t *node )
{
	edict_t *clip;
	link_t *l, *next;
	CBaseEntity *pClip, *pEnt;
	Vector test, offset;

	pEnt = CBaseEntity::Instance( ent );

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		clip = EDICT_FROM_AREA( l );
		pClip = CBaseEntity::Instance( clip );

		if( pClip == pEnt || pClip->pev->solid == SOLID_NOT ) // disabled ?
			continue;

		if( pClip->pev->groupinfo && pEnt->pev->groupinfo )
		{
			if(( !g_groupop && !(pClip->pev->groupinfo & pEnt->pev->groupinfo)) ||
			(g_groupop == 1 && (pClip->pev->groupinfo & pEnt->pev->groupinfo)))
				continue;
		}

		if( !BoundsIntersect( entmins, entmaxs, clip->v.absmin, clip->v.absmax ))
			continue;

		// check brush triggers accuracy
		if( UTIL_GetModelType( pClip->pev->modelindex ) == mod_brush )
		{
			// force to select bsp-hull
			hull_t *hull = UTIL_HullForBsp( pClip, pEnt->pev->mins, pEnt->pev->maxs, offset );

			// support for rotational triggers
			if( UTIL_CanRotate( pClip ) && pClip->GetAbsAngles() != g_vecZero )
			{
				matrix4x4	matrix( offset, pClip->GetAbsAngles() );
				test = matrix.VectorITransform( pEnt->GetAbsOrigin() );
			}
			else
			{
				// offset the test point appropriately for this hull.
				test = pEnt->GetAbsOrigin() - offset;
			}

			// test hull for intersection with this model
			if( UTIL_HullPointContents( hull, hull->firstclipnode, test ) != CONTENTS_SOLID )
				continue;
		}

      		gpGlobals->time = PHYSICS_TIME();
		DispatchTouch( ent, clip );
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;
	
	if( entmaxs[node->axis] > node->dist )
		ClipLinks( ent, entmins, entmaxs, node->children[0] );
	if( entmins[node->axis] < node->dist )
		ClipLinks( ent, entmins, entmaxs, node->children[1] );
}

/*
====================
SleepPortals

put portals to sleep for some time
====================
*/
void CBaseEntity :: SleepPortals( edict_t *ent, const Vector &entmins, const Vector &entmaxs, areanode_t *node )
{
	edict_t *touch;
	link_t *l, *next;
	CBaseEntity *pTouch, *pEnt;
	Vector test, offset;

	pEnt = CBaseEntity::Instance( ent );

	// touch linked edicts
	for( l = node->trigger_edicts.next; l != &node->trigger_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );
		pTouch = CBaseEntity::Instance( touch );

		if( !pTouch->IsPortal( ))
			continue;

		if( !BoundsIntersect( entmins, entmaxs, touch->v.absmin, touch->v.absmax ))
			continue;

		// check brush triggers accuracy
		if( UTIL_GetModelType( pTouch->pev->modelindex ) == mod_brush )
		{
			// force to select bsp-hull
			hull_t *hull = UTIL_HullForBsp( pTouch, pEnt->pev->mins, pEnt->pev->maxs, offset );

			// support for rotational triggers
			if( UTIL_CanRotate( pTouch ) && pTouch->GetAbsAngles() != g_vecZero )
			{
				matrix4x4	matrix( offset, pTouch->GetAbsAngles() );
				test = matrix.VectorITransform( pEnt->GetAbsOrigin() );
			}
			else
			{
				// offset the test point appropriately for this hull.
				test = pEnt->GetAbsOrigin() - offset;
			}

			// test hull for intersection with this model
			if( UTIL_HullPointContents( hull, hull->firstclipnode, test ) != CONTENTS_SOLID )
				continue;
		}

		// complex two-side portals may activate twice
		// so we need put to sleep them to avoid back teleportation
		pTouch->PortalSleep( 0.2f );

		// make backside portal is active now
		if( pTouch->pev->sequence > 0 )
		{
			CBaseEntity *pPortalCamera = CBaseEntity::Instance( INDEXENT( pTouch->pev->sequence ));
			if( pPortalCamera ) SetBits( pPortalCamera->pev->effects, EF_MERGE_VISIBILITY ); // visible now
		}
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;
	
	if( entmaxs[node->axis] > node->dist )
		SleepPortals( ent, entmins, entmaxs, node->children[0] );
	if( entmins[node->axis] < node->dist )
		SleepPortals( ent, entmins, entmaxs, node->children[1] );
}

void CBaseEntity::RelinkEntity( BOOL touch_triggers, const Vector *pPrevOrigin, BOOL sleep_portals )
{
	if( !g_fPhysicInitialized )
	{
		SET_ORIGIN( edict(), pev->origin );
		return;
	}

#ifdef USE_ENGINE_TOUCH_TRIGGERS
	LINK_ENTITY( edict(), TRUE );
#else
	LINK_ENTITY( edict(), FALSE );

	// custom trigger handler used an accurate collision on fast moving objects with triggers
	if( touch_triggers )
	{
		Vector entmins, entmaxs;

		// only point entities needs to check with special case:
		// fast moving point entity moving through portal
		if( !IsPointSized( )) pPrevOrigin = NULL;

		if( pPrevOrigin != NULL )
			UTIL_MoveBounds( *pPrevOrigin, pev->mins, pev->maxs, GetAbsOrigin(), entmins, entmaxs );
		else UTIL_MoveBounds( GetAbsOrigin(), pev->mins, pev->maxs, GetAbsOrigin(), entmins, entmaxs );

		if( sleep_portals )
			SleepPortals( edict(), entmins, entmaxs, GET_AREANODE() );

		if( !g_fTouchLinkSemaphore )
		{
			g_fTouchLinkSemaphore = TRUE;
 
			// moving triggers retouches objects
#if 0			// g-cont. disabled until is done
			if( pev->solid == SOLID_TRIGGER )
				ClipLinks( edict(), entmins, entmaxs, GET_AREANODE() );
			else
#endif
				TouchLinks( edict(), entmins, entmaxs, pPrevOrigin, GET_AREANODE() );
			g_fTouchLinkSemaphore = FALSE;
		}
	}
#endif
}

void CBaseEntity :: SetModel( const char *model )
{
	int modelIndex = MODEL_INDEX( model );
	if( modelIndex == 0 ) return;

	pev->model = MAKE_STRING( model );
	pev->modelindex = modelIndex;

	modtype_t mod_type = UTIL_GetModelType( modelIndex );

	Vector mins = g_vecZero;
	Vector maxs = g_vecZero;

	// studio models set to zero sizes as default
	switch( mod_type )
	{
	case mod_brush:
		UTIL_GetModelBounds( modelIndex, mins, maxs );
		break;
	case mod_studio:
		ResetPoseParameters();
		break;
	case mod_sprite:
		UTIL_GetModelBounds( modelIndex, mins, maxs );
		break;
	default:
		break;
	}

	UTIL_SetSize( this, mins, maxs );
}

void CBaseEntity::UnlinkFromParent( void )
{
	if( m_hParent == NULL )
		return;

	if( m_hParent->IsPlayer( ))
	{
		RestoreMoveType();
	}

	// parent is loosed
	OnClearParent ();

	// NOTE: Have to do this before the unlink to ensure local coords are valid
	Vector vecOrigin = GetAbsOrigin();
	Vector angAngles = GetAbsAngles();
	Vector vecVelocity = GetAbsVelocity();
//	Vector vecAvelocity = GetAbsAvelocity();

	UnlinkChild( m_hParent );

	SetAbsOrigin( vecOrigin );
	SetAbsAngles( angAngles );
	SetAbsVelocity( vecVelocity);
//	SetAbsAvelocity( vecAbsAvelocity );
}

void CBaseEntity :: UnlinkChild( CBaseEntity *pParent )
{
	CBaseEntity *pList;
	EHANDLE *pPrev;

	if( pParent == NULL )
		return;

	pList = pParent->m_hChild;
	pPrev = &pParent->m_hChild;

	while( pList )
	{
		CBaseEntity *pNext = pList->m_hNextChild;

		if( pList == this )
		{
			// patch up the list
			*pPrev = pNext;

			// clear hierarchy bits for this guy
			pList->m_hParent = NULL;
			pList->m_hNextChild = NULL;
			return;
		}
		else
		{
			pPrev = &pList->m_hNextChild;
			pList = pNext;
		}
	}

	// this only happens if the child wasn't found in the parent's child list
	ALERT( at_warning, "UnlinkChild: couldn't find the parent!\n" );
}

void CBaseEntity :: LinkChild( CBaseEntity *pParent )
{
	if( pParent == NULL )
		return;

	m_hParent = pParent;
	m_hNextChild = m_hParent->m_hChild;
	m_hParent->m_hChild = this;
}

void CBaseEntity :: UnlinkAllChildren( void )
{
	CBaseEntity *pChild = m_hChild;

	while( pChild )
	{
		CBaseEntity *pNext = pChild->m_hNextChild;
		pChild->UnlinkFromParent();
		pChild = pNext;
	}
}

CBaseEntity *CBaseEntity :: GetRootParent( void )
{
	CBaseEntity *pEntity = this;
	CBaseEntity *pParent = m_hParent;

	while( pParent )
	{
		pEntity = pParent;
		pParent = pEntity->m_hParent;
	}

	return pEntity;
}

BOOL CBaseEntity :: HasAttachment( void )
{
	// try to extract aiment from name
	char *pname = (char *)STRING( m_iParent );

	return (Q_strchr( pname, '.' ) != NULL);
}

//=======================================================================
//	set parent (void ) dinamically link parents
//=======================================================================
void CBaseEntity :: SetParent( int m_iNewParent, int m_iAttachment )
{
	if( !m_iNewParent )
	{
		// unlink entity from chain
		UnlinkFromParent();
		return;
          }

	CBaseEntity *pParent;

	if( !m_iAttachment )
	{
		char	name[256];

		// don't modify string from table, make a local copy!
		Q_strncpy( name, STRING( m_iNewParent ), sizeof( name ));

		// try to extract aiment from name
		char *pname = (char *)name;
		char *pstart  = Q_strchr( pname, '.' );

		if( pstart )
		{
			m_iAttachment = Q_atoi( pstart + 1 );	
			pname[pstart-pname] = '\0';
			pParent = UTIL_FindEntityByTargetname( NULL, pname );
			SetParent( pParent, m_iAttachment );
			return;
		}
	}

	pParent = UTIL_FindEntityByTargetname( NULL, STRING( m_iNewParent ));	
	SetParent( pParent, m_iAttachment );

	CheckForMultipleParents( this, pParent );
}

//=======================================================================
//		set parent main function
//=======================================================================
void CBaseEntity :: SetParent( CBaseEntity *pParent, int m_iAttachment )
{
	if( ( ( pParent != NULL ) && ( pParent == m_hParent ) ) || m_fPicked )
		return; // new parent is already set or object is picked

	if( pParent == g_pWorld )
		return; // newer use world as parent

	if( m_iActorType == ACTOR_STATIC )
	{
		ALERT( at_error, "SetParent: static actor %s can't have parent\n", GetClassname( ));
		return;
	}

	UnlinkFromParent();

	if( pParent == NULL )
	{
		if( pev->targetname )
			ALERT( at_warning, "Not found parent for %s with name %s\n", STRING( pev->classname ), STRING( pev->targetname ));
		else ALERT( at_warning, "Not found parent for %s\n", STRING( pev->classname ));
		return;
	}

	// check for himself parent
	if( pParent == this ) 
	{
		if( pev->targetname )
			ALERT( at_error, "%s with name %s has illegal parent\n", STRING( pev->classname ), STRING( pev->targetname ));
		else ALERT( at_error, "%s has illegal parent\n", STRING( pev->classname ));
		return;
	}

	// make sure what child is the point entity and the parent have studiomodel
         	if( m_iAttachment && m_iFlags & MF_POINTENTITY && pParent->IsStudioModel( ))
         	{
		pev->skin = ENTINDEX( pParent->edict( ));
		pev->body = m_iAttachment;
		pev->aiment = pParent->edict();
		pev->movetype = MOVETYPE_FOLLOW;
		return;
	}

	if( pParent->IsPlayer( ))
	{
		MakeNonMoving();
	}

	LinkChild( pParent );

	// I have the axes of local space in world space. (childMatrix)
	// I want to compute those world space axes in the parent's local space
	// and set that transform (as angles) on the child's object so the net
	// result is that the child is now in parent space, but still oriented the same way
	Vector angles = pParent->GetAbsAngles();

	if( pParent->IsPlayer() )
		angles.x = angles.z = 0.0f; // only YAW will be affected

	matrix4x4	parent( pParent->GetAbsOrigin(), angles, 1.0f );
	matrix4x4	child( GetLocalOrigin(), GetLocalAngles( ), 1.0f );

	matrix4x4	tmp( parent.Invert( ));
	tmp = tmp.ConcatTransforms( child );

	Vector localOrigin, localAngles;

	tmp.GetAngles( localAngles );
	tmp.GetOrigin( localOrigin );

	SetLocalAngles( localAngles );
	SetLocalOrigin( localOrigin );
	RelinkEntity( FALSE );

	// NOTE: no need to call this event on initialization
	// because entities linked *before* spawn not after
	if( GET_SERVER_STATE() == SERVER_ACTIVE )
	{
		// parent is changed
		OnChangeParent ();
	}
}

BOOL CBaseEntity :: ShouldCollide( CBaseEntity *pOther )
{
	if( m_iParentFilter && m_pRootParent )
	{
		if( m_pRootParent == pOther->GetRootParent() )
		{
			if( pOther->pev->movetype != MOVETYPE_PUSHSTEP )
				return FALSE;
		}
	}

	if( m_iPushableFilter && pev->movetype == MOVETYPE_PUSHSTEP && pev->solid == SOLID_BSP )
	{
		if( pOther->GetGroundEntity() == this )
			return FALSE;
	}

	return TRUE;
}

// This updates global tables that need to know about entities being removed
void CBaseEntity::UpdateOnRemove( void )
{
	if ( FBitSet( pev->flags, FL_GRAPHED ) )
	{
		// this entity was a LinkEnt in the world node graph, so we must remove it from
		// the graph since we are removing it from the world.
		for ( int i = 0; i < WorldGraph.m_cLinks; i++ )
		{
			if ( WorldGraph.m_pLinkPool[i].m_pLinkEnt == pev )
			{
				// if this link has a link ent which is the same ent
				// that is removing itself, remove it!
				WorldGraph.m_pLinkPool[i].m_pLinkEnt = NULL;
			}
		}
	}

	UnlinkFromParent();
	UnlinkAllChildren();

	if ( pev->globalname )
		gGlobalState.EntitySetState( pev->globalname, GLOBAL_DEAD );

	// remove rigid body if present
	WorldPhysic->RemoveBody( edict() );

	// call event "on remove"
	OnRemove();

	if( pev->modelindex )
	{
		// remove all the particle trails, attached to this entity
		MESSAGE_BEGIN( MSG_ALL, gmsgKillPart );
			WRITE_ENTITY( entindex() );
		MESSAGE_END();

		// remove all the studio decals, attached to this entity
		MESSAGE_BEGIN( MSG_ALL, gmsgKillDecals );
			WRITE_ENTITY( entindex() );
		MESSAGE_END();
	}
}

/*
==============================
SUB_UseTargets

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Removes all entities with a targetname that match self.killtarget,
and removes them, so some events can remove other triggers.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function (if they have one)

==============================
*/
void CBaseEntity :: SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value, string_t m_iszAltTarget )
{
	//
	// fire targets
	//
	if( !FStringNull( m_iszAltTarget ))
	{
		UTIL_FireTargets( m_iszAltTarget, pActivator, this, useType, value );
	}
	else if( !FStringNull( pev->target ))
	{
		UTIL_FireTargets( GetTarget(), pActivator, this, useType, value );
	}
}

// give health
int CBaseEntity :: TakeHealth( float flHealth, int bitsDamageType )
{
	if (!pev->takedamage)
		return 0;

	if ( pev->health >= pev->max_health )
		return 0;

	pev->health += flHealth;

	pev->health = Q_min(pev->health, pev->max_health);

	return 1;
}

// give battery charge
int CBaseEntity :: TakeArmor( float flArmor, int suit )
{
	if (!pev->takedamage)
		return 0;

	if ( pev->armorvalue >= MAX_NORMAL_BATTERY )
		return 0;

	pev->armorvalue += flArmor;
	pev->armorvalue = Q_min(pev->armorvalue, MAX_NORMAL_BATTERY);

	return 1;
}

// inflict damage on this entity.  bitsDamageType indicates type of damage inflicted, ie: DMG_CRUSH

int CBaseEntity :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector	vecTemp;

	if (!pev->takedamage)
		return 0;

	// UNDONE: some entity types may be immune or resistant to some bitsDamageType
	
	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if ( pevAttacker == pevInflictor )	
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}
	else
	// an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}

	// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();
		
	// save damage based on the target's armor level

	// figure momentum add (don't let hurt brushes or other triggers move player)
	if ((!FNullEnt(pevInflictor)) && (pev->movetype == MOVETYPE_WALK || pev->movetype == MOVETYPE_STEP) && (pevAttacker->solid != SOLID_TRIGGER) )
	{
		Vector vecDir = GetAbsOrigin() - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();

		float flForce = flDamage * ((32 * 32 * 72.0) / (pev->size.x * pev->size.y * pev->size.z)) * 5;
		
		if (flForce > 1000.0) 
			flForce = 1000.0;
		SetAbsVelocity( GetAbsVelocity() + vecDir * flForce );
	}

	// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		Killed( pevAttacker, GIB_NORMAL );
		return 0;
	}

	return 1;
}

void CBaseEntity :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	UTIL_Remove( this );
}

void CBaseEntity :: ReportInfo( void )
{
	Msg( "class %s[%i] serial %i", GetClassname(), entindex(), edict()->serialnumber );

	if( pev->globalname != NULL_STRING )
		Msg( ", globalname %s", GetGlobalname());

	if( pev->targetname != NULL_STRING )
		Msg( ", targetname %s", GetTargetname());

	if( pev->target != NULL_STRING )
		Msg( ", target %s", GetTarget());


	if( pev->model != NULL_STRING )
		Msg( ", model %s", GetModel());
	
	Msg( ", state %s", GetStringForState( GetState() )); //LRC

	Msg( "\n" );

	if( m_hParent != NULL )
	{
		Msg( "Parent: " );
		m_hParent->ReportInfo();
	}
}

CBaseEntity *CBaseEntity::GetNextTarget( void )
{
	if ( FStringNull( pev->target ) )
		return NULL;
	edict_t *pTarget = FIND_ENTITY_BY_TARGETNAME ( NULL, STRING(pev->target) );
	if ( FNullEnt(pTarget) )
		return NULL;

	return Instance( pTarget );
}

//-----------------------------------------------------------------------------
// Purpose: Recurses an entity hierarchy and fills out a list of all entities
// in the hierarchy with their current origins and angles.
//
// This list is necessary to keep lazy updates of abs origins and angles
// from messing up our child/constrained entity fixup.
//-----------------------------------------------------------------------------
static void BuildTeleportList_r( CBaseEntity *pTeleport, CUtlArray<TeleportListEntry_t> &teleportList )
{
	TeleportListEntry_t entry;
	
	entry.pEntity = pTeleport;
	entry.prevAbsOrigin = pTeleport->GetAbsOrigin();
	entry.prevAbsAngles = pTeleport->GetAbsAngles();

	teleportList.AddToTail( entry );

	CBaseEntity *pList = pTeleport->m_hChild;

	while( pList )
	{
		BuildTeleportList_r( pList, teleportList );
		pList = pList->m_hNextChild;
	}
}

static void BuildTeleportListTouch( CBaseEntity *pTeleport, CUtlArray<TeleportListEntry_t> &teleportList )
{
	edict_t *pEdict = INDEXENT( 1 );
	TeleportListEntry_t entry;
	TraceResult trace;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;

		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if( !pEntity || ( pEntity == pTeleport ) || ( pEntity->m_hParent != NULL ) || !pEntity->pev->model )
			continue;	// filter unneeded ents

		if( BoundsIntersect( pTeleport->pev->absmin, pTeleport->pev->absmax, pEntity->pev->absmin, pEntity->pev->absmax ))
		{
			Msg( "add to teleport list %s\n", pEntity->GetClassname());
			entry.pEntity = pEntity;
			entry.prevAbsOrigin = pEntity->GetAbsOrigin();
			entry.prevAbsAngles = pEntity->GetAbsAngles();
			teleportList.AddToTail( entry );
		}
	}
}

void CBaseEntity :: Teleport( const Vector *newPosition, const Vector *newAngles, const Vector *newVelocity )
{
	if( g_TeleportStack.Find( this ) >= 0 )
		return;

	int index = g_TeleportStack.AddToTail( this );

	CUtlArray<TeleportListEntry_t> teleportList;
	BuildTeleportList_r( this, teleportList );

	// also teleport all ents that stay on train
	if( FClassnameIs( this, "func_tracktrain" ))
	{
//		BuildTeleportListTouch( this, teleportList );
	}

	for( int i = 0; i < teleportList.Count(); i++ )
	{
		UTIL_Teleport( this, teleportList[i], newPosition, newAngles, newVelocity );
	}

	ASSERT( g_TeleportStack[index] == this );
	g_TeleportStack.FastRemove( index );
}

BEGIN_DATADESC_NO_BASE( CBaseEntity )
	DEFINE_FIELD( m_pGoalEnt, FIELD_CLASSPTR ),

	DEFINE_KEYFIELD( m_iParent, FIELD_STRING, "parent" ),
	DEFINE_FIELD( m_hParent, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hChild, FIELD_EHANDLE ),	
	DEFINE_FIELD( m_hNextChild, FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_iParentFlags, FIELD_INTEGER, "parentflags" ),

	DEFINE_FIELD( m_vecEndPos, FIELD_POSITION_VECTOR ),	// for beams 

	DEFINE_FIELD( m_vecOrigin, FIELD_VECTOR ), 
	DEFINE_FIELD( m_vecAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecVelocity, FIELD_VECTOR ), 
	DEFINE_FIELD( m_vecAvelocity, FIELD_VECTOR ),
	DEFINE_ARRAY( m_local, FIELD_FLOAT, 16 ),		// matrix4x4

	DEFINE_FIELD( m_iFlags, FIELD_INTEGER ),
	DEFINE_FIELD( m_iOldSolid, FIELD_INTEGER ),
	DEFINE_FIELD( m_iOldMoveType, FIELD_INTEGER ),
	DEFINE_FIELD( m_flMoveDoneTime, FIELD_FLOAT ),		// local time saved as float, not time!
	DEFINE_FIELD( m_fPicked, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iStyle, FIELD_INTEGER ),
	DEFINE_FIELD( m_flGaitYaw, FIELD_FLOAT ),

	DEFINE_FIELD( m_pfnThink, FIELD_FUNCTION ),		// UNDONE: Build table of these!!!
	DEFINE_FIELD( m_pfnTouch, FIELD_FUNCTION ),
	DEFINE_FIELD( m_pfnUse, FIELD_FUNCTION ),
	DEFINE_FIELD( m_pfnBlocked, FIELD_FUNCTION ),
	DEFINE_FIELD( m_pfnMoveDone, FIELD_FUNCTION ),

	DEFINE_FIELD( m_flShowHostile, FIELD_TIME ),
	DEFINE_FIELD( m_isChaining, FIELD_BOOLEAN ),		// door stuff but need to set everywhere

	// studio pose parameters
	DEFINE_AUTO_ARRAY( m_flPoseParameter, FIELD_FLOAT ),

	// function pointers
	DEFINE_FUNCTION( SUB_Remove ),
	DEFINE_FUNCTION( SUB_DoNothing ),
	DEFINE_FUNCTION( SUB_StartFadeOut ),
	DEFINE_FUNCTION( SUB_FadeOut ),
	DEFINE_FUNCTION( SUB_CallUseToggle ),

	// PhysX description
	DEFINE_FIELD( m_iActorType, FIELD_CHARACTER ),
	DEFINE_FIELD( m_iActorFlags, FIELD_INTEGER ),
	DEFINE_FIELD( m_iBodyFlags, FIELD_INTEGER ),
	DEFINE_FIELD( m_iFilterData[0], FIELD_INTEGER ),
	DEFINE_FIELD( m_iFilterData[1], FIELD_INTEGER ),
	DEFINE_FIELD( m_iFilterData[2], FIELD_INTEGER ),
	DEFINE_FIELD( m_iFilterData[3], FIELD_INTEGER ),
	DEFINE_FIELD( m_flBodyMass, FIELD_FLOAT ),
	DEFINE_FIELD( m_fFreezed, FIELD_BOOLEAN ),
END_DATADESC()

int CBaseEntity::Save( CSave &save )
{
	// here, we must force recomputation of all abs data so it gets saved correctly
	// we can't leave the transform bits set because the loader can't cope with it.
	CalcAbsolutePosition();
	CalcAbsoluteVelocity();

	if( m_iActorType != ACTOR_INVALID )
		WorldPhysic->SaveBody( this );

	if( save.WriteEntVars( "ENTVARS", GetDataDescMap(), pev ))
		return save.WriteAll( this, GetDataDescMap());

	return 0;
}

int CBaseEntity::Restore( CRestore &restore )
{
	int status;
	EHANDLE hParent, hChild, hNextChild;

	if( restore.IsGlobalMode() )
	{
		// we already have the valid chain.
		// Don't break it with bad pointers from previous level
		hParent = m_hParent;
		hChild = m_hChild;
		hNextChild = m_hNextChild;
	}

	status = restore.ReadEntVars( "ENTVARS", GetDataDescMap(), pev );
	if ( status )
		status = restore.ReadAll( this, GetDataDescMap());

	if( restore.IsGlobalMode() )
	{
		// restore our chian here
		m_hParent = hParent;
		m_hChild = hChild;
		m_hNextChild = hNextChild;
	}

	// ---------------------------------------------------------------
	// HACKHACK: We don't know the space of these vectors until now
	// if they are worldspace, fix them up.
	// ---------------------------------------------------------------
	{
		Vector parentSpaceOffset = restore.modelSpaceOffset;

		if( m_hParent == NULL )
		{
			// parent is the world, so parent space is worldspace
			// so update with the worldspace leveltransition transform
			parentSpaceOffset += gpGlobals->vecLandmarkOffset;
		}

		m_local.SetOrigin( pev->origin );
		m_vecOrigin += parentSpaceOffset;
	}

	if ( pev->modelindex != 0 && !FStringNull( pev->model ))
	{
		Vector mins = pev->mins; // Set model is about to destroy these
		Vector maxs = pev->maxs;


		PRECACHE_MODEL( GetModel() );
		SetModel( GetModel() );
		UTIL_SetSize( pev, mins, maxs ); // Reset them
	}

	if( m_iActorType != ACTOR_INVALID )
		m_pUserData = WorldPhysic->RestoreBody( this );

	return status;
}

// Initialize absmin & absmax to the appropriate box
void CBaseEntity::SetObjectCollisionBox( void )
{
	if( UTIL_CanRotateBModel( this ) && ( GetAbsAngles() != g_vecZero ))
	{	
		// expand for rotation
		TransformAABB( EntityToWorldTransform(), pev->mins, pev->maxs, pev->absmin, pev->absmax );
	}
	else
	{
		pev->absmin = GetAbsOrigin() + pev->mins;
		pev->absmax = GetAbsOrigin() + pev->maxs;
	}

	pev->absmin.x -= 1;
	pev->absmin.y -= 1;
	pev->absmin.z -= 1;
	pev->absmax.x += 1;
	pev->absmax.y += 1;
	pev->absmax.z += 1;
}

//-----------------------------------------------------------------------------
// Transforms an AABB measured in entity space to a box that surrounds it in world space
//-----------------------------------------------------------------------------
void CBaseEntity::EntityAABBToWorldAABB( const Vector &entityMins, const Vector &entityMaxs, Vector &pWorldMins, Vector &pWorldMaxs ) const
{
	if( GetAbsAngles() == g_vecZero )
	{
		pWorldMins = entityMins + GetAbsOrigin();
		pWorldMaxs = entityMaxs + GetAbsOrigin();
	}
	else
	{
		TransformAABB( EntityToWorldTransform(), entityMins, entityMaxs, pWorldMins, pWorldMaxs );
	}
}

void CBaseEntity::WorldSpaceAABB( Vector &pWorldMins, Vector &pWorldMaxs ) const
{
	if( UTIL_GetModelType( pev->modelindex ) != mod_brush )
	{
		pWorldMins = pev->mins + GetAbsOrigin();
		pWorldMaxs = pev->maxs + GetAbsOrigin();
	}
	else
	{
		EntityAABBToWorldAABB( pev->mins, pev->maxs, pWorldMins, pWorldMaxs ); 
	}
}

void CBaseEntity :: CalcNearestPoint( const Vector &vecWorldPt, Vector *pVecNearestWorldPt ) const
{
	// calculate physics force
	Vector localPt, localClosestPt;

	WorldToCollisionSpace( vecWorldPt, &localPt );
	CalcClosestPointOnAABB( pev->mins, pev->maxs, localPt, localClosestPt );
	CollisionToWorldSpace( localClosestPt, pVecNearestWorldPt );
}

int CBaseEntity :: Intersects( CBaseEntity *pOther )
{
	if ( pOther->pev->absmin.x > pev->absmax.x ||
	     pOther->pev->absmin.y > pev->absmax.y ||
	     pOther->pev->absmin.z > pev->absmax.z ||
	     pOther->pev->absmax.x < pev->absmin.x ||
	     pOther->pev->absmax.y < pev->absmin.y ||
	     pOther->pev->absmax.z < pev->absmin.z )
		 return 0;
	return 1;
}

int CBaseEntity :: AreaIntersect( Vector mins, Vector maxs )
{
	if( pev->absmin.x >= maxs.x
	||  pev->absmin.y >= maxs.y
	||  pev->absmin.z >= maxs.z
	||  pev->absmax.x <= mins.x
	||  pev->absmax.y <= mins.y
	||  pev->absmax.z <= mins.z )
		return FALSE;
	return TRUE;
}

int CBaseEntity :: TriggerIntersects( CBaseEntity *pOther )
{
	if ( !Intersects( pOther ))
		 return 0;

	int hullNumber = human_hull;
	TraceResult trace;

	if( FBitSet( pOther->pev->flags, FL_DUCKING ))
		hullNumber = head_hull;

	UTIL_TraceModel( pOther->GetAbsOrigin(), pOther->GetAbsOrigin(), hullNumber, edict(), &trace );

	return trace.fStartSolid;
}

int CBaseEntity :: IsWater( void )
{
	if( pev->solid == SOLID_NOT && pev->skin <= CONTENTS_WATER && pev->skin > CONTENTS_TRANSLUCENT )
	{
		if( UTIL_GetModelType( pev->modelindex ) == mod_brush )
			return true;
	}
	return false;
}

void CBaseEntity :: MakeDormant( void )
{
	SetBits( pev->flags, FL_DORMANT );
	
	// Don't touch
	pev->solid = SOLID_NOT;
	// Don't move
	pev->movetype = MOVETYPE_NONE;
	// Don't draw
	SetBits( pev->effects, EF_NODRAW );
	// Don't think
	DontThink();
	// Relink
	RelinkEntity( FALSE );
}

int CBaseEntity :: IsDormant( void )
{
	return FBitSet( pev->flags, FL_DORMANT );
}

BOOL CBaseEntity :: IsInWorld( BOOL checkVelocity )
{
	Vector absOrigin = GetAbsOrigin();
	const float originLimit = 32768.f;
	const float velocityLimit = CVAR_GET_FLOAT("sv_maxvelocity");

	// position 
	if( absOrigin.x >= originLimit) return FALSE;
	if( absOrigin.y >= originLimit) return FALSE;
	if( absOrigin.z >= originLimit) return FALSE;
	if( absOrigin.x <= -originLimit) return FALSE;
	if( absOrigin.y <= -originLimit) return FALSE;
	if( absOrigin.z <= -originLimit) return FALSE;

	if( !checkVelocity )
		return TRUE;

	Vector absVelocity = GetAbsVelocity();

	// speed
	if( absVelocity.x >= velocityLimit) return FALSE;
	if( absVelocity.y >= velocityLimit) return FALSE;
	if( absVelocity.z >= velocityLimit) return FALSE;
	if( absVelocity.x <= -velocityLimit) return FALSE;
	if( absVelocity.y <= -velocityLimit) return FALSE;
	if( absVelocity.z <= -velocityLimit) return FALSE;

	return TRUE;
}

int CBaseEntity::ShouldToggle( USE_TYPE useType, BOOL currentState )
{
	if ( useType != USE_TOGGLE && useType != USE_SET )
	{
		if ( (currentState && useType == USE_ON) || (!currentState && useType == USE_OFF) )
			return 0;
	}
	return 1;
}

BOOL CBaseEntity :: ShouldToggle( USE_TYPE useType )
{
	STATE curState = GetState();

	if( useType == USE_ON || useType == USE_OFF )
	{
		switch( curState )
		{
		case STATE_ON:
		case STATE_TURN_ON:
			if( useType == USE_ON )
				return FALSE;
			break;
		case STATE_OFF:
		case STATE_TURN_OFF:
			if( useType == USE_OFF )
				return FALSE;
			break;
		}
	}
	return TRUE;
}

const char *CBaseEntity::DamageDecal( int bitsDamageType )
{
	if (pev->rendermode != kRenderNormal) {
		return "shot_glass";
	}
	return "shot";
}

// NOTE: szName must be a pointer to constant memory, e.g. "monster_class" because the entity
// will keep a pointer to it after this call.
CBaseEntity * CBaseEntity::Create( char *szName, const Vector &vecOrigin, const Vector &vecAngles, edict_t *pentOwner )
{
	CBaseEntity *pEntity = CreateEntityByName( szName );

	if( !pEntity )
	{
		ALERT( at_console, "NULL Ent in Create!\n" );
		return NULL;
	}

	pEntity->pev->owner = pentOwner;
	pEntity->SetAbsOrigin( vecOrigin );
	pEntity->SetAbsAngles( vecAngles );
	pEntity->m_fSetAngles = TRUE;

	DispatchSpawn( pEntity->edict() );

	return pEntity;
}
