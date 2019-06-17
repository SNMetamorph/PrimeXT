/*
movelist.h - list of all linked or touched entities for group moving
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

#ifndef MOVELIST_H
#define MOVELIST_H

#include <assert.h>
#include <utlarray.h>

//-----------------------------------------------------------------------------
// Purpose: Keeps track of original positions of any entities that are being possibly pushed
//  and handles restoring positions for those objects if the push is aborted
//-----------------------------------------------------------------------------
class CPhysicsPushedEntities
{
public:
	CPhysicsPushedEntities( void ) : m_rgPusher( 8, 8 ), m_rgMoved( 32, 32 ) {}

	// Purpose: Tries to rotate an entity hierarchy, returns the blocker if any
	CBaseEntity	*PerformRotatePush( CBaseEntity *pRoot, float movetime );

	// Purpose: Tries to linearly push an entity hierarchy, returns the blocker if any
	CBaseEntity	*PerformLinearPush( CBaseEntity *pRoot, float movetime );

	int		CountMovedEntities() { return m_rgMoved.Count(); }
	void		BeginPush( CBaseEntity *pRootEntity );

	void		AddPushedEntityToBlockingList( CBaseEntity *pPushed );
protected:

	// describes the per-frame incremental motion of a rotating MOVETYPE_PUSH
	struct RotatingPushMove_t
	{
		Vector		origin;
		Vector		amove;		// delta orientation
		matrix4x4		startLocalToWorld;
		matrix4x4		endLocalToWorld;
	};

	// Pushers + their original positions also (for touching triggers)
	struct PhysicsPusherInfo_t
	{
		CBaseEntity	*m_pEntity;
		Vector		m_vecStartAbsOrigin;
	};

	// Pushed entities + various state related to them being pushed
	struct PhysicsPushedInfo_t
	{
		CBaseEntity	*m_pEntity;
		Vector		m_vecStartAbsOrigin;
		TraceResult	m_trace;
		bool		m_bBlocked;
		bool		m_bPusherIsGround;
	};

	// Adds the specified entity to the list
	void	AddEntity( CBaseEntity *ent );

	// Check pushed entity if we standing on
	bool	IsStandingOnPusher( CBaseEntity *pCheck );

	// If a move fails, restores all entities to their original positions
	void	RestoreEntities( );

	// Compute the direction to move the rotation blocker
	void	CalcRotationalPushDirection( CBaseEntity *pBlocker, const RotatingPushMove_t &rotMove, Vector &pMove, CBaseEntity *pRoot );

	// Speculatively checks to see if all entities in this list can be pushed
	bool SpeculativelyCheckPush( PhysicsPushedInfo_t &info, const Vector &vecAbsPush, bool bRotationalPush );

	// Speculatively checks to see if all entities in this list can be pushed
	virtual bool SpeculativelyCheckRotPush( const RotatingPushMove_t &rotPushMove, CBaseEntity *pRoot );

	// Speculatively checks to see if all entities in this list can be pushed
	virtual bool SpeculativelyCheckLinearPush( const Vector &vecAbsPush );

	// Registers a blockage
	CBaseEntity *RegisterBlockage();

	// Some fixup for objects pushed by rotating objects
	virtual void FinishRotPushedEntity( CBaseEntity *pPushedEntity, const RotatingPushMove_t &rotPushMove );

	// Commits the speculative movement
	void	FinishPush( bool bIsRotPush = false, const RotatingPushMove_t *pRotPushMove = NULL );

	// Generates a list of all entities potentially blocking all pushers
	void	GenerateBlockingEntityList();
	void	GenerateBlockingEntityListAddBox( const Vector &vecMoved );

	// Purpose: Gets a list of all entities hierarchically attached to the root 
	void	SetupAllInHierarchy( CBaseEntity *pParent );

	// Unlink + relink the pusher list so we can actually do the push
	void	UnlinkPusherList( void );
	void	RelinkPusherList( void );

	// Causes all entities in the list to touch triggers from their prev position
	void	FinishPushers();

	// Purpose: Rotates the root entity, fills in the pushmove structure
	void	RotateRootEntity( CBaseEntity *pRoot, float movetime, RotatingPushMove_t &rotation );

	// Purpose: Linearly moves the root entity
	void	LinearlyMoveRootEntity( CBaseEntity *pRoot, float movetime, Vector &pAbsPushVector );

	bool	IsPushedPositionValid( CBaseEntity *pBlocker );

protected:
	CUtlArray<PhysicsPusherInfo_t>	m_rgPusher;
	CUtlArray<PhysicsPushedInfo_t>	m_rgMoved;
	int				m_nBlocker;
	bool				m_bIsUnblockableByPlayer;
	int				s_nEnumCount; // to avoid push entities twice
};

#endif//MOVELIST_H
