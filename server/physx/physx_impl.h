/*
physx_impl.h - part of PhysX physics engine implementation
Copyright (C) 2012 Uncle Mike
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#pragma once
#include "physic.h"
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "io_streams.h"
#include "error_stream.h"
#include "assert_handler.h"
#include "clipfile.h"
#include "filesystem_utils.h"

#include <PxPhysicsAPI.h>
#include <PxSimulationEventCallback.h>
#include <PxScene.h>
#include <PxActor.h>
#include <PxTriangleMeshDesc.h>
#include <PxConvexMeshDesc.h>
#include <PxMaterial.h>
#include <PxCooking.h>
#include <PxTriangle.h>
	
class DebugRenderer;
class EventHandler;
class ContactModifyCallback;

class CPhysicPhysX : public IPhysicLayer
{
public:
	CPhysicPhysX();
	void	InitPhysic( void );
	void	FreePhysic( void );
	void	Update( float flTimeDelta );
	void	EndFrame( void );
	void	RemoveBody( edict_t *pEdict );
	void	*CreateBodyFromEntity( CBaseEntity *pEntity );
	void	*CreateBoxFromEntity( CBaseEntity *pObject );
	void	*CreateKinematicBodyFromEntity( CBaseEntity *pEntity );
	void	*CreateStaticBodyFromEntity( CBaseEntity *pObject );
	void	*CreateVehicle( CBaseEntity *pObject, string_t scriptName = 0 );
	void	*CreateTriggerFromEntity( CBaseEntity *pEntity );
	void	*RestoreBody( CBaseEntity *pEntity );
	void	SaveBody( CBaseEntity *pObject );
	bool	Initialized( void ) { return (m_pPhysics != NULL); }
	void	SetOrigin( CBaseEntity *pEntity, const Vector &origin );
	void	SetAngles( CBaseEntity *pEntity, const Vector &angles );
	void	SetVelocity( CBaseEntity *pEntity, const Vector &velocity );
	void	SetAvelocity( CBaseEntity *pEntity, const Vector &velocity );
	void	MoveObject( CBaseEntity *pEntity, const Vector &finalPos );
	void	RotateObject( CBaseEntity *pEntity, const Vector &finalAngle );
	void	SetLinearMomentum( CBaseEntity *pEntity, const Vector &velocity );
	void	AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor );
	void	AddForce( CBaseEntity *pEntity, const Vector &force );
	void	EnableCollision( CBaseEntity *pEntity, int fEnable );
	void	MakeKinematic( CBaseEntity *pEntity, int fEnable );
	void	UpdateVehicle( CBaseEntity *pObject );
	int		FLoadTree( char *szMapName );
	int		CheckBINFile( char *szMapName );
	int		BuildCollisionTree( char *szMapName );
	bool	UpdateEntityTransform( CBaseEntity *pEntity );
	void	UpdateEntityAABB( CBaseEntity *pEntity );
	bool	UpdateActorPos( CBaseEntity *pEntity );
	void	SetupWorld( void );	
	void	FreeWorld( void );
	void	DebugDraw( void );
	void	DrawPSpeeds( void );

	void	TeleportCharacter( CBaseEntity *pEntity );
	void	TeleportActor( CBaseEntity *pEntity );
	void	MoveCharacter( CBaseEntity *pEntity );
	void	MoveKinematic( CBaseEntity *pEntity );
	void	SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, struct trace_s *tr );
	void	SweepEntity( CBaseEntity *pEntity, const Vector &start, const Vector &end, struct gametrace_s *tr );
	bool	IsBodySleeping( CBaseEntity *pEntity );
	void	*GetCookingInterface( void ) { return m_pCooking; }
	void	*GetPhysicInterface( void ) { return m_pPhysics; }

private:
	// misc routines
	bool DebugEnabled() const;
	bool TracingStateChanges(physx::PxActor *actor) const;
	void HandleEvents();
	int	ConvertEdgeToIndex( model_t *model, int edge );
	physx::PxConvexMesh	*ConvexMeshFromBmodel( entvars_t *pev, int modelindex );
	physx::PxConvexMesh	*ConvexMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin );
	physx::PxConvexMesh	*ConvexMeshFromEntity( CBaseEntity *pObject );
	physx::PxTriangleMesh *TriangleMeshFromBmodel( entvars_t *pev, int modelindex );
	physx::PxTriangleMesh *TriangleMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin );
	physx::PxTriangleMesh *TriangleMeshFromEntity( CBaseEntity *pObject );
	physx::PxActor *ActorFromEntity( CBaseEntity *pObject );
	physx::PxBounds3 GetIntersectionBounds( const physx::PxBounds3 &a, const physx::PxBounds3 &b );
	CBaseEntity	*EntityFromActor( physx::PxActor *pObject );
	bool CheckCollision(physx::PxRigidActor *pActor);
	void ToggleCollision(physx::PxRigidActor *pActor, bool enabled);
	void UpdateCharacterBounds( CBaseEntity *pEntity, physx::PxShape *pShape );
	int	CheckFileTimes( const char *szFile1, const char *szFile2 );
	void StudioCalcBoneQuaterion( mstudiobone_t *pbone, mstudioanim_t *panim, Vector4D &q );
	void StudioCalcBonePosition( mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos );
	bool P_SpeedsMessage( char *out, size_t size );
	void CacheNameForModel( model_t *model, fs::Path &hullfile, uint32_t hash, const char *suffix );
	uint32_t GetHashForModelState( model_t *model, int32_t body, int32_t skin );
	clipfile::GeometryType ShapeTypeToGeomType(physx::PxGeometryType::Enum geomType);

private:
	physx::PxPhysics *m_pPhysics;	// pointer to the PhysX engine
	physx::PxFoundation	*m_pFoundation;
	physx::PxDefaultCpuDispatcher *m_pDispatcher;
	physx::PxScene *m_pScene;	// pointer to world scene
	model_t	*m_pWorldModel;	// pointer to worldmodel

	char m_szMapName[256];
	bool m_fLoaded;	// collision tree is loaded and actual
	bool m_fDisableWarning;	// some warnings will be swallowed
	bool m_fWorldChanged;	// world is changed refresh the statics in case their scale was changed too
	bool m_traceStateChanges;
	double m_flAccumulator;

	physx::PxTriangleMesh *m_pSceneMesh;
	physx::PxActor *m_pSceneActor;	// scene with installed shape
	physx::PxBounds3 m_worldBounds;
	physx::PxMaterial *m_pDefaultMaterial;
	physx::PxMaterial *m_pConveyorMaterial;

	char p_speeds_msg[1024];	// debug message

	AssertHandler m_assertHandler;
	ErrorCallback m_errorCallback;
	std::unique_ptr<DebugRenderer> m_debugRenderer;
	std::unique_ptr<EventHandler> m_eventHandler;
	std::unique_ptr<ContactModifyCallback> m_contactModifyCallback;
	physx::PxCooking *m_pCooking;
	physx::PxDefaultAllocator m_Allocator;
	physx::PxPvd *m_pVisualDebugger;
};
