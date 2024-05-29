/*
physic.cpp - common physics code
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "client.h"
#include "nodes.h"
#include "decals.h"
#include "gamerules.h"
#include "game.h"
#include "com_model.h"
#include "studio.h"
#include "movelist.h"
#include "enginefeatures.h"
#include "render_api.h"
#include "physic.h"
#include "triangleapi.h"
#include "pm_defs.h"
#include "pm_shared.h"
#include "player.h"
#include "sv_materials.h"

#define MOVE_EPSILON	0.01f
#define MAX_CLIP_PLANES	5

// implementation of physic pushers
CPhysicsPushedEntities	s_PushedEntities;
CPhysicsPushedEntities	*g_pPushedEntities = &s_PushedEntities;

unsigned int EngineSetFeatures(void)
{
	unsigned int flags = (
		ENGINE_WRITE_LARGE_COORD |
		ENGINE_PHYSICS_PUSHER_EXT |
		ENGINE_COMPUTE_STUDIO_LERP |
		ENGINE_LARGE_LIGHTMAPS |
		ENGINE_LOAD_DELUXEDATA |
		ENGINE_COMPENSATE_QUAKE_BUG |
		ENGINE_LINEAR_GAMMA_SPACE
	);
	return flags;
}

void DrawDebugTriangles( void )
{
	if( g_debugdraw != NULL && g_debugdraw->value > 0.0f )
		WorldPhysic->DebugDraw();

	if( g_physdebug != NULL && g_physdebug->value > 0.0f )
	{
		edict_t *pEdict = INDEXENT( 1 );
		for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
		{
			if ( pEdict->free )	// Not in use
				continue;

			if( FClassnameIs( &pEdict->v, "func_door_rotating" ))
			{
				Tri->Color4f( 1.0f, 0.5f, 0.0f, 1.0f );
				Tri->Begin( TRI_POINTS );
					Tri->Vertex3fv( pEdict->v.origin );
				Tri->End();
			}
		}
	}
}

void DrawOrthoTriangles( void )
{
	if( p_speeds != NULL && p_speeds->value > 0.0f )
		WorldPhysic->DrawPSpeeds();
}

/*
============
SV_AllowPushRotate

Allows to change entity yaw?
============
*/
BOOL SV_AllowPushRotate( CBaseEntity *pEntity )
{
	model_t	*mod;

	mod = (model_t *)MODEL_HANDLE( pEntity->pev->modelindex );
	if( !mod || mod->type != mod_brush )
		return TRUE;

	// NOTE: flag 2 it's a internal engine flag (see model.c for details)
	// we can recalc real model origin here but this check is faster :)
	return (mod->flags & 2) ? TRUE : FALSE;
}

void SV_ClipMoveToEntity( edict_t *ent, const float *start, float *mins, float *maxs, const float *end, trace_t *trace )
{
	// convert edict_t to base entity
	CBaseEntity *pTouch = CBaseEntity::Instance( ent );

	if( !pTouch )
	{
		// removed entity?
		trace->allsolid = false;
		return;
	}

	WorldPhysic->SweepTest( pTouch, start, mins, maxs, end, trace );
}

void SV_ClipPMoveToEntity( physent_t *pe, const float *start, float *mins, float *maxs, const float *end, pmtrace_t *tr )
{
	// convert physent_t to base entity
	CBaseEntity *pTouch = CBaseEntity::Instance( INDEXENT( pe->info ));
	trace_t trace;

	if( !pTouch )
	{
		// removed entity?
		tr->allsolid = false;
		return;
	}

	// make trace default
	memset( &trace, 0, sizeof( trace ));
	trace.allsolid = true;
	trace.fraction = 1.0f;
	trace.endpos = end;

	WorldPhysic->SweepTest( pTouch, start, mins, maxs, end, &trace );

	// convert trace_t into pmtrace_t
	tr->allsolid = trace.allsolid;
	tr->startsolid = trace.startsolid;
	tr->inopen = trace.inopen;
	tr->inwater = trace.inwater;
	tr->fraction = trace.fraction;
	tr->endpos = trace.endpos;
	tr->plane.dist = trace.plane.dist;
	tr->plane.normal = trace.plane.normal;
	tr->materialHash = trace.materialHash;

	if (trace.ent != nullptr && PM_GetPlayerMove())
		tr->ent = pe - PM_GetPlayerMove()->physents;
	else 
		tr->ent = -1;
}

int SV_RestoreDecal(decallist_t *entry, edict_t *pEdict, qboolean adjacent)
{
	int	flags = entry->flags;
	int	entityIndex = ENTINDEX(pEdict);
	int	cacheID = 0, modelIndex = 0;
	Vector	scale = g_vecZero;

	if (flags & FDECAL_STUDIO)
	{
		if (FBitSet(pEdict->v.iuser1, CF_STATIC_ENTITY))
		{
			cacheID = pEdict->v.colormap;
			scale = pEdict->v.startpos;
		}

		UTIL_RestoreStudioDecal(entry->position, entry->impactPlaneNormal, entityIndex,
			pEdict->v.modelindex, entry->name, flags, &entry->studio_state, cacheID, scale);
		return TRUE;
	}

	if (adjacent && entry->entityIndex != 0 && (!pEdict || pEdict->free))
	{
		TraceResult tr;

		ALERT(at_error, "couldn't restore entity index %i\n", entry->entityIndex);

		Vector testspot = entry->position + entry->impactPlaneNormal * 5.0f;
		Vector testend = entry->position + entry->impactPlaneNormal * -5.0f;

		UTIL_TraceLine(testspot, testend, ignore_monsters, NULL, &tr);

		// NOTE: this code may does wrong result on moving brushes e.g. func_tracktrain
		if (tr.flFraction != 1.0f && !tr.fAllSolid)
		{
			// check impact plane normal
			float	dot = DotProduct(entry->impactPlaneNormal, tr.vecPlaneNormal);

			if (dot >= 0.95f)
			{
				entityIndex = ENTINDEX(tr.pHit);
				if (entityIndex > 0) modelIndex = tr.pHit->v.modelindex;
				UTIL_RestoreCustomDecal(tr.vecEndPos, entry->impactPlaneNormal, entityIndex, modelIndex, entry->name, flags, entry->scale);
			}
		}
	}
	else
	{
		// global entity is exist on new level so we can apply decal in local space
		// NOTE: this case also used for transition world decals
		UTIL_RestoreCustomDecal(entry->position, entry->impactPlaneNormal, entityIndex, pEdict->v.modelindex, entry->name, flags, entry->scale);
	}

	return TRUE;
}

// handle player touching ents
void PM_PlayerTouch( playermove_t *pmove, edict_t *client )
{
	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity :: Instance( client );

	if( !pPlayer || !pmove ) return; // ???

	// touch triggers
	pPlayer->RelinkEntity( true );

	// save original velocity
	Vector oldAbsVelocity = pPlayer->GetAbsVelocity();

	// touch other objects
	for( int i = 0; i < pmove->numtouch; i++ )
	{
		pmtrace_t *pmtrace = &pmove->touchindex[i];
		CBaseEntity *pTouch = CBaseEntity :: Instance( INDEXENT( pmove->physents[pmtrace->ent].info ) );

		// touch himself?
		if( !pTouch || pTouch == pPlayer )
			continue;

		// set momentum velocity to allow player pushing boxes
		pPlayer->SetAbsVelocity( pmtrace->deltavelocity );

		TraceResult tr;

		tr.fAllSolid = pmtrace->allsolid;
		tr.fStartSolid = pmtrace->startsolid;
		tr.fInOpen = pmtrace->inopen;
		tr.fInWater = pmtrace->inwater;
		tr.flFraction = pmtrace->fraction;
		tr.vecEndPos = pmtrace->endpos;
		tr.flPlaneDist = pmtrace->plane.dist;
		tr.vecPlaneNormal = pmtrace->plane.normal;
		tr.iHitgroup = pmtrace->hitgroup;
		tr.pHit = pTouch->edict();

		// IMPORTANT: don't change order!
		SV_Impact( pTouch, pPlayer, &tr );
	}

	// restore velocity
	pPlayer->SetAbsVelocity( oldAbsVelocity );
}

string_t SV_AllocString( const char *szValue )
{
	return g_GameStringPool.AllocString( szValue );
}

string_t SV_MakeString( const char *szValue )
{
	return g_GameStringPool.AllocString( szValue );
}

const char *SV_GetString( string_t iString )
{
	return g_GameStringPool.FindString( iString );
}

void PrepWorldFrame( void )
{
	WorldPhysic->EndFrame();
}

void SV_ProcessModelData(model_t *mod, qboolean create, const byte *buffer)
{
	if (FBitSet(mod->flags, MODEL_WORLD)) {
		SV_ProcessWorldData(mod, create, buffer);
	}

	if (mod->type == mod_studio)
	{
		if (create)
		{
			CRC32_t ulCrc;
			studiohdr_t *src = (studiohdr_t *)buffer;
			CRC32_INIT(&ulCrc);
			CRC32_PROCESS_BUFFER(&ulCrc, (byte *)buffer, src->length);
			mod->modelCRC = CRC32_FINAL(ulCrc);
		}
		else {
			// here you can deallocate stuff which contained in model_t
		}
	}
}

//
// Xash3D physics interface
//
static physics_interface_t gPhysicsInterface = 
{
	SV_PHYSICS_INTERFACE_VERSION,
	DispatchCreateEntity,
	DispatchPhysicsEntity,
	DispatchSpawnEntities,
	DispatchUpdatePlayerBaseVelocity,
	NULL,	// not needs
	NULL,	// not needs
	EngineSetFeatures,
	DrawDebugTriangles,
	PrepWorldFrame,
	DrawOrthoTriangles,
	SV_ClipMoveToEntity,
	SV_ClipPMoveToEntity,
	EndFrame,
	NULL, // obsolete
	DispatchCreateEntitiesInRestoreList,
#ifdef HAVE_STRINGPOOL
	SV_AllocString,
	SV_MakeString,
	SV_GetString,
#else
	NULL,
	NULL,
	NULL,
#endif
	SV_RestoreDecal,
	PM_PlayerTouch,
	SV_ProcessModelData
};

int Server_GetPhysicsInterface( int iVersion, server_physics_api_t *pfuncsFromEngine, physics_interface_t *pFunctionTable )
{
	if ( !pFunctionTable || !pfuncsFromEngine || iVersion != SV_PHYSICS_INTERFACE_VERSION )
	{
		return FALSE;
	}

	size_t iExportSize = sizeof( server_physics_api_t );
	size_t iImportSize = sizeof( physics_interface_t );

	// copy new physics interface
	memcpy( &g_physfuncs, pfuncsFromEngine, iExportSize );

	// fill engine callbacks
	memcpy( pFunctionTable, &gPhysicsInterface, iImportSize );

	g_fPhysicInitialized = TRUE;

	return TRUE;
}

