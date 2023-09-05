/*
novodex.h - this file is a part of Novodex physics engine implementation
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

#include "NxPhysicsSDK.h"
#include "NxUserContactReport.h"
#include "NxUserOutputStream.h"
#include "NxScene.h"
#include "NxActor.h"
#include "NxUserStream.h"
#include "NxErrorStream.h"
#include "NxTriangleMeshDesc.h"
#include "NxTriangleMeshShapeDesc.h"
#include "NxForceFieldLinearKernel.h"
#include "NxBoxForceFieldShapeDesc.h"
#include "NxForceFieldDesc.h"
#include "NxDebugRenderable.h"
#include "NxConvexShapeDesc.h"
#include "NxConvexMeshDesc.h"
#include "NxBoxShapeDesc.h"
#include "NxBoxShape.h"
#include "NxMaterial.h"
#include "NxActorDesc.h"
#include "NxCooking.h"
#include "NxTriangle.h"
#include "PhysXLoader.h"
#include "clipfile.h"
#include "filesystem_utils.h"

#define DENSITY_FACTOR		0.0013f
#define PADDING_FACTOR		0.49f
#define SOLVER_ITERATION_COUNT	16
#define CONVEYOR_SCALE_FACTOR		((1.0f / gpGlobals->frametime) * 0.5f)
	
class CPhysicNovodex : public IPhysicLayer
{
private:
	NxPhysicsSDK	*m_pPhysics;	// pointer to the novodex engine
	NxScene		*m_pScene;	// pointer to world scene
	model_t		*m_pWorldModel;	// pointer to worldmodel

	char		m_szMapName[256];
	BOOL		m_fLoaded;	// collision tree is loaded and actual
	BOOL		m_fDisableWarning;	// some warnings will be swallowed
	BOOL		m_fWorldChanged;	// world is changed refresh the statics in case their scale was changed too
	BOOL		m_fNeedFetchResults;

	NxTriangleMesh	*m_pSceneMesh;
	NxActor		*m_pSceneActor;	// scene with installed shape
	NxBounds3		worldBounds;

	char		p_speeds_msg[1024];	// debug message

	NxErrorStream	m_ErrorStream;
	NxCookingInterface	*m_pCooking;
	NxUtilLib		*m_pUtils;

	cvar_t		*fps_max;
public:
	void		InitPhysic( void );
	void		FreePhysic( void );
	void		*GetUtilLibrary( void );
	void		Update( float flTime );
	void		EndFrame( void );
	void		RemoveBody( edict_t *pEdict );
	void		*CreateBodyFromEntity( CBaseEntity *pEntity );
	void		*CreateBoxFromEntity( CBaseEntity *pObject );
	void		*CreateKinematicBodyFromEntity( CBaseEntity *pEntity );
	void		*CreateStaticBodyFromEntity( CBaseEntity *pObject );
	void		*CreateVehicle( CBaseEntity *pObject, string_t scriptName = 0 );
	void		*RestoreBody( CBaseEntity *pEntity );
	void		SaveBody( CBaseEntity *pObject );
	bool		Initialized( void ) { return (m_pPhysics != NULL); }
	void		SetOrigin( CBaseEntity *pEntity, const Vector &origin );
	void		SetAngles( CBaseEntity *pEntity, const Vector &angles );
	void		SetVelocity( CBaseEntity *pEntity, const Vector &velocity );
	void		SetAvelocity( CBaseEntity *pEntity, const Vector &velocity );
	void		MoveObject( CBaseEntity *pEntity, const Vector &finalPos );
	void		RotateObject( CBaseEntity *pEntity, const Vector &finalAngle );
	void		SetLinearMomentum( CBaseEntity *pEntity, const Vector &velocity );
	void		AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor );
	void		AddForce( CBaseEntity *pEntity, const Vector &force );
	void		EnableCollision( CBaseEntity *pEntity, int fEnable );
	void		MakeKinematic( CBaseEntity *pEntity, int fEnable );
	void		UpdateVehicle( CBaseEntity *pObject );
	int		FLoadTree( char *szMapName );
	int		CheckBINFile( char *szMapName );
	int		BuildCollisionTree( char *szMapName );
	bool		UpdateEntityPos( CBaseEntity *pEntity );
	void		UpdateEntityAABB( CBaseEntity *pEntity );
	bool		UpdateActorPos( CBaseEntity *pEntity );
	void		SetupWorld( void );	
	void		DebugDraw( void );
	void		DrawPSpeeds( void );
	void		FreeAllBodies( void );

	void		TeleportCharacter( CBaseEntity *pEntity );
	void		TeleportActor( CBaseEntity *pEntity );
	void		MoveCharacter( CBaseEntity *pEntity );
	void		MoveKinematic( CBaseEntity *pEntity );
	void		SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, struct trace_s *tr );
	void		SweepEntity( CBaseEntity *pEntity, const Vector &start, const Vector &end, struct gametrace_s *tr );
	bool		IsBodySleeping( CBaseEntity *pEntity );
	void		*GetCookingInterface( void ) { return m_pCooking; }
	void		*GetPhysicInterface( void ) { return m_pPhysics; }
private:
	// misc routines
	int		ConvertEdgeToIndex( model_t *model, int edge );
	NxConvexMesh	*ConvexMeshFromBmodel( entvars_t *pev, int modelindex );
	NxConvexMesh	*ConvexMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin );
	NxConvexMesh	*ConvexMeshFromEntity( CBaseEntity *pObject );
	NxTriangleMesh	*TriangleMeshFromBmodel( entvars_t *pev, int modelindex );
	NxTriangleMesh	*TriangleMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin );
	NxTriangleMesh	*TriangleMeshFromEntity( CBaseEntity *pObject );
	NxActor		*ActorFromEntity( CBaseEntity *pObject );
	CBaseEntity	*EntityFromActor( NxActor *pObject );
	void		*CreateForceField( CBaseEntity *pEntity, const Vector &force );

	int		CheckFileTimes( const char *szFile1, const char *szFile2 );
	void		CacheNameForModel( model_t *model, fs::Path &hullfile, uint32_t hash, const char *suffix );
	uint32_t	GetHashForModelState( model_t *model, int32_t body, int32_t skin );

	void		StudioCalcBoneQuaterion( mstudiobone_t *pbone, mstudioanim_t *panim, Vector4D &q );
	void		StudioCalcBonePosition( mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos );
	clipfile::GeometryType ShapeTypeToGeomType(NxShapeType shapeType);
	bool		P_SpeedsMessage( char *out, size_t size );
};