//
// attempt to create custom entity when default method is failed
// 0 - attempt to create, -1 - reject to create
//
int DispatchCreateEntity( edict_t *pent, const char *szName )
{
	return -1;
}

//
// run custom physics for each entity
// return 0 to use built-in engine physic
//
int DispatchPhysicsEntity( edict_t *pEdict )
{
	CBaseEntity *pEntity = CBaseEntity :: Instance( pEdict );

	if( !pEntity )
		return 0;	// not initialized

	if( RunPhysicsFrame( pEntity ))
	{
		// g-cont. don't alow free entities during loading because
		// this produce a corrupted baselines
		if( GET_SERVER_STATE() == SERVER_ACTIVE )
		{
			// g-cont. impulse 101 issues
			if( !pEdict->free && pEntity->pev->flags & FL_KILLME )
				REMOVE_ENTITY( pEdict );
		}
		return 1;
	}

	return 0;
}

void DispatchUpdatePlayerBaseVelocity( edict_t *pEdict )
{
	CBaseEntity *pEntity = CBaseEntity :: Instance( pEdict );

	if( !pEntity )
		return;

	SV_UpdateBaseVelocity( pEntity );
}

static void AddEntityToBlockingList( CBaseEntity *pPushed )
{
	g_pPushedEntities->AddPushedEntityToBlockingList( pPushed );
}

/*
=============
SV_AngularMove

may use friction for smooth stopping
=============
*/
void SV_AngularMove( CBaseEntity *pEntity, float frametime, float friction )
{
	Vector angles = pEntity->GetLocalAngles();
	Vector amove = pEntity->GetLocalAvelocity() * frametime;

	angles += amove;
	pEntity->SetLocalAngles( angles );

	if( friction == 0.0f || pEntity->GetLocalAvelocity() == g_vecZero )
		return;

	float game_friction = CVAR_GET_FLOAT( "sv_friction" );
	float game_stopspeed = CVAR_GET_FLOAT( "sv_stopspeed" );
	float adjustment = frametime * (game_stopspeed / 10) * game_friction * fabs( friction );
	Vector avelocity = pEntity->GetLocalAvelocity();

	for( int i = 0; i < 3; i++ )
	{
		if( avelocity[i] > 0.0f )
		{
			avelocity[i] -= adjustment;
			if( avelocity[i] < 0.0f ) avelocity[i] = 0.0f;
		}
		else
		{
			avelocity[i] += adjustment;
			if( avelocity[i] > 0.0f ) avelocity[i] = 0.0f;
		}
	}

	pEntity->SetLocalAvelocity( avelocity );
}

/*
=============
SV_LinearMove

use friction for smooth stopping
=============
*/
void SV_LinearMove( CBaseEntity *pEntity, float frametime, float friction )
{
	Vector origin = pEntity->GetLocalOrigin();
	Vector move = pEntity->GetLocalVelocity() * frametime;

	origin += move;
	pEntity->SetLocalOrigin( origin );

	if( friction == 0.0f || pEntity->GetAbsVelocity() == g_vecZero )
		return;

	float game_friction = CVAR_GET_FLOAT( "sv_friction" );
	float game_stopspeed = CVAR_GET_FLOAT( "sv_stopspeed" );
	float adjustment = frametime * (game_stopspeed / 10) * game_friction * fabs( friction );
	Vector velocity = pEntity->GetLocalVelocity();

	for( int i = 0; i < 3; i++ )
	{
		if( velocity[i] > 0.0f )
		{
			velocity[i] -= adjustment;
			if( velocity[i] < 0.0f ) velocity[i] = 0.0f;
		}
		else
		{
			velocity[i] += adjustment;
			if( velocity[i] > 0.0f ) velocity[i] = 0.0f;
		}
	}

	pEntity->SetLocalVelocity( velocity );
}

//-----------------------------------------------------------------------------
// Purpose: Store off entity and copy original origin to temporary array
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::AddEntity( CBaseEntity *ent )
{
	int i = m_rgMoved.AddToTail();
	m_rgMoved[i].m_pEntity = ent;
	m_rgMoved[i].m_vecStartAbsOrigin = ent->GetAbsOrigin();
}

//-----------------------------------------------------------------------------
// Unlink + relink the pusher list so we can actually do the push
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::UnlinkPusherList( void )
{
	for( int i = m_rgPusher.Count(); --i >= 0; )
	{
		CBaseEntity *pEnt = m_rgPusher[i].m_pEntity;
		pEnt->MakeNonSolid();
	}
}

void CPhysicsPushedEntities::RelinkPusherList( void )
{
	for( int i = m_rgPusher.Count(); --i >= 0; )
	{
		CBaseEntity *pEnt = m_rgPusher[i].m_pEntity;
		pEnt->RestoreSolid();
		pEnt->RelinkEntity( FALSE );
	}
}

//-----------------------------------------------------------------------------
// Compute the direction to move the rotation blocker
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::CalcRotationalPushDirection( CBaseEntity *pBlocker, const RotatingPushMove_t &rotMove, Vector &pMove, CBaseEntity *pRoot )
{
	// calculate destination position
	// "start" is relative to the *root* pusher, world orientation
	Vector start;

	if( !SV_AllowPushRotate( pBlocker ) || pBlocker->pev->movetype == MOVETYPE_STEP )
		start = pBlocker->Center();
	else start = pBlocker->GetAbsOrigin();

	// org is pusher local coordinate of start

	// transform starting point into local space
	Vector local = rotMove.startLocalToWorld.VectorITransform( start );

	// rotate local org into world space at end of rotation
	Vector end = rotMove.endLocalToWorld.VectorTransform( local );

	// move is the difference (in world space) that the move will push this object
	pMove = end - start;
}

bool CPhysicsPushedEntities::IsPushedPositionValid( CBaseEntity *pBlocker )
{
	TraceResult trace;

	UTIL_TraceEntity( pBlocker, pBlocker->GetAbsOrigin(), pBlocker->GetAbsOrigin(), &trace );

	return !trace.fStartSolid;
}

//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::SpeculativelyCheckPush( PhysicsPushedInfo_t &info, const Vector &srcAbsPush, bool bRotationalPush )
{
	CBaseEntity *pBlocker = info.m_pEntity;
	edict_t *ent = pBlocker->edict();
	Vector vecAbsPush = srcAbsPush;

	// See if it's possible to move the entity, but disable all pushers in the hierarchy first
	UnlinkPusherList();

	// i can't clear FL_ONGROUND in all cases because many bad things may be happen
	if( pBlocker->pev->movetype != MOVETYPE_WALK && bRotationalPush )
	{
		if( vecAbsPush[2] < 0.0f && !m_rgPusher[0].m_pEntity->pev->dmg )
			vecAbsPush[2] = 0.0f; // let's the free falling
	}

	Vector pushDestPosition = pBlocker->GetAbsOrigin() + vecAbsPush;

	UTIL_TraceEntity( pBlocker, pBlocker->GetAbsOrigin(), pushDestPosition, &info.m_trace );

	RelinkPusherList();

	info.m_bPusherIsGround = false;

	// remove the onground flag for non-players
	if( pBlocker->pev->movetype != MOVETYPE_WALK && !bRotationalPush )
		pBlocker->pev->flags &= ~FL_ONGROUND;

	if( pBlocker->GetGroundEntity() && pBlocker->GetGroundEntity()->GetRootParent() == m_rgPusher[0].m_pEntity )
	{
		info.m_bPusherIsGround = true;
	}

	bool bIsDynamic = ( pBlocker->m_iActorType == ACTOR_DYNAMIC ) ? true : false;
	bool bIsCharacter = (pBlocker->IsPlayer() || pBlocker->MyMonsterPointer()) ? true : false;
	bool bIsUnblockable = (m_bIsUnblockableByPlayer && bIsCharacter) ? true : false;
	bool bIsBlocked = (pushDestPosition != info.m_trace.vecEndPos) ? true : false;

	if( pBlocker->pev->movetype != MOVETYPE_WALK )
	{
		if( bRotationalPush && ( bIsBlocked || srcAbsPush.z != 0.0f ))
			pBlocker->pev->flags &= ~FL_ONGROUND;
	}

	if( bIsUnblockable )
	{
		pBlocker->SetAbsOrigin( pushDestPosition );
	}
	else
	{
		// move the blocker into its new position
		if( info.m_trace.flFraction )
		{
			if( !info.m_trace.fAllSolid )
			{
				pBlocker->SetAbsOrigin( info.m_trace.vecEndPos );
			}
			else if( pBlocker->m_iActorType == ACTOR_DYNAMIC && !info.m_bPusherIsGround && !bRotationalPush )
			{
				return false;
			}
		}

		// we're not blocked if the blocker is point-sized or non-solid
		// dynamic actors also doesn't block way since all pushables are kinematic actors
		// and they just can push away other dynamic actors despite of obstacles
		if( pBlocker->IsPointSized() || pBlocker->pev->solid == SOLID_NOT || bIsDynamic )
		{
			return true;
		}

		// we're not blocked if the blocker is dead body or trigger
		if( pBlocker->pev->deadflag >= DEAD_DEAD || pBlocker->pev->solid == SOLID_TRIGGER )
		{
			return true;
		}

		if(( !bRotationalPush || bIsDynamic || bIsCharacter ) && ( pBlocker->GetAbsOrigin() == pushDestPosition ))
		{
			if( !info.m_bPusherIsGround && !IsPushedPositionValid( pBlocker ))
			{
				if( !bIsDynamic )
				{
					ALERT( at_warning, "Interpenetrating entities! (%s and %s)\n",
					pBlocker->GetClassname(), m_rgPusher[0].m_pEntity->GetClassname() );
				}
			}
			return true;
		}
	}

	// check to see if we're still blocked by the pushers
	// FIXME: If the trace fraction == 0 can we early out also?
	info.m_bBlocked = !IsPushedPositionValid( pBlocker );

	if( !info.m_bBlocked /* && !bIsBlocked*/ )
		return true;

	// if the player is blocking the train try nudging him around to fix accumulated error
	if( bIsUnblockable )
	{
		Vector org = pBlocker->GetAbsOrigin();

		for( int checkCount = 0; checkCount < 4; checkCount++ )
		{
			Vector move = m_rgPusher[0].m_pEntity->EntityToWorldTransform()[checkCount>>1];
			
			// alternate movements 1/2" in each direction
			float factor = ( checkCount & 1 ) ? -0.5f : 0.5f;
			pBlocker->SetAbsOrigin( org + move * factor );
			info.m_bBlocked = !IsPushedPositionValid( pBlocker );

			if( !info.m_bBlocked )
				return true;
		}

		pBlocker->SetAbsOrigin( pushDestPosition );
		ALERT( at_aiconsole, "Ignoring player blocking train!\n" );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::SpeculativelyCheckRotPush( const RotatingPushMove_t &rotPushMove, CBaseEntity *pRoot )
{
	Vector vecAbsPush;
	m_nBlocker = -1;

	for( int i = m_rgMoved.Count(); --i >= 0; )
	{
		CalcRotationalPushDirection( m_rgMoved[i].m_pEntity, rotPushMove, vecAbsPush, pRoot );

		if( !SpeculativelyCheckPush( m_rgMoved[i], vecAbsPush, true ))
		{
			m_nBlocker = i;
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Speculatively checks to see if all entities in this list can be pushed
//-----------------------------------------------------------------------------
bool CPhysicsPushedEntities::SpeculativelyCheckLinearPush( const Vector &vecAbsPush )
{
	m_nBlocker = -1;

	for( int i = m_rgMoved.Count(); --i >= 0; )
	{
		if( !SpeculativelyCheckPush( m_rgMoved[i], vecAbsPush, false ))
		{
			m_nBlocker = i;
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::FinishPushers()
{
	// We succeeded! Now that we know the final location of all entities,
	// touch triggers + update physics objects + do other fixup
	for( int i = m_rgPusher.Count(); --i >= 0; )
	{
		PhysicsPusherInfo_t &info = m_rgPusher[i];

		// cause touch functions to be called
		info.m_pEntity->RelinkEntity( TRUE, &info.m_vecStartAbsOrigin );

		// kinematic pushers that linked with parent system
		if( info.m_pEntity->m_iActorType == ACTOR_KINEMATIC )
		{
			WorldPhysic->MoveKinematic( info.m_pEntity );
		}
	}
}

//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::FinishRotPushedEntity( CBaseEntity *pPushedEntity, const RotatingPushMove_t &rotPushMove )
{
	// impart angular velocity of push onto pushed objects
	if( pPushedEntity->IsNetClient( ))
	{
		if( rotPushMove.amove[YAW] != 0.0f )
		{
			pPushedEntity->pev->avelocity[YAW] += rotPushMove.amove[YAW];
			SetBits( pPushedEntity->pev->fixangle, 2 );
		}
	}

	Vector angles = pPushedEntity->GetAbsAngles();

	// update goal if monster try turning on the rotational platform
	if( FBitSet( pPushedEntity->pev->flags, FL_MONSTER ))
		pPushedEntity->pev->ideal_yaw += rotPushMove.amove[YAW];
		
	// only rotate YAW with pushing.
	angles.y += rotPushMove.amove.y;
	pPushedEntity->SetAbsAngles( angles );

	// keep gait yaw is actual
	if( pPushedEntity->IsPlayer( ) && rotPushMove.amove.y != 0.0f )
		pPushedEntity->m_flGaitYaw = pPushedEntity->pev->angles[YAW];
}

//-----------------------------------------------------------------------------
// Causes all entities in the list to touch triggers from their prev position
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::FinishPush( bool bIsRotPush, const RotatingPushMove_t *pRotPushMove )
{
	FinishPushers();

	for( int i = m_rgMoved.Count(); --i >= 0; )
	{
		PhysicsPushedInfo_t &info = m_rgMoved[i];
		CBaseEntity *pPushedEntity = info.m_pEntity;

		// cause touch functions to be called
		info.m_pEntity->RelinkEntity( TRUE, &info.m_vecStartAbsOrigin );
		info.m_pEntity->m_iFlags |= MF_GROUNDMOVE;

		// register physics impacts...
		if( info.m_trace.pHit )
		{
			SV_Impact( pPushedEntity, NULL, &info.m_trace );
		}

		if( bIsRotPush )
		{
			FinishRotPushedEntity( pPushedEntity, *pRotPushMove );
		}

		if( info.m_pEntity->m_iActorType == ACTOR_CHARACTER )
		{
			// NPC and player update shadow
			WorldPhysic->MoveCharacter( info.m_pEntity );
		}
	}
}

// save initial state when beginning a push sequence
void CPhysicsPushedEntities::BeginPush( CBaseEntity *pRoot )
{
	m_rgMoved.RemoveAll();
	m_rgPusher.RemoveAll();
}

//-----------------------------------------------------------------------------
// Registers a blockage
//-----------------------------------------------------------------------------
CBaseEntity *CPhysicsPushedEntities::RegisterBlockage( void )
{
	ASSERT( m_nBlocker >= 0 );

	// generate a PhysicsImpact against the blocker...
	PhysicsPushedInfo_t &info = m_rgMoved[m_nBlocker];

	if( info.m_trace.pHit )
	{
		SV_Impact( info.m_pEntity, NULL, &info.m_trace );
	}

	// this is the dude 
	return info.m_pEntity;
}

//-----------------------------------------------------------------------------
// Purpose: Restore entities that might have been moved
// Input  : fromrotation - if the move is from a rotation, then angular move must also be reverted
//			*amove - 
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::RestoreEntities( void )
{
	// reset all of the pushed entities to get them back into place also
	for( int i = m_rgMoved.Count(); --i >= 0; )
	{
		m_rgMoved[i].m_pEntity->SetAbsOrigin( m_rgMoved[i].m_vecStartAbsOrigin );
	}
}

bool CPhysicsPushedEntities::IsStandingOnPusher( CBaseEntity *pCheck )
{
	CBaseEntity *pGroundEnt = pCheck->GetGroundEntity();

	if( pCheck->pev->flags & FL_ONGROUND || pGroundEnt )
	{
		for( int i = m_rgPusher.Count(); --i >= 0; )
		{
			if( m_rgPusher[i].m_pEntity == pGroundEnt )
				return true;
		}
	}

	return false;
}

void CPhysicsPushedEntities::AddPushedEntityToBlockingList( CBaseEntity *pPushed )
{
	if( !pPushed ) return;

	if( pPushed->m_iPushEnumCount == s_nEnumCount )
		return;	// already added

	if( pPushed->pev->solid == SOLID_NOT )
		return;

	// filter movetypes to collide with
	switch( pPushed->pev->movetype )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_FOLLOW:
	case MOVETYPE_NOCLIP:
	case MOVETYPE_COMPOUND:
		return;
	}

	// All elements are part of the same hierarchy, so they all have
	// the same root, so it doesn't matter which one we grab
	CBaseEntity *pRootHighestParent = m_rgPusher[0].m_pEntity->GetRootParent();
	CBaseEntity *pCheckHighestParent = pPushed->GetRootParent();

	if( pCheckHighestParent == pRootHighestParent )
	{
		// g-cont. attached pushables can be block hierarchy!
		if( pPushed->pev->movetype != MOVETYPE_PUSHSTEP )
			return;
	}

	// If we're standing on the pusher or any rigidly attached child
	// of the pusher, we don't need to bother checking for interpenetration
	if( !IsStandingOnPusher( pPushed ))
	{
		// Our surrounding boxes are touching. But we may well not be colliding....
		// see if the ent's bbox is inside the pusher's final position
		if( !SV_TestEntityPosition( pPushed, NULL ))
			return;
	}

	pCheckHighestParent->m_iPushEnumCount = s_nEnumCount;

	// NOTE: This is pretty tricky here. If a rigidly attached child comes into
	// contact with a pusher, we *cannot* push the child. Instead, we must push
	// the highest parent of that child.
	AddEntity( pCheckHighestParent );
}

//-----------------------------------------------------------------------------
// Generates a list of potential blocking entities
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::GenerateBlockingEntityList()
{
	m_rgMoved.RemoveAll();
	s_nEnumCount++;

	for( int i = m_rgPusher.Count(); --i >= 0;  )
	{
		CBaseEntity *pPusher = m_rgPusher[i].m_pEntity;

		// don't bother if the pusher isn't solid
		if( pPusher->pev->solid == SOLID_NOT && !FBitSet( pPusher->m_iFlags, MF_LADDER ))
			continue;

		Vector vecAbsMins, vecAbsMaxs;
		pPusher->WorldSpaceAABB( vecAbsMins, vecAbsMaxs );

		UTIL_AreaNode( vecAbsMins, vecAbsMaxs, AREA_SOLID, AddEntityToBlockingList );
		UTIL_AreaNode( vecAbsMins, vecAbsMaxs, AREA_TRIGGERS, AddEntityToBlockingList );
	}
}

//-----------------------------------------------------------------------------
// Generates a list of potential blocking entities
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::GenerateBlockingEntityListAddBox( const Vector &vecMoved )
{
	m_rgMoved.RemoveAll();
	s_nEnumCount++;

	for( int i = m_rgPusher.Count(); --i >= 0;  )
	{
		CBaseEntity *pPusher = m_rgPusher[i].m_pEntity;

		// don't bother if the pusher isn't solid
		if( pPusher->pev->solid == SOLID_NOT && !FBitSet( pPusher->m_iFlags, MF_LADDER ))
			continue;

		Vector vecAbsMins, vecAbsMaxs;
		pPusher->WorldSpaceAABB( vecAbsMins, vecAbsMaxs );

		for( int iAxis = 0; iAxis < 3; iAxis++ )
		{
			if( vecMoved[iAxis] >= 0.0f )
				vecAbsMins[iAxis] -= vecMoved[iAxis];
			else vecAbsMaxs[iAxis] -= vecMoved[iAxis];
		}

		UTIL_AreaNode( vecAbsMins, vecAbsMaxs, AREA_SOLID, AddEntityToBlockingList );
		UTIL_AreaNode( vecAbsMins, vecAbsMaxs, AREA_TRIGGERS, AddEntityToBlockingList );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets a list of all entities hierarchically attached to the root 
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::SetupAllInHierarchy( CBaseEntity *pParent )
{
	if( !pParent ) return;

	// Make sure to snack the position +before+ relink because applying the
	// rotation (which occurs in relink) will put it at the final location
	// NOTE: The root object at this point is actually at its final position.
	// We'll fix that up later
	int i = m_rgPusher.AddToTail();
	m_rgPusher[i].m_pEntity = pParent;
	m_rgPusher[i].m_vecStartAbsOrigin = pParent->GetAbsOrigin();

	for( CBaseEntity *pChild = pParent->m_hChild; pChild != NULL; pChild = pChild->m_hNextChild )
	{
		SetupAllInHierarchy( pChild );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Rotates the root entity, fills in the pushmove structure
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::RotateRootEntity( CBaseEntity *pRoot, float movetime, RotatingPushMove_t &rotation )
{
	rotation.amove = pRoot->GetLocalAvelocity() * movetime;
	rotation.origin = pRoot->GetAbsOrigin();

	// knowing the initial + ending basis is needed for determining
	// which corner we're pushing 
	rotation.startLocalToWorld = pRoot->EntityToWorldTransform();

	if( pRoot->pev->movetype != MOVETYPE_VEHICLE )
	{
		// rotate the pusher to it's final position
		SV_AngularMove( pRoot, movetime, pRoot->pev->friction );
	}	

	// compute the change in absangles
	rotation.endLocalToWorld = pRoot->EntityToWorldTransform();
}

//-----------------------------------------------------------------------------
// Purpose: Tries to rotate an entity hierarchy, returns the blocker if any
//-----------------------------------------------------------------------------
CBaseEntity *CPhysicsPushedEntities::PerformRotatePush( CBaseEntity *pRoot, float movetime )
{
	m_bIsUnblockableByPlayer = FBitSet( pRoot->pev->flags, FL_UNBLOCKABLE ) ? true : false;

	// build a list of this entity + all its children because we're going to try to move them all
	// this will also make sure each entity is linked in the appropriate place
	// with correct absboxes
	m_rgPusher.RemoveAll();
	SetupAllInHierarchy( pRoot );

	// save where we rotated from, in case we're blocked
	Vector angPrevAngles = pRoot->GetLocalAngles();

	// apply the rotation
	RotatingPushMove_t rotPushMove;
	RotateRootEntity( pRoot, movetime, rotPushMove );

	// next generate a list of all entities that could potentially be intersecting with
	// any of the children in their new locations...
	GenerateBlockingEntityList( );

	// now we have a unique list of things that could potentially block our push
	// and need to be pushed out of the way. Lets try to push them all out of the way.
	// if we fail, undo it all
	if( !SpeculativelyCheckRotPush( rotPushMove, pRoot ))
	{
		CBaseEntity *pBlocker = RegisterBlockage();
		if( pRoot->pev->movetype != MOVETYPE_VEHICLE )
			pRoot->SetLocalAngles( angPrevAngles );
		RestoreEntities( );
		return pBlocker;
	}

	FinishPush( true, &rotPushMove );

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Linearly moves the root entity
//-----------------------------------------------------------------------------
void CPhysicsPushedEntities::LinearlyMoveRootEntity( CBaseEntity *pRoot, float movetime, Vector &pAbsPushVector )
{
	if( pRoot->pev->movetype != MOVETYPE_VEHICLE )
	{
		// move the pusher to it's final position
		SV_LinearMove( pRoot, movetime, pRoot->pev->friction );
	}

	// Store off the abs push vector
	pAbsPushVector = pRoot->GetAbsVelocity() * movetime;
}


//-----------------------------------------------------------------------------
// Purpose: Tries to linearly push an entity hierarchy, returns the blocker if any
//-----------------------------------------------------------------------------
CBaseEntity *CPhysicsPushedEntities::PerformLinearPush( CBaseEntity *pRoot, float movetime )
{
	m_bIsUnblockableByPlayer = FBitSet( pRoot->pev->flags, FL_UNBLOCKABLE ) ? true : false;

	// build a list of this entity + all its children because we're going to try to move them all
	// this will also make sure each entity is linked in the appropriate place
	// with correct absboxes
	m_rgPusher.RemoveAll();
	SetupAllInHierarchy( pRoot );

	// save where we started from, in case we're blocked
	Vector vecPrevOrigin = pRoot->GetAbsOrigin();

	// Move the root (and all children) into its new position
	Vector vecAbsPush;
	LinearlyMoveRootEntity( pRoot, movetime, vecAbsPush );

	// Next generate a list of all entities that could potentially be intersecting with
	// any of the children in their new locations...
	GenerateBlockingEntityListAddBox( vecAbsPush );

	// Now we have a unique list of things that could potentially block our push
	// and need to be pushed out of the way. Lets try to push them all out of the way.
	// if we fail, undo it all
	if( !SpeculativelyCheckLinearPush( vecAbsPush ))
	{
		CBaseEntity *pBlocker = RegisterBlockage();
		if( pRoot->pev->movetype != MOVETYPE_VEHICLE )
			pRoot->SetAbsOrigin( vecPrevOrigin );
		RestoreEntities();
		return pBlocker;
	}

	FinishPush();

	return NULL;
}

/*
================
SV_UpdateBaseVelocity
================
*/
void SV_UpdateBaseVelocity( CBaseEntity *pEntity )
{
	if( pEntity->pev->flags & FL_ONGROUND )
	{
		CBaseEntity *pGround = pEntity->GetGroundEntity();

		if( pGround )
		{
			// On conveyor belt that's moving?
			if( pGround->pev->flags & FL_CONVEYOR )
			{
				Vector	new_basevel, m_vecPushDir;

				if( pGround->m_hParent != NULL )
					m_vecPushDir = pGround->EntityToWorldTransform().VectorRotate( pGround->pev->movedir );
				else m_vecPushDir = pGround->pev->movedir;

				new_basevel = m_vecPushDir * pGround->pev->speed;
				if( pEntity->pev->flags & FL_BASEVELOCITY )
					new_basevel += pEntity->GetBaseVelocity();

				pEntity->pev->flags |= FL_BASEVELOCITY;
				pEntity->pev->basevelocity = new_basevel;
			}
		}
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
BOOL SV_RunThink( CBaseEntity *pEntity )
{
	float	thinktime;
	edict_t	*ed = pEntity->edict();

	thinktime = pEntity->pev->nextthink;

	if( thinktime <= 0 || thinktime > PHYSICS_TIME( ) + gpGlobals->frametime )
		return TRUE;
		
	if( thinktime < PHYSICS_TIME( ))
		thinktime = PHYSICS_TIME();	// don't let things stay in the past.
					// it is possible to start that way
					// by a trigger with a local time.
	pEntity->DontThink();

	gpGlobals->time = thinktime;	// ouch!!!
	DispatchThink( pEntity->edict() );

	return (ed->free) ? FALSE : TRUE;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact( CBaseEntity *pEntity1, CBaseEntity *pEntity2, TraceResult *trace )
{
	// get it from trace
	if( !pEntity2 ) pEntity2 = CBaseEntity::Instance( trace->pHit );

	gpGlobals->time = PHYSICS_TIME(); // ouch!

	if(( pEntity1->pev->flags|pEntity2->pev->flags ) & FL_KILLME )
		return;

	// group trace support
	if( pEntity1->pev->groupinfo && pEntity2->pev->groupinfo )
	{
		if(( g_groupop == 0 && !( pEntity1->pev->groupinfo & pEntity2->pev->groupinfo )) ||
		   ( g_groupop == 1 && ( pEntity1->pev->groupinfo & pEntity2->pev->groupinfo )))
			return;
	}

	if( pEntity1->pev->solid != SOLID_NOT )
	{
		UTIL_CopyTraceToGlobal( trace );
		DispatchTouch( pEntity1->edict(), pEntity2->edict());
	}

	if( pEntity2->pev->solid != SOLID_NOT )
	{
		UTIL_CopyTraceToGlobal( trace );
		DispatchTouch( pEntity2->edict(), pEntity1->edict());
	}
}

/*
============
SV_TestEntityPosition

returns true if the entity is in solid currently
============
*/
BOOL SV_TestEntityPosition( CBaseEntity *pEntity, CBaseEntity *pBlocker )
{
	TraceResult trace;

	if( pEntity->IsPlayer( ))
	{
		// to avoid falling through tracktrain update client mins\maxs here
		if( pEntity->pev->flags & FL_DUCKING ) 
			UTIL_SetSize( pEntity->pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
		else UTIL_SetSize( pEntity->pev, VEC_HULL_MIN, VEC_HULL_MAX );
	}

	gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
	TRACE_MONSTER_HULL( pEntity->edict(), pEntity->GetAbsOrigin(), pEntity->GetAbsOrigin(), dont_ignore_monsters, pEntity->edict(), &trace );

	if( pBlocker && trace.pHit )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( trace.pHit );
		if( pHit->pev->movetype == MOVETYPE_PUSH || pHit == pBlocker )
			return trace.fStartSolid;
		return FALSE;
	}

	return trace.fStartSolid;
}

/*
=============
SV_RecursiveWaterLevel

recursively recalculating the middle
=============
*/
float SV_RecursiveWaterLevel( const Vector &origin, float mins, float maxs, int depth )
{
	float waterlevel = ((mins - maxs) * 0.5f) + maxs;
	if( ++depth > 5 ) return waterlevel;

	Vector point = Vector( origin.x, origin.y, origin.z + waterlevel );

	if( UTIL_PointContents( point ) == CONTENTS_WATER )
		return SV_RecursiveWaterLevel( origin, mins, waterlevel, depth );
	return SV_RecursiveWaterLevel( origin, waterlevel, maxs, depth );
}

/*
=============
SV_Submerged

determine how deep the entity is
=============
*/
float SV_Submerged( CBaseEntity *pEntity )
{
	Vector	point;

	Vector halfmax = (pEntity->pev->absmin + pEntity->pev->absmax) * 0.5f;
	float waterlevel = pEntity->pev->absmin.z - halfmax.z;

	switch( pEntity->pev->waterlevel )
	{
	case 1:
		return SV_RecursiveWaterLevel( halfmax, 0.0f, waterlevel, 0 ) - waterlevel;
	case 3:
		point = Vector( halfmax.x, halfmax.y, pEntity->pev->absmax.z );

		if( UTIL_PointContents( point ) == CONTENTS_WATER )
		{
			return (pEntity->pev->maxs.z - pEntity->pev->mins.z);
		}
		// intentionally fallthrough
	case 2:
		return SV_RecursiveWaterLevel( halfmax, pEntity->pev->absmax.z - halfmax.z, 0.0f, 0 ) - waterlevel;
	default:
		return 0.0f;
	}
}

/*
=============
SV_CheckMover

test thing (applies the friction to pushables while standing on moving platform)
=============
*/
BOOL SV_CheckMover( CBaseEntity *pEntity )
{
	CBaseEntity *pGround = pEntity->GetGroundEntity();

	if( !pGround )
		return false;

	if( pGround->pev->movetype != MOVETYPE_PUSH )
		return false;

	if( pGround->GetLocalVelocity() == g_vecZero && pGround->GetLocalAvelocity() == g_vecZero )
		return false;

	return true;
}

/*
=============
SV_CheckWater
=============
*/
BOOL SV_CheckWater( CBaseEntity *pEntity )
{
	Vector	point;

	point.x = (pEntity->pev->absmin.x + pEntity->pev->absmax.x) * 0.5f;
	point.y = (pEntity->pev->absmin.y + pEntity->pev->absmax.y) * 0.5f;
	point.z =  pEntity->pev->absmin.z + 1;	

	pEntity->pev->waterlevel = 0;
	pEntity->pev->watertype = CONTENTS_EMPTY;

	int cont = UTIL_PointContents( point );

	if( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
	{
		pEntity->pev->watertype = cont;
		pEntity->pev->waterlevel = 1;

		if( pEntity->pev->absmin.z == pEntity->pev->absmax.z )
		{
			// a point entity
			pEntity->pev->waterlevel = 3;
		}
		else
		{
			point.z = (pEntity->pev->absmin.z + pEntity->pev->absmax.z) * 0.5f;

			cont = UTIL_PointContents( point );

			if( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
			{
				pEntity->pev->waterlevel = 2;

				point += pEntity->pev->view_ofs;
				cont = UTIL_PointContents( point );

				if( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
					pEntity->pev->waterlevel = 3;
			}
		}
	}

	return (pEntity->pev->waterlevel > 1);
}

/*
=============
SV_CheckWaterTransition

=============
*/
void SV_CheckWaterTransition( CBaseEntity *pEntity )
{
	int	cont;
	Vector	halfmax;

	halfmax.x = (pEntity->pev->absmax.x + pEntity->pev->absmin.x) * 0.5f;
	halfmax.y = (pEntity->pev->absmax.y + pEntity->pev->absmin.y) * 0.5f;
	halfmax.z =  pEntity->pev->absmin.z + 1.0f;

	cont = UTIL_PointContents( halfmax );

	if( !pEntity->pev->watertype )
	{
		// just spawned here
		pEntity->pev->watertype = cont;
		pEntity->pev->waterlevel = 1;
		return;
	}

	if( cont > CONTENTS_WATER || cont <= CONTENTS_TRANSLUCENT )
	{
		if( pEntity->pev->watertype != CONTENTS_EMPTY )
		{
			EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade2.wav", 1.0f, ATTN_NORM );
		}

		pEntity->pev->watertype = CONTENTS_EMPTY;
		pEntity->pev->waterlevel = 0;
		return;
	}

	if( pEntity->pev->watertype == CONTENTS_EMPTY )
	{
		EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade1.wav", 1.0f, ATTN_NORM );
		Vector vecAbsVelocity = pEntity->GetAbsVelocity();
		vecAbsVelocity.z *= 0.5f; // cut speed for floatables
		pEntity->SetAbsVelocity( vecAbsVelocity );
	}

	pEntity->pev->watertype = cont;
	pEntity->pev->waterlevel = 1;

	if( pEntity->pev->absmin.z == pEntity->pev->absmax.z )
	{
		// a point entity
		pEntity->pev->waterlevel = 3;
	}

	halfmax.z = (pEntity->pev->absmin.z + pEntity->pev->absmax.z) * 0.5f;

	cont = UTIL_PointContents( halfmax );

	if( cont > CONTENTS_WATER || cont <= CONTENTS_TRANSLUCENT )
		return;

	pEntity->pev->waterlevel = 2;
	halfmax += pEntity->pev->view_ofs;

	cont = UTIL_PointContents( halfmax );

	if( cont > CONTENTS_WATER || cont <= CONTENTS_TRANSLUCENT )
		return;

	pEntity->pev->waterlevel = 3;
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity( CBaseEntity *pEntity )
{
	Vector origin = pEntity->GetAbsOrigin();
	Vector vecAbsVelocity = pEntity->GetAbsVelocity();
	float maxVelocity = CVAR_GET_FLOAT( "sv_maxvelocity" );
	bool bReset = false;

	// bound velocity
	for( int i = 0; i < 3; i++ )
	{
		if( IS_NAN( vecAbsVelocity[i] ))
		{
			ALERT( at_console, "Got a NaN velocity on %s\n", pEntity->GetClassname( ));
			vecAbsVelocity[i] = 0.0f;
			bReset = true;
		}

		if( IS_NAN( origin[i] ))
		{
			ALERT( at_console, "Got a NaN origin on %s\n", pEntity->GetClassname( ));
			origin[i] = 0.0f;
			bReset = true;
		}

		if( vecAbsVelocity[i] > maxVelocity )
		{
			vecAbsVelocity[i] = maxVelocity;
			bReset = true;
		}
		else if( vecAbsVelocity[i] < -maxVelocity )
		{
			vecAbsVelocity[i] = -maxVelocity;
			bReset = true;
		}
	}

	if( bReset )
	{
		pEntity->SetAbsOrigin( origin );
		pEntity->SetAbsVelocity( vecAbsVelocity );
	}
}

/*
==================
SV_ClipVelocity

Slide off of the impacting object
==================
*/
int SV_ClipVelocity( const Vector &in, const Vector &normal, Vector &out, float overbounce )
{
	int blocked = 0;
	if( normal[2] > 0.0f ) blocked |= 1;	// floor
	if( !normal[2] ) blocked |= 2;	// step
	
	float backoff = DotProduct( in, normal ) * overbounce;

	for( int i = 0; i < 3; i++ )
	{
		float change = normal[i] * backoff;
		out[i] = in[i] - change;

		if( out[i] > -1.0f && out[i] < 1.0f )
			out[i] = 0.0f;
	}

	return blocked;
}

float SV_CalcOverBounce( CBaseEntity *pEntity, TraceResult *trace )
{
	float overbounce = 1.0f;

	CBaseEntity *pHit = CBaseEntity::Instance( trace->pHit );

	if( FNullEnt( pHit ))
		return overbounce;

	if( pEntity->pev->movetype == MOVETYPE_PUSHSTEP && pHit->IsPlayer() && pHit->pev->button & IN_BACK )
	{
		// g-cont. this is a player that pulling the 'func_pushable'
		// disable overbounce to avoid box slide along the player
		overbounce = 0.0f;
	}

	return overbounce;
}

/*
===============================================================================

	FLYING MOVEMENT CODE

===============================================================================
*/
/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
*steptrace - if not NULL, the trace results of any vertical wall hit will be stored
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
============
*/
int SV_FlyMove( CBaseEntity *pEntity, float time, TraceResult *steptrace )
{
	int i, j;
	Vector planes[MAX_CLIP_PLANES];
	Vector new_velocity;
	TraceResult trace;

	Vector original_velocity = pEntity->GetAbsVelocity();
	Vector primal_velocity = pEntity->GetAbsVelocity();
	edict_t *ed = pEntity->edict();

	int blocked = 0;
	int numplanes = 0;
	float allFraction = 0;
	float time_left = time;

	for( int bumpcount = 0; bumpcount < 4; bumpcount++ )
	{
		if( pEntity->GetAbsVelocity() == g_vecZero )
			break;

		Vector end = pEntity->GetAbsOrigin() + pEntity->GetAbsVelocity() * time_left;

		pEntity->m_iPushableFilter = TRUE;
		TRACE_MONSTER_HULL( pEntity->edict(), pEntity->GetAbsOrigin(), end, dont_ignore_monsters, pEntity->edict(), &trace );
		pEntity->m_iPushableFilter = FALSE;

		CBaseEntity *pHit = CBaseEntity::Instance( trace.pHit );
		allFraction += trace.flFraction;

		if( trace.fAllSolid )
		{	
			// entity is trapped in another solid
			pEntity->SetAbsVelocity( g_vecZero );
			return 4;
		}

		if( trace.flFraction > 0.0f )
		{	
			// actually covered some distance
			pEntity->SetAbsOrigin( trace.vecEndPos );
			original_velocity = pEntity->GetAbsVelocity();
			numplanes = 0;
		}

		if( trace.flFraction == 1.0f )
		{
			 break; // moved the entire distance
                    }

		if( !trace.pHit )
			ALERT( at_error, "SV_FlyMove: trace.pHit == NULL\n" );

		if( trace.vecPlaneNormal.z > 0.7f )
		{
			blocked |= 1; // floor

         			if( trace.pHit->v.solid == SOLID_BSP || trace.pHit->v.solid == SOLID_SLIDEBOX ||
			trace.pHit->v.movetype == MOVETYPE_PUSHSTEP || (trace.pHit->v.flags & FL_CLIENT))
			{
				pEntity->SetGroundEntity( trace.pHit );
			}
		}

		if( trace.vecPlaneNormal.z == 0.0f )
		{
			blocked |= 2; // step
			if( steptrace ) *steptrace = trace; // save for player extrafriction
		}

		// run the impact function
		SV_Impact( pEntity, NULL, &trace );

		// break if removed by the impact function
		if( ed->free || pEntity->pev->flags & FL_KILLME )
			break;

		time_left -= time_left * trace.flFraction;

		// clipped to another plane
		if( numplanes >= MAX_CLIP_PLANES )
		{
			// this shouldn't really happen
			pEntity->SetAbsVelocity( g_vecZero );
			break;
		}

		planes[numplanes] = trace.vecPlaneNormal;
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for( i = 0; i < numplanes; i++ )
		{
			float overbounce = SV_CalcOverBounce( pEntity, &trace );

			SV_ClipVelocity( original_velocity, planes[i], new_velocity, overbounce );

			for( j = 0; j < numplanes; j++ )
			{
				if( j != i )
				{
					if( DotProduct( new_velocity, planes[j] ) < 0.0f )
						break; // not ok
				}
			}
			if( j == numplanes )
				break;
		}

		if( i != numplanes )
		{
			// go along this plane
			pEntity->SetAbsVelocity( new_velocity );
		}
		else
		{
			// go along the crease
			if( numplanes != 2 )
			{
				pEntity->SetAbsVelocity( g_vecZero );
				break;
			}

			Vector dir = CrossProduct( planes[0], planes[1] );
			float d = DotProduct( dir, pEntity->GetAbsVelocity() );
			pEntity->SetAbsVelocity( dir * d );
		}

		// if current velocity is against the original velocity,
		// stop dead to avoid tiny occilations in sloping corners
		if( DotProduct( pEntity->GetAbsVelocity(), primal_velocity ) <= 0.0f )
		{
			pEntity->SetAbsVelocity( g_vecZero );
			break;
		}
	}

	if( allFraction == 0.0f )
		pEntity->SetAbsVelocity( g_vecZero );

	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity( CBaseEntity *pEntity )
{
	float	ent_gravity;

	if( pEntity->pev->gravity )
		ent_gravity = pEntity->pev->gravity;
	else ent_gravity = 1.0f; // factor

	float gravity = CVAR_GET_FLOAT( "sv_gravity" );
	Vector vecAbsVelocity = pEntity->GetAbsVelocity();

	// add gravity incorrectly
	vecAbsVelocity.z -= ( ent_gravity * gravity * gpGlobals->frametime );
	vecAbsVelocity.z += pEntity->pev->basevelocity[2] * gpGlobals->frametime;
	pEntity->pev->basevelocity[2] = 0.0f;
	pEntity->SetAbsVelocity( vecAbsVelocity );

	// bound velocity
	SV_CheckVelocity( pEntity );
}

/*
============
SV_AddHalfGravity

============
*/
void SV_AddHalfGravity( CBaseEntity *pEntity, float timestep )
{
	float	ent_gravity;

	if( pEntity->pev->gravity )
		ent_gravity = pEntity->pev->gravity;
	else ent_gravity = 1.0f;

	float gravity = CVAR_GET_FLOAT( "sv_gravity" );
	Vector vecAbsVelocity = pEntity->GetAbsVelocity();

	// Add 1/2 of the total gravitational effects over this timestep
	vecAbsVelocity.z -= ( 0.5f * ent_gravity * gravity * timestep );
	vecAbsVelocity.z += pEntity->pev->basevelocity.z * gpGlobals->frametime;
	pEntity->pev->basevelocity.z = 0.0f;
	pEntity->SetAbsVelocity( vecAbsVelocity );
	
	// bound velocity
	SV_CheckVelocity( pEntity );
}

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
TraceResult SV_PushEntity( CBaseEntity *pEntity, const Vector &lpush, const Vector &apush, BOOL *blocked )
{
	TraceResult trace;
	Vector start = pEntity->GetAbsOrigin();
	Vector end = start + lpush;

	UTIL_TraceEntity( pEntity, pEntity->GetAbsOrigin(), end, &trace );

	if( trace.flFraction != 0.0f )
	{
		pEntity->SetAbsOrigin( trace.vecEndPos );

		if( apush[YAW] && pEntity->IsNetClient( ))
		{
			pEntity->pev->avelocity[YAW] += apush[YAW];
			pEntity->pev->fixangle = 2;
		}

		// don't rotate pushables!
		if( SV_AllowPushRotate( pEntity ))
		{
			Vector vecAngles = pEntity->GetAbsAngles();
			vecAngles[YAW] += trace.flFraction * apush[YAW];
			pEntity->SetAbsAngles( vecAngles );

			// keep gait yaw is actual
			if( pEntity->IsPlayer( ) && apush[YAW] != 0.0f )
				pEntity->m_flGaitYaw = pEntity->pev->angles[YAW];
		}
	}

	// notsolid ents never touch triggers
	pEntity->RelinkEntity( (pEntity->pev->solid == SOLID_NOT) ? FALSE : TRUE, &start );

	if( blocked )
	{
		// can't move full distance
		*blocked = ( pEntity->GetAbsOrigin() != end ) ? TRUE : FALSE;
	}

	// so we can run impact function afterwards.
	if( trace.pHit ) SV_Impact( pEntity, NULL, &trace );

	return trace;
}

/*
=============
SV_PhysicsNone

Non moving objects can only think
=============
*/
void SV_Physics_None( CBaseEntity *pEntity )
{
	if( !SV_RunThink( pEntity ))
		return;

	if( pEntity->m_hParent )
		pEntity->RelinkEntity( TRUE );
}

/*
=============
SV_PhysicsRigid

rigid bodies check ground and conveyors
=============
*/
void SV_Physics_Rigid( CBaseEntity *pEntity )
{
	if( !SV_RunThink( pEntity ))
		return;

	UTIL_WaterMove( pEntity );
	SV_CheckWater( pEntity );
	WorldPhysic->UpdateEntityTransform( pEntity );

	// detect the ground
	CBaseEntity *pGround = pEntity->GetGroundEntity();

	if( pEntity->GetAbsVelocity().z > 0.0f || pGround == NULL || (pGround->pev->flags & ( FL_MONSTER|FL_CLIENT )))
	{
		pEntity->pev->flags &= ~FL_ONGROUND;
	}

	if( pEntity->m_iFlags & MF_GROUNDMOVE )
	{
		pEntity->ClearGroundEntity();
		pEntity->m_iFlags &= ~MF_GROUNDMOVE;
	}

	// if on ground or not moving, return.
	if( pEntity->pev->flags & FL_ONGROUND || WorldPhysic->IsBodySleeping( pEntity ))
	{
		if( pGround && FBitSet( pGround->pev->flags, FL_CONVEYOR ) && pGround->pev->speed && WorldPhysic->IsBodySleeping( pEntity ))
		{
			// wake up body if conveyor was enabled (add light kick)
			WorldPhysic->AddForce( pEntity, pGround->pev->movedir );
		}
		return;	// at rest
	}

	TraceResult trace;
	Vector vecPos = pEntity->GetAbsOrigin();
	Vector vecEnd = vecPos + pEntity->pev->mins.z - 1.0f;

	UTIL_TraceLine( vecPos, vecEnd, ignore_monsters, pEntity->edict() ,&trace );

	if( trace.vecPlaneNormal[2] > 0.7f )
		pEntity->SetGroundEntity( trace.pHit ? trace.pHit : ENT( 0 ));
}

/*
=============
SV_PhysicsNoclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip( CBaseEntity *pEntity )
{
	// regular thinking
	if( !SV_RunThink( pEntity )) return;

	SV_CheckWater( pEntity );	

	SV_LinearMove( pEntity, gpGlobals->frametime, pEntity->pev->friction );
	SV_AngularMove( pEntity, gpGlobals->frametime, pEntity->pev->friction );

	// noclip ents never touch triggers
	pEntity->RelinkEntity( FALSE );
}

/*
=============
SV_Physics_Follow

just copy angles and origin of parent
=============
*/
void SV_Physics_Follow( CBaseEntity *pEntity )
{
	CBaseEntity *pParent;

	// regular thinking
	if( !SV_RunThink( pEntity )) return;

	pParent = CBaseEntity::Instance( pEntity->pev->aiment );
	if( !pParent ) return;

	pEntity->pev->v_angle = pParent->EyePosition();
	pEntity->SetAbsAngles( pParent->GetAbsAngles());

	pEntity->RelinkEntity();
}

/*
=============
SV_Physics_Compound

a glue two entities together
=============
*/
void SV_Physics_Compound( CBaseEntity *pEntity )
{
	// regular thinking
	if( !SV_RunThink( pEntity )) return;

	CBaseEntity *pParent = CBaseEntity :: Instance( pEntity->pev->aiment );
	if( !pParent ) return;

	// not initialized ?
	if( pEntity->m_hParent != pParent )
	{
		if( pEntity->pev->solid != SOLID_TRIGGER )
			pEntity->pev->solid = SOLID_NOT;
		pEntity->SetParent( pParent );
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss( CBaseEntity *pEntity )
{
	TraceResult	trace;
	Vector		move;
	float		backoff;
	edict_t		*ed = pEntity->edict();

	SV_CheckWater( pEntity );
	
	// regular thinking
	if( !SV_RunThink( pEntity )) return;

	CBaseEntity *pGround = pEntity->GetGroundEntity();

	if( pEntity->GetAbsVelocity().z > 0.0f || pGround == NULL || (pGround->pev->flags & ( FL_MONSTER|FL_CLIENT )))
	{
		pEntity->pev->flags &= ~FL_ONGROUND;
	}

	if( pEntity->m_iFlags & MF_GROUNDMOVE )
	{
		pEntity->ClearGroundEntity();
		pEntity->m_iFlags &= ~MF_GROUNDMOVE;
	}

	// if on ground and not moving, return.
	if( pEntity->pev->flags & FL_ONGROUND && pEntity->GetAbsVelocity() == g_vecZero )
	{
		pEntity->SetLocalAvelocity( g_vecZero );

		if( pEntity->pev->basevelocity == g_vecZero )
			return;	// at rest
	}

	SV_CheckVelocity( pEntity );

	// add gravity
	switch( pEntity->pev->movetype )
	{
	case MOVETYPE_FLY:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_BOUNCEMISSILE:
		break;
	default:
		SV_AddGravity( pEntity );
		break;
	}

	// move angles (with friction)
	switch( pEntity->pev->movetype )
	{
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		SV_AngularMove( pEntity, gpGlobals->frametime, pEntity->pev->friction );
		break;         
	default:
		SV_AngularMove( pEntity, gpGlobals->frametime, 0.0f );
		break;
	}

	// move origin
	// Base velocity is not properly accounted for since this entity will move again
	// after the bounce without taking it into account
	Vector vecAbsVelocity = pEntity->GetAbsVelocity();
	vecAbsVelocity += pEntity->GetBaseVelocity();
	pEntity->SetAbsVelocity( vecAbsVelocity );

	SV_CheckVelocity( pEntity );

	move = pEntity->GetAbsVelocity() * gpGlobals->frametime;

	vecAbsVelocity = pEntity->GetAbsVelocity();
	vecAbsVelocity -= pEntity->GetBaseVelocity();
	pEntity->SetAbsVelocity( vecAbsVelocity );

	trace = SV_PushEntity( pEntity, move, g_vecZero, NULL );
	if( ed->free || pEntity->pev->flags & FL_KILLME ) return;

	SV_CheckVelocity( pEntity );

	if( trace.fAllSolid )
	{
		if( pEntity->m_iActorType == ACTOR_CHARACTER )
			WorldPhysic->MoveCharacter( pEntity );

		// entity is trapped in another solid
		pEntity->SetLocalAvelocity( g_vecZero );
		pEntity->SetLocalVelocity( g_vecZero );
		return;
	}

	if( trace.flFraction == 1.0f )
	{
		if( pEntity->m_iActorType == ACTOR_CHARACTER )
			WorldPhysic->MoveCharacter( pEntity );

		SV_CheckWaterTransition( pEntity );
		return;
	}

	if( pEntity->pev->movetype == MOVETYPE_BOUNCE )
		backoff = 2.0f - pEntity->pev->friction;
	else if( pEntity->pev->movetype == MOVETYPE_BOUNCEMISSILE )
		backoff = 2.0f;
	else backoff = 1.0f;

	vecAbsVelocity = pEntity->GetAbsVelocity();
	SV_ClipVelocity( vecAbsVelocity, trace.vecPlaneNormal, vecAbsVelocity, backoff );
	pEntity->SetAbsVelocity( vecAbsVelocity );

	// stop if on ground
	if( trace.vecPlaneNormal[2] > 0.7f )
	{		
		move = pEntity->GetAbsVelocity() + pEntity->GetBaseVelocity();

		float gravity = CVAR_GET_FLOAT( "sv_gravity" );
		float vel = DotProduct( move, move );

		if( pEntity->GetAbsVelocity().z < gravity * gpGlobals->frametime )
		{
			// we're rolling on the ground, add static friction.
			pEntity->SetGroundEntity( trace.pHit );
			vecAbsVelocity = pEntity->GetAbsVelocity();			
			vecAbsVelocity.z = 0.0f;
			pEntity->SetAbsVelocity( vecAbsVelocity );
		}

		if( vel < 900.0f || ( pEntity->pev->movetype != MOVETYPE_BOUNCE && pEntity->pev->movetype != MOVETYPE_BOUNCEMISSILE ))
		{
			pEntity->SetGroundEntity( trace.pHit );
			pEntity->SetLocalAvelocity( g_vecZero );
			pEntity->SetLocalVelocity( g_vecZero );
		}
		else
		{
			move = pEntity->GetAbsVelocity() * ((1.0f - trace.flFraction) * gpGlobals->frametime * 0.9f);
			move += pEntity->GetBaseVelocity() * ((1.0f - trace.flFraction) * gpGlobals->frametime * 0.9f);
			trace = SV_PushEntity( pEntity, move, g_vecZero, NULL );
			if( ed->free ) return;
		}
	}

	if( pEntity->m_iActorType == ACTOR_CHARACTER )
		WorldPhysic->MoveCharacter( pEntity );
	
	// check for in water
	SV_CheckWaterTransition( pEntity );
}

CUtlArray<CBaseEntity*>	m_rgGroundEnts;
static CBaseEntity		*pMovingGround;	// a pointer to func_pushable
static int		s_nEnumCount;
	
static void AddEntityToGroundList( CBaseEntity *pEntity )
{
	if( !pEntity ) return;

	if( pEntity->m_iGroundEnumCount == s_nEnumCount )
		return;	// already added

	if( pEntity->pev->solid == SOLID_NOT )
		return;

	// filter movetypes to collide with
	switch( pEntity->pev->movetype )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_FOLLOW:
	case MOVETYPE_NOCLIP:
	case MOVETYPE_COMPOUND:
		return;
	}

	if( pEntity->GetGroundEntity() != pMovingGround )
		return;

	pEntity->m_iGroundEnumCount = s_nEnumCount;

	// store entity in list
	int i = m_rgGroundEnts.AddToTail();
	m_rgGroundEnts[i] = pEntity;
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/
/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
void SV_Physics_Step( CBaseEntity *pEntity )
{
	BOOL	inwater;
	BOOL	wasonmover;
	BOOL	wasonground;
	edict_t	*ent = pEntity->edict();
	TraceResult trace;

	Vector oldAbsOrigin = pEntity->GetAbsOrigin();

	UTIL_WaterMove( pEntity );
	SV_CheckVelocity( pEntity );

	wasonground = (pEntity->pev->flags & FL_ONGROUND);
	wasonmover = SV_CheckMover( pEntity );
	inwater = SV_CheckWater( pEntity );

	float game_friction = CVAR_GET_FLOAT( "sv_friction" );
	float game_stopspeed = CVAR_GET_FLOAT( "sv_stopspeed" );

	if( pEntity->pev->flags & FL_FLOAT && pEntity->pev->waterlevel > 0 && pEntity->m_hParent == NULL )
	{
		float buoyancy = SV_Submerged( pEntity ) * pEntity->pev->skin * gpGlobals->frametime;

		SV_AddGravity( pEntity );

		// add buoyancy
		Vector vecAbsVelocity = pEntity->GetAbsVelocity();
		vecAbsVelocity.z += buoyancy;
		pEntity->SetAbsVelocity( vecAbsVelocity );
	}

	if( !wasonground && !( pEntity->pev->flags & FL_FLY ) && (!( pEntity->pev->flags & FL_SWIM ) || pEntity->pev->waterlevel <= 0 ))
	{
		// compound link, so we ignore gravity here
		if( !inwater && pEntity->m_hParent == NULL )
			SV_AddGravity( pEntity );
	}

	if( pEntity->m_hParent != NULL )
		pEntity->SetAbsVelocity( g_vecZero );

	CBaseEntity *pGround = pEntity->GetGroundEntity();

	if( !pGround || (pGround->pev->flags & ( FL_MONSTER|FL_CLIENT )))
		pEntity->pev->flags &= ~FL_ONGROUND;

	if( pEntity->GetAbsVelocity() != g_vecZero || pEntity->GetBaseVelocity() != g_vecZero )
	{
		pEntity->pev->flags &= ~FL_ONGROUND;

		if(( wasonground || wasonmover ) && ( pEntity->pev->health > 0 || ENT_IS_ON_FLOOR( pEntity->edict() )))
		{
			Vector	vel = pEntity->GetAbsVelocity();
			float	control, speed, newspeed;
			float	friction;

			speed = sqrt(( vel[0] * vel[0] ) + ( vel[1] * vel[1] ));	// DotProduct2D

			if( speed )
			{
				friction = game_friction * pEntity->pev->friction;	// factor
				pEntity->pev->friction = 1.0f; // g-cont. ???
				if( wasonmover ) friction *= 0.5f;

				control = (speed < game_stopspeed) ? game_stopspeed : speed;
				newspeed = speed - (gpGlobals->frametime * control * friction);

				if( newspeed < 0 )
					newspeed = 0;
				newspeed /= speed;

				vel[0] = vel[0] * newspeed;
				vel[1] = vel[1] * newspeed;

				pEntity->SetAbsVelocity( vel );
			}
		}

		pEntity->ApplyAbsVelocityImpulse( pEntity->GetBaseVelocity( ));
		SV_CheckVelocity( pEntity );

		if( 1 )
		{
			// Try sliding forward both on ground and up 16 pixels
			// take the move that goes farthest
			Vector lastorg = pEntity->GetAbsOrigin();
			Vector lastvel = pEntity->GetAbsVelocity();
			Vector dest;

			SV_FlyMove( pEntity, gpGlobals->frametime, &trace );

			// Copy the results out
			Vector downorg = pEntity->GetAbsOrigin();
			Vector downvel = pEntity->GetAbsVelocity();

			// Reset original values.
			pEntity->SetAbsOrigin( lastorg );
			pEntity->SetAbsVelocity( lastvel );

			// Start out up one stair height
			dest = pEntity->GetAbsOrigin();
			dest.z += g_psv_stepsize->value;

			pEntity->m_iPushableFilter = TRUE;
			TRACE_MONSTER_HULL( pEntity->edict(), pEntity->GetAbsOrigin(), dest, dont_ignore_monsters, pEntity->edict(), &trace );
			pEntity->m_iPushableFilter = FALSE;
	
			// If we started okay and made it part of the way at least,
			//  copy the results to the movement start position and then
			//  run another move try.
			if( !trace.fStartSolid && !trace.fAllSolid )
			{
				pEntity->SetAbsOrigin( trace.vecEndPos );
			}

			SV_FlyMove( pEntity, gpGlobals->frametime, &trace );	

			// Now try going back down from the end point
			//  press down the stepheight
			dest = pEntity->GetAbsOrigin();
			dest.z -= g_psv_stepsize->value;

			pEntity->m_iPushableFilter = TRUE;
			TRACE_MONSTER_HULL( pEntity->edict(), pEntity->GetAbsOrigin(), dest, dont_ignore_monsters, pEntity->edict(), &trace );
			pEntity->m_iPushableFilter = FALSE;

			float updist, downdist;
			Vector uporg;
	
			// If we are not on the ground any more then
			//  use the original movement attempt
			if( trace.vecPlaneNormal.z < 0.7f )
				goto usedown;
			// If the trace ended up in empty space, copy the end
			//  over to the origin.
			if( !trace.fStartSolid && !trace.fAllSolid )
			{
				pEntity->SetAbsOrigin( trace.vecEndPos );
			}

			// Copy this origin to up.
			uporg = pEntity->GetAbsOrigin();

			// decide which one went farther
			downdist = (downorg[0] - lastorg[0]) * (downorg[0] - lastorg[0]) + (downorg[1] - lastorg[1]) * (downorg[1] - lastorg[1]);
			updist = (uporg[0] - lastorg[0]) * (uporg[0] - lastorg[0]) + (uporg[1] - lastorg[1]) * (uporg[1] - lastorg[1]);

			if( downdist > updist )
			{
usedown:
				pEntity->SetAbsOrigin( downorg );
				pEntity->SetAbsVelocity( downvel );
			}
			else
			{
				// copy z value from slide move
				Vector vel = pEntity->GetAbsVelocity();
				vel.z = downvel.z;
				pEntity->SetAbsVelocity( vel );
			}
		}
		else
		{
			int move = SV_FlyMove( pEntity, gpGlobals->frametime, &trace );
		}

		if( ent->free || pEntity->pev->flags & FL_KILLME )
			return;

		SV_CheckVelocity( pEntity );
		pEntity->ApplyAbsVelocityImpulse( -pEntity->GetBaseVelocity( ));

		SV_CheckVelocity( pEntity );

		Vector mins = pEntity->GetAbsOrigin() + pEntity->pev->mins;
		Vector maxs = pEntity->GetAbsOrigin() + pEntity->pev->maxs;
		Vector point;

		point.z = mins.z - 1.0f;

		for( int x = 0; x <= 1; x++ )
		{
			if( pEntity->pev->flags & FL_ONGROUND )
				break;

			for( int y = 0; y <= 1; y++ )
			{
				point.x = x ? maxs.x : mins.x;
				point.y = y ? maxs.y : mins.y;

				UTIL_TraceLine( point, point, dont_ignore_monsters, ent, &trace );

				if( trace.fStartSolid )
				{
					pEntity->SetGroundEntity( trace.pHit );
					pEntity->pev->friction = 1.0f;
					break;
				}
			}
		}

		pEntity->RelinkEntity( TRUE, &oldAbsOrigin );

		// move func_pushable riders
		if( pEntity->pev->movetype == MOVETYPE_PUSHSTEP && pEntity->pev->solid == SOLID_BSP )
		{
			Vector vecAbsMins, vecAbsMaxs;
			pEntity->WorldSpaceAABB( vecAbsMins, vecAbsMaxs );

			m_rgGroundEnts.RemoveAll();
			pMovingGround = pEntity;
			s_nEnumCount++;

			vecAbsMaxs.z *= 2;

			UTIL_AreaNode( vecAbsMins, vecAbsMaxs, AREA_SOLID, AddEntityToGroundList );
			UTIL_AreaNode( vecAbsMins, vecAbsMaxs, AREA_TRIGGERS, AddEntityToGroundList );

			Vector lmove = pEntity->GetAbsOrigin() - oldAbsOrigin;
			int blocked;

			// FIXME: need a recursive here for move all standing items
			// e.g. player standing on the pushable which standing on the pushable
			// whcih standing on the pushable.. oh, well
			for( int i = 0; i < m_rgGroundEnts.Count(); i++ )
			{
				SV_PushEntity( m_rgGroundEnts[i], lmove, g_vecZero, &blocked );
				if( m_rgGroundEnts[i]->m_iActorType == ACTOR_CHARACTER )
					WorldPhysic->MoveCharacter( m_rgGroundEnts[i] );
				else if( m_rgGroundEnts[i]->m_iActorType == ACTOR_KINEMATIC )
					WorldPhysic->MoveKinematic( m_rgGroundEnts[i] );
				if( blocked ) m_rgGroundEnts[i]->pev->flags &= ~FL_ONGROUND;
			}
		}
	}
	else
	{
		if( gpGlobals->force_retouch != 0.0f )
		{
			TRACE_MONSTER_HULL( ent, pEntity->GetAbsOrigin(), pEntity->GetAbsOrigin(), dont_ignore_monsters, ent, &trace );

			// hentacle impact code
			if(( trace.flFraction < 1.0f || trace.fStartSolid )  && trace.pHit )
				SV_Impact( pEntity, NULL, &trace );
		}
	}

	if( ent->free || pEntity->pev->flags & FL_KILLME )
		return;
	
	if( !SV_RunThink( pEntity )) return;
	SV_CheckWaterTransition( pEntity );

	if( pEntity->m_iActorType == ACTOR_KINEMATIC )
		WorldPhysic->MoveKinematic( pEntity );
	else if( pEntity->m_iActorType == ACTOR_CHARACTER )
		WorldPhysic->MoveCharacter( pEntity );
}

/*
============
SV_PushMove

============
*/
static CBaseEntity *SV_PushMove( CBaseEntity *pPusher, float movetime )
{
	// if this entity isn't moving, just update the time.
	pPusher->IncrementLocalTime( movetime );

	// not moving (while changelevel tracktrain may potentially moving on a frame forward)
	if( pPusher->GetLocalVelocity() == g_vecZero || gpGlobals->changelevel )
		return NULL;

	// now check that the entire hierarchy can move into the new location
	CBaseEntity *pBlocker = g_pPushedEntities->PerformLinearPush( pPusher, movetime );
	if( pBlocker ) pPusher->IncrementLocalTime( -movetime );

	return pBlocker;
}

/*
============
SV_PushRotate

============
*/
static CBaseEntity *SV_PushRotate( CBaseEntity *pPusher, float movetime )
{
	// if this entity isn't moving, just update the time.
	pPusher->IncrementLocalTime( movetime );

	// not rotating (while changelevel tracktrain may potentially moving on a frame forward)
	if( pPusher->GetLocalAvelocity() == g_vecZero || gpGlobals->changelevel )
		return NULL;

	// now check that the entire hierarchy can rotate into the new location
	CBaseEntity *pBlocker = g_pPushedEntities->PerformRotatePush( pPusher, movetime );
	if( pBlocker ) pPusher->IncrementLocalTime( -movetime );

	return pBlocker;
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher( CBaseEntity *pEntity )
{
	if( GET_SERVER_STATE() == SERVER_ACTIVE )
	{
		// regular thinking
		if( !SV_RunThink( pEntity ))
			return;
	}

	// enable interpolation on a bmodels (Xash3D extension)
//	pEntity->pev->animtime = gpGlobals->time;

	float movetime = pEntity->GetMoveDoneTime();
	if( movetime > gpGlobals->frametime )
		movetime = gpGlobals->frametime;

	CBaseEntity *pBlocker = NULL;
	g_pPushedEntities->BeginPush( pEntity );

	if( movetime > 0 )
	{
		if( pEntity->GetLocalAvelocity() != g_vecZero )
		{
			if( pEntity->GetLocalVelocity() != g_vecZero )
			{
				float flOldRotateTime = pEntity->GetLocalTime();

				pBlocker = SV_PushRotate( pEntity, movetime );

				if( !pBlocker )
				{
					float flOldMoveTime = pEntity->GetLocalTime();

					// reset the local time to what it was before we rotated
					pEntity->pev->ltime = flOldRotateTime;
					pBlocker = SV_PushMove( pEntity, movetime );
					if( pEntity->pev->ltime < flOldMoveTime )
						pEntity->pev->ltime = flOldMoveTime;
				}
			}
			else
			{
				pBlocker = SV_PushRotate( pEntity, movetime );
			}
		}
		else 
		{
			pBlocker = SV_PushMove( pEntity, movetime );
		}

		// update physics shadow for root pushers
		WorldPhysic->MoveKinematic( pEntity );
	}

	// if the pusher has a "blocked" function, call it
	// otherwise, just stay in place until the obstacle is gone
	if( pBlocker ) DispatchBlocked( pEntity->edict(), pBlocker->edict() );

	// NOTE: all the entities with MOVETYPE_PUSH now has two seperated thinks:
	// normal regular thinking and local think (movedone)
	if( pEntity->m_flMoveDoneTime <= pEntity->GetLocalTime() && pEntity->m_flMoveDoneTime > 0 )
	{
		pEntity->SetMoveDoneTime( -1 );
		pEntity->MoveDone();
	}
}

/*
=============
SV_PhysicsVehicle

Vehicles can push other entities
=============
*/
void SV_Physics_Vehicle( CBaseEntity *pEntity )
{
	// regular thinking
	if( !SV_RunThink( pEntity ))
		return;

	WorldPhysic->UpdateVehicle( pEntity );

	SV_CheckWater( pEntity );

	// sync physic states
	WorldPhysic->UpdateEntityTransform( pEntity );

	float movetime = gpGlobals->frametime;

	if( movetime > 0 )
	{
		CBaseEntity *pBlocker = NULL;
		g_pPushedEntities->BeginPush( pEntity );

		if( pEntity->GetLocalAvelocity() != g_vecZero )
		{
			if( pEntity->GetLocalVelocity() != g_vecZero )
			{
				pBlocker = SV_PushRotate( pEntity, movetime );
				if( !pBlocker ) pBlocker = SV_PushMove( pEntity, movetime );
			}
			else
			{
				pBlocker = SV_PushRotate( pEntity, movetime );
			}
		}
		else if( pEntity->GetLocalVelocity() != g_vecZero ) 
		{
			pBlocker = SV_PushMove( pEntity, movetime );
		}

		// if the pusher has a "blocked" function, call it
		// otherwise, just stay in place until the obstacle is gone
		if( pBlocker ) DispatchBlocked( pEntity->edict(), pBlocker->edict() );
	}

	// detect the ground
	CBaseEntity *pGround = pEntity->GetGroundEntity();

	if( pEntity->GetAbsVelocity().z > 0.0f || pGround == NULL || (pGround->pev->flags & ( FL_MONSTER|FL_CLIENT )))
	{
		pEntity->pev->flags &= ~FL_ONGROUND;
	}

	if( pEntity->m_iFlags & MF_GROUNDMOVE )
	{
		pEntity->ClearGroundEntity();
		pEntity->m_iFlags &= ~MF_GROUNDMOVE;
	}

	// if on ground or not moving, return.
	if( pEntity->pev->flags & FL_ONGROUND || WorldPhysic->IsBodySleeping( pEntity ))
	{
		if( pGround && FBitSet( pGround->pev->flags, FL_CONVEYOR ) && pGround->pev->speed && WorldPhysic->IsBodySleeping( pEntity ))
		{
			// wake up body if conveyor was enabled (add light kick)
			WorldPhysic->AddForce( pEntity, pGround->pev->movedir );
		}
		return;	// at rest
	}

	TraceResult trace;
	Vector vecPos = pEntity->GetAbsOrigin();
	Vector vecEnd = vecPos + pEntity->pev->mins.z - 1.0f;

	UTIL_TraceLine( vecPos, vecEnd, ignore_monsters, pEntity->edict() ,&trace );

	if( trace.vecPlaneNormal[2] > 0.7f )
		pEntity->SetGroundEntity( trace.pHit ? trace.pHit : ENT( 0 ));
}

//
// assume pEntity is valid
//
int RunPhysicsFrame( CBaseEntity *pEntity )
{
	// to avoid run physics frame twice for parent entities
	if( pEntity->m_iPhysicsFrame == g_ulFrameCount )
		return 1;	// handled

	pEntity->m_iPhysicsFrame = g_ulFrameCount;

	if( gpGlobals->force_retouch != 0.0f )
	{
		// force retouch even for stationary
		pEntity->RelinkEntity( true );
	}

	// If we've have a parent, we must simulate that first.
	CBaseEntity *pParent = pEntity->m_hParent;

	if( pEntity->pev->movetype == MOVETYPE_NONE && !pParent )
	{
		SV_Physics_None( pEntity );
		return 1;
	}

	// compound movetype waiting for parent
	if( pEntity->pev->movetype == MOVETYPE_COMPOUND && !pEntity->pev->aiment )
	{
		if( pEntity->m_hParent )
			pEntity->SetParent(( CBaseEntity *)NULL );
		SV_Physics_None( pEntity );
		return 1;
	}

	// player is already updated for this frame
	if( pParent && !pParent->IsPlayer( ))
	{
		// run the parent first
		RunPhysicsFrame( pParent );
	}
	else
	{
		SV_UpdateBaseVelocity( pEntity );

		if(!( pEntity->pev->flags & FL_BASEVELOCITY ) && pEntity->GetBaseVelocity() != g_vecZero )
		{
			// Apply momentum (add in half of the previous frame of velocity first)
			Vector vecAbsVelocity = pEntity->GetAbsVelocity() + pEntity->GetBaseVelocity() * (1.0f + (gpGlobals->frametime * 0.5f));
			pEntity->SetAbsVelocity( vecAbsVelocity );
			pEntity->SetBaseVelocity( g_vecZero );
		}
		pEntity->pev->flags &= ~FL_BASEVELOCITY;
	}

	// NOTE: at this point pEntity assume to be valid
	switch( pEntity->pev->movetype )
	{
	case MOVETYPE_NONE:
		SV_Physics_None( pEntity );
		return 1;
	case MOVETYPE_NOCLIP:
		SV_Physics_Noclip( pEntity );
		return 1;
	case MOVETYPE_FOLLOW:
		SV_Physics_Follow( pEntity );
		return 1;
	case MOVETYPE_COMPOUND:
		SV_Physics_Compound( pEntity );
		return 1;
	case MOVETYPE_FLY:
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_BOUNCEMISSILE:
		SV_Physics_Toss( pEntity );
		return 1;
	case MOVETYPE_STEP:
	case MOVETYPE_PUSHSTEP:
		SV_Physics_Step( pEntity );
		return 1;
	case MOVETYPE_PUSH:
		SV_Physics_Pusher( pEntity );
		return 1;
	case MOVETYPE_PHYSIC:
		SV_Physics_Rigid( pEntity );
		return 1;
	case MOVETYPE_VEHICLE:
		SV_Physics_Vehicle( pEntity );
		return 1;
	}

	// other movetypes uses built-in engine physic
	return 0;
}
