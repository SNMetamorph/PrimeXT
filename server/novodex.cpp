/*
novodex.cpp - this file is a part of Novodex physics engine implementation
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

#include "physic.h"		// must be first!
#ifdef USE_PHYSICS_ENGINE

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "client.h"
#include "bspfile.h"
#include "triangleapi.h"
#include "studio.h"
#include "novodex.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "animation.h"
#include "trace.h"
#include "game.h"
#include "build.h"
#include "meshdesc_factory.h"

#if defined (HAS_PHYSIC_VEHICLE)
#include "NxVehicle.h"
#include "vehicles/NxParser.h"
#endif

CPhysicNovodex	NovodexPhysic;
IPhysicLayer	*WorldPhysic = &NovodexPhysic;

// exports given from physics SDK
static NxPhysicsSDK* (__cdecl *pNxCreatePhysicsSDK)( 
	NxU32 sdkVersion, 
	NxUserAllocator* allocator, 
	NxUserOutputStream* outputStream,
	const NxPhysicsSDKDesc& desc, 
	NxSDKCreateError *errorCode
);
static NxCookingInterface *(__cdecl *pNxGetCookingLib)( NxU32 sdk_version_number );
static void *(__cdecl *pNxReleasePhysicsSDK)( NxPhysicsSDK *sdk );
static NxUtilLib* (__cdecl *pNxGetUtilLib)( void );

static dllfunc_t NxPhysics[] =
{
{ "NxCreatePhysicsSDK",	(void **)&pNxCreatePhysicsSDK },
{ "NxReleasePhysicsSDK",	(void **)&pNxReleasePhysicsSDK },
{ "NxGetCookingLib",	(void **)&pNxGetCookingLib },
{ "NxGetUtilLib",		(void **)&pNxGetUtilLib },
{ NULL, NULL },
};

static dllhandle_t hPhysics = NULL;

class DebugRenderer
{
public:
	NX_INLINE void setupColor( NxU32 color ) const
	{
		NxF32 Blue = NxF32((color) & 0xff) / 255.0f;
		NxF32 Green = NxF32((color>>8) & 0xff) / 255.0f;
		NxF32 Red	= NxF32((color>>16) & 0xff) / 255.0f;
		Tri->Color4f( Red, Green, Blue, 1.0f );
	}

	void renderData( const NxDebugRenderable& data ) const
	{
		// Render points
		NxU32 NbPoints = data.getNbPoints();
		const NxDebugPoint* Points = data.getPoints();

		Tri->Begin( TRI_POINTS );
		while( NbPoints-- )
		{
			setupColor( Points->color );
			Tri->Vertex3fv( (float *)&Points->p.x );
			Points++;
		}
		Tri->End();

		// Render lines
		NxU32 NbLines = data.getNbLines();
		const NxDebugLine* Lines = data.getLines();

		Tri->Begin( TRI_LINES );
		while( NbLines-- )
		{
			setupColor( Lines->color );
			Tri->Vertex3fv( (float *)&Lines->p0.x );
			Tri->Vertex3fv( (float *)&Lines->p1.x );
			Lines++;
		}
		Tri->End();

		// Render triangles
		NxU32 NbTris = data.getNbTriangles();
		const NxDebugTriangle* Triangles = data.getTriangles();

		Tri->Begin( TRI_TRIANGLES );
		while( NbTris-- )
		{
			setupColor( Triangles->color );
			Tri->Vertex3fv( (float *)&Triangles->p0.x );
			Tri->Vertex3fv( (float *)&Triangles->p1.x );
			Tri->Vertex3fv( (float *)&Triangles->p2.x );
			Triangles++;
		}
		Tri->End();
	}
} gDebugRenderer;

class ContactReport : public NxUserContactReport
{
public:
	virtual void onContactNotify( NxContactPair& pair, NxU32 events )
	{
		if( !FBitSet( events, NX_NOTIFY_ON_TOUCH ))
			return;

		edict_t *e1 = (edict_t *)pair.actors[0]->userData;
		edict_t *e2 = (edict_t *)pair.actors[1]->userData;

		if( !e1 || !e2 ) return;

		if( e1->v.flags & FL_CONVEYOR )
		{
			Vector basevelocity = e1->v.movedir * e1->v.speed * CONVEYOR_SCALE_FACTOR;
			pair.actors[1]->setLinearMomentum( basevelocity );
		}

		if( e2->v.flags & FL_CONVEYOR )
		{
			Vector basevelocity = e2->v.movedir * e2->v.speed * CONVEYOR_SCALE_FACTOR;
			pair.actors[0]->setLinearMomentum( basevelocity );
		}

		if( e1 && e1->v.solid != SOLID_NOT )
		{
			// FIXME: build trace info
			DispatchTouch( e1, e2 );
		}

		if( e2 && e2->v.solid != SOLID_NOT )
		{
			// FIXME: build trace info
			DispatchTouch( e1, e2 );
		}
	}
} gContactReport;

void CPhysicNovodex :: InitPhysic( void )
{
	if( m_pPhysics )
	{
		ALERT( at_error, "InitPhysic: physics already initalized\n" );
		return;
	}

	if( g_allow_physx != NULL && g_allow_physx->value == 0.0f )
	{
		ALERT( at_console, "InitPhysic: PhysX support is disabled by user.\n" );
		GameInitNullPhysics ();
		return;
	}

#if !XASH_64BIT
	const char *libraryName = "PhysXLoader.dll";
	const char *libraryGlobalName = "*PhysXLoader.dll";
#else
	const char *libraryName = "PhysXLoader64.dll";
	const char *libraryGlobalName = "*PhysXLoader64.dll";
#endif

	// trying to load dlls from mod-folder
	if (!Sys_LoadLibrary(libraryName, &hPhysics, NxPhysics))
	{
		// NOTE: using '*' symbol to force loading dll from system path not game folder (Nvidia PhysX drivers)
		if (!Sys_LoadLibrary(libraryGlobalName, &hPhysics, NxPhysics))
		{
			ALERT(at_error, "InitPhysic: failed to loading \"%s\"\nPhysics abstraction layer will be disabled.\n", libraryName);
			GameInitNullPhysics();
			return;
		}
	}

	m_pPhysics = pNxCreatePhysicsSDK( NX_PHYSICS_SDK_VERSION, NULL, &m_ErrorStream, NxPhysicsSDKDesc(), NULL );

	if( !m_pPhysics )
	{
		ALERT( at_error, "InitPhysic: failed to initalize physics engine\n" );
		Sys_FreeLibrary( &hPhysics );
		GameInitNullPhysics ();
		return;
	}

	m_pCooking = pNxGetCookingLib( NX_PHYSICS_SDK_VERSION );

	if( !m_pCooking )
	{
		ALERT( at_warning, "InitPhysic: failed to initalize cooking library\n" );
	}

	m_pUtils = pNxGetUtilLib();

	if( !m_pUtils )
	{
		ALERT( at_warning, "InitPhysic: failed to initalize util library\n" );
	}

	float maxSpeed = CVAR_GET_FLOAT( "sv_maxspeed" );

	m_pPhysics->setParameter( NX_SKIN_WIDTH, 0.25f );

	m_pPhysics->setParameter( NX_VISUALIZATION_SCALE, 1.0f );
	m_pPhysics->setParameter( NX_VISUALIZE_COLLISION_SHAPES, 1 );
	m_pPhysics->setParameter( NX_VISUALIZE_CONTACT_POINT, 1 );
	m_pPhysics->setParameter( NX_VISUALIZE_CONTACT_NORMAL, 1 );
	m_pPhysics->setParameter( NX_MAX_ANGULAR_VELOCITY, maxSpeed );
	m_pPhysics->setParameter( NX_CONTINUOUS_CD, 1 );
	m_pPhysics->setParameter( NX_VISUALIZE_BODY_AXES, 1 );
	m_pPhysics->setParameter( NX_DEFAULT_SLEEP_LIN_VEL_SQUARED, 5.0f );
	m_pPhysics->setParameter( NX_DEFAULT_SLEEP_ANG_VEL_SQUARED, 5.0f );
	m_pPhysics->setParameter( NX_VISUALIZE_FORCE_FIELDS, 1.0f );
	m_pPhysics->setParameter( NX_ADAPTIVE_FORCE, 0.0f );

	// create a scene
	NxSceneDesc sceneDesc;

	sceneDesc.userContactReport = &gContactReport;
	sceneDesc.gravity = NxVec3( 0.0f, 0.0f, -800.0f );
	sceneDesc.maxTimestep = (1.0f / 150.0f);
	sceneDesc.bpType = NX_BP_TYPE_SAP_MULTI;
	sceneDesc.maxIter = SOLVER_ITERATION_COUNT;
	sceneDesc.dynamicStructure = NX_PRUNING_DYNAMIC_AABB_TREE;

	worldBounds.min = NxVec3( -32768, -32768, -32768 );
	worldBounds.max = NxVec3(  32768,  32768,  32768 );
	sceneDesc.maxBounds = &worldBounds;
	sceneDesc.nbGridCellsX = 8;
	sceneDesc.nbGridCellsY = 8;
	sceneDesc.upAxis = 2;

	m_pScene = m_pPhysics->createScene( sceneDesc );

	// notify on all contacts:
	m_pScene->setActorGroupPairFlags( 0, 0, NX_NOTIFY_ON_TOUCH );

	NxMaterial *defaultMaterial = m_pScene->getMaterialFromIndex( 0 ); 
	defaultMaterial->setStaticFriction( 0.5f );
	defaultMaterial->setDynamicFriction( 0.5f );
	defaultMaterial->setRestitution( 0.0f );

	NxMaterialDesc conveyorMat;
	NxMaterial *conveyorMaterial = m_pScene->createMaterial( conveyorMat ); 
	conveyorMaterial->setStaticFriction( 1.0f );
	conveyorMaterial->setDynamicFriction( 1.0f );
	conveyorMaterial->setRestitution( 0.0f );
	conveyorMaterial->setDirOfAnisotropy( NxVec3( 0, 0, 1 ));
	conveyorMaterial->setFlags( NX_MF_ANISOTROPIC );

	m_fNeedFetchResults = FALSE;
}

void CPhysicNovodex :: FreePhysic( void )
{
	if( !m_pPhysics ) return;

	if( m_pCooking )
		m_pCooking->NxCloseCooking();

	m_pPhysics->releaseScene( *m_pScene );
	pNxReleasePhysicsSDK( m_pPhysics );

	Sys_FreeLibrary( &hPhysics );
	m_pPhysics = NULL;
	m_pScene = NULL;
}

void *CPhysicNovodex :: GetUtilLibrary( void )
{
	return (void *)m_pUtils;
}

void CPhysicNovodex :: Update( float flTime )
{
	if( !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE )
		return;

	if( g_psv_gravity )
	{
		// clamp gravity
		if( g_psv_gravity->value < 0.0f )
			CVAR_SET_FLOAT( "sv_gravity", 0.0f );
		if( g_psv_gravity->value > 800.0f )
			CVAR_SET_FLOAT( "sv_gravity", 800.0f );

		NxVec3 gravity;
		m_pScene->getGravity( gravity );

		if( gravity.z != -( g_psv_gravity->value ))
		{
			ALERT( at_aiconsole, "gravity changed from %g to %g\n", gravity.z, -(g_psv_gravity->value));
			gravity.z = -(g_psv_gravity->value);
			m_pScene->setGravity( gravity );
		}
	}

	if( g_sync_physic.value )
	{
		m_pScene->simulate( flTime );
		m_pScene->flushStream();
		m_pScene->fetchResults( NX_RIGID_BODY_FINISHED, true );
	}
	else
	{
		if( m_fNeedFetchResults )
			return; // waiting

		m_pScene->simulate( flTime );
		m_fNeedFetchResults = TRUE;
	}
}

void CPhysicNovodex :: EndFrame( void )
{
	if( !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE )
		return;

	if( m_fNeedFetchResults )
	{
		m_pScene->flushStream();
		m_pScene->fetchResults( NX_RIGID_BODY_FINISHED, true );
		m_fNeedFetchResults = FALSE;
	}

	// fill physics stats
	if( !p_speeds || p_speeds->value <= 0.0f )
		return;

	NxSceneStats stats;
	m_pScene->getStats( stats );

	switch( (int)p_speeds->value )
	{
	case 1:
		Q_snprintf( p_speeds_msg, sizeof( p_speeds_msg ), "%3i active bodies, %3i actors\n%3i static shapes, %3i dynamic shapes",
		stats.numDynamicActorsInAwakeGroups, stats.numActors, stats.numStaticShapes, stats.numDynamicShapes );
		break;		
	}
}

void CPhysicNovodex :: RemoveBody( struct edict_s *pEdict )
{
	if( !m_pScene || !pEdict || pEdict->free )
		return; // scene purge all the objects automatically

	CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
	NxActor *pActor = ActorFromEntity( pEntity );

	if( pActor ) m_pScene->releaseActor( *pActor );
	pEntity->m_pUserData = NULL;
}

NxConvexMesh *CPhysicNovodex :: ConvexMeshFromBmodel( entvars_t *pev, int modelindex )
{
	if( !m_pCooking )
		return NULL; // don't spam console about missed NxCooking.dll

	if( modelindex == 1 )
	{
		ALERT( at_error, "ConvexMeshFromBmodel: can't create convex hull from worldmodel\n" );
		return NULL; // don't create rigidbody from world
          }

	model_t *bmodel;

	// get a world struct
	if(( bmodel = (model_t *)MODEL_HANDLE( modelindex )) == NULL )
	{
		ALERT( at_error, "ConvexMeshFromBmodel: unbale to fetch model pointer %i\n", modelindex );
		return NULL;
	}

	if( bmodel->nummodelsurfaces <= 0 )
	{
		ALERT( at_aiconsole, "ConvexMeshFromBmodel: %s has no visible surfaces\n", bmodel->name );
		m_fDisableWarning = TRUE;
		return NULL;
	}

	int numVerts = 0, totalVerts = 0;
	NxConvexMesh *pHull = NULL;
	msurface_t *psurf;
	Vector *verts;
	int i, j;

	// compute vertexes count
	psurf = &bmodel->surfaces[bmodel->firstmodelsurface];
	for( i = 0; i < bmodel->nummodelsurfaces; i++, psurf++ )
		totalVerts += psurf->numedges;

	// create a temp vertices array
	verts = new Vector[totalVerts];

	psurf = &bmodel->surfaces[bmodel->firstmodelsurface];
	for( i = 0; i < bmodel->nummodelsurfaces; i++, psurf++ )
	{
		for( j = 0; j < psurf->numedges; j++ )
		{
			int e = bmodel->surfedges[psurf->firstedge+j];
			int v = (e > 0) ? bmodel->edges[e].v[0] : bmodel->edges[-e].v[1];
			verts[numVerts++] = bmodel->vertexes[v].position;
		}
	}

	NxConvexMeshDesc meshDesc;
	meshDesc.points = verts;
	meshDesc.pointStrideBytes = sizeof(Vector);
	meshDesc.numVertices = numVerts;
	meshDesc.flags |= NX_CF_COMPUTE_CONVEX;
	m_pCooking->NxInitCooking();

	MemoryWriteBuffer buf;
	bool status = m_pCooking->NxCookConvexMesh( meshDesc, buf );
	delete [] verts;

	if( !status )
	{
		ALERT( at_error, "failed to create convex mesh from %s\n", bmodel->name );
		return NULL;
	}

	pHull = m_pPhysics->createConvexMesh( MemoryReadBuffer( buf.data ));
	if( !pHull ) ALERT( at_error, "failed to create convex mesh from %s\n", bmodel->name );

	return pHull;
}

NxTriangleMesh *CPhysicNovodex :: TriangleMeshFromBmodel( entvars_t *pev, int modelindex )
{
	if( !m_pCooking )
		return NULL; // don't spam console about missed NxCooking.dll

	if( modelindex == 1 )
	{
		ALERT( at_error, "TriangleMeshFromBmodel: can't create triangle mesh from worldmodel\n" );
		return NULL; // don't create rigidbody from world
          }

	model_t *bmodel;

	// get a world struct
	if(( bmodel = (model_t *)MODEL_HANDLE( modelindex )) == NULL )
	{
		ALERT( at_error, "TriangleMeshFromBmodel: unable to fetch model pointer %i\n", modelindex );
		return NULL;
	}

	if( bmodel->nummodelsurfaces <= 0 )
	{
		ALERT( at_aiconsole, "TriangleMeshFromBmodel: %s has no visible surfaces\n", bmodel->name );
		m_fDisableWarning = TRUE;
		return NULL;
	}

	// don't build hulls for water
	if( FBitSet( bmodel->flags, MODEL_LIQUID ))
	{
		m_fDisableWarning = TRUE;
		return NULL;
	}

	int i, numElems = 0, totalElems = 0;
	NxTriangleMesh *pMesh = NULL;
	msurface_t *psurf;

	// compute vertexes count
	for( i = 0; i < bmodel->nummodelsurfaces; i++ )
	{
		psurf = &bmodel->surfaces[bmodel->firstmodelsurface + i];
		totalElems += (psurf->numedges - 2);
	}

	// create a temp indices array
	NxU32 *indices = new NxU32[totalElems * 3];

	for( i = 0; i < bmodel->nummodelsurfaces; i++ )
	{
		msurface_t *face = &bmodel->surfaces[bmodel->firstmodelsurface + i];
		bool reverse = (face->flags & SURF_PLANEBACK) ? true : false;
		int k = face->firstedge;

		// build the triangles from polygons
		for( int j = 0; j < face->numedges - 2; j++ )
		{
			indices[numElems*3+0] = ConvertEdgeToIndex( bmodel, k );
			indices[numElems*3+1] = ConvertEdgeToIndex( bmodel, k + j + 2 );
			indices[numElems*3+2] = ConvertEdgeToIndex( bmodel, k + j + 1 );
			numElems++;
		}
	}

	NxTriangleMeshDesc meshDesc;
	meshDesc.points = (const NxPoint*)&(bmodel->vertexes[0].position);	// pointer to all vertices in the map
	meshDesc.pointStrideBytes = sizeof( mvertex_t );
	meshDesc.triangleStrideBytes = 3 * sizeof( NxU32 );
	meshDesc.numVertices = bmodel->numvertexes;
	meshDesc.numTriangles = numElems;
	meshDesc.triangles = indices;
	meshDesc.flags = 0;
	m_pCooking->NxInitCooking();

	MemoryWriteBuffer buf;
	bool status = m_pCooking->NxCookTriangleMesh( meshDesc, buf );
	delete [] indices;

	if( !status )
	{
		ALERT( at_error, "failed to create triangle mesh from %s\n", bmodel->name );
		return NULL;
	}

	pMesh = m_pPhysics->createTriangleMesh( MemoryReadBuffer( buf.data ));
	if( !pMesh ) ALERT( at_error, "failed to create triangle mesh from %s\n", bmodel->name );

	return pMesh;
}

void CPhysicNovodex :: StudioCalcBoneQuaterion( mstudiobone_t *pbone, mstudioanim_t *panim, Vector4D &q )
{
	mstudioanimvalue_t *panimvalue;
	Radian angle;

	for( int j = 0; j < 3; j++ )
	{
		if( !panim || panim->offset[j+3] == 0 )
		{
			angle[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			angle[j] = panimvalue[1].value;
			angle[j] = pbone->value[j+3] + angle[j] * pbone->scale[j+3];
		}
	}

	AngleQuaternion( angle, q );
}

void CPhysicNovodex :: StudioCalcBonePosition( mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos )
{
	mstudioanimvalue_t *panimvalue;

	for( int j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;

		if( panim && panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			pos[j] += panimvalue[1].value * pbone->scale[j];
		}
	}
}

clipfile::GeometryType CPhysicNovodex::ShapeTypeToGeomType(NxShapeType shapeType)
{
	switch (shapeType)
	{
		case NX_SHAPE_CONVEX: return clipfile::GeometryType::CookedConvexHull;
		case NX_SHAPE_MESH: return clipfile::GeometryType::CookedTriangleMesh;
		case NX_SHAPE_BOX: return clipfile::GeometryType::CookedBox;
		default: return clipfile::GeometryType::Unknown;
	}
}

NxConvexMesh *CPhysicNovodex :: ConvexMeshFromStudio( entvars_t *pev, int modelindex )
{
	if( UTIL_GetModelType( modelindex ) != mod_studio )
	{
		ALERT( at_error, "CollisionFromStudio: not a studio model\n" );
		return NULL;
          }

	model_t *smodel = (model_t *)MODEL_HANDLE( modelindex );
	studiohdr_t *phdr = (studiohdr_t *)smodel->cache.data;

	if( !phdr || phdr->numbones < 1 )
	{
		ALERT( at_error, "CollisionFromStudio: bad model header\n" );
		return NULL;
	}

	char szHullFilename[MAX_PATH];
	NxConvexMesh *pHull = NULL;

	HullNameForModel( smodel->name, szHullFilename, sizeof( szHullFilename ));

	if( CheckFileTimes( smodel->name, szHullFilename ))
	{
		// hull is never than studiomodel. Trying to load it
		pHull = m_pPhysics->createConvexMesh( UserStream( szHullFilename, true ));

		if( !pHull )
		{
			// we failed to loading existed hull and can't cooking new :(
			if( m_pCooking == NULL )
				return NULL; // don't spam console about missed nxCooking.dll

			// trying to rebuild hull
			ALERT( at_error, "Convex mesh for %s is corrupted. Rebuilding...\n", smodel->name );
		}
		else
		{
			// all is ok
			return pHull;
		}
	}
	else
	{
		// can't cooking new hull because nxCooking.dll is missed
		if( m_pCooking == NULL )
			return NULL; // don't spam console about missed nxCooking.dll

		// trying to rebuild hull
		ALERT( at_console, "Convex mesh for %s is out of Date. Rebuilding...\n", smodel->name );
	}

	// at this point nxCooking instance is always valid

	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex) + pseqdesc->seqgroup;

	// sanity check
	if( pseqdesc->seqgroup != 0 )
	{
		ALERT( at_error, "CollisionFromStudio: bad sequence group (must be 0)\n" );
		return NULL;
	}

	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqgroup->data + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];

	for( int i = 0; i < phdr->numbones; i++, pbone++, panim++ ) 
	{
		StudioCalcBoneQuaterion( pbone, panim, q[i] );
		StudioCalcBonePosition( pbone, panim, pos[i] );
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix4x4	transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	transform.Identity();

	// compute bones for default anim
	for( int i = 0; i < phdr->numbones; i++ ) 
	{
		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( pbone[i].parent == -1 ) 
			bonetransform[i] = transform.ConcatTransforms( bonematrix );
		else bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms( bonematrix );
	}

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex);
	mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex);
	Vector *pstudioverts = (Vector *)((byte *)phdr + psubmodel->vertindex);
	Vector *m_verts = new Vector[psubmodel->numverts];
	byte *pvertbone = ((byte *)phdr + psubmodel->vertinfoindex);
	Vector *verts = new Vector[psubmodel->numverts * 8];	// allocate temporary vertices array
	NxU32 *indices = new NxU32[psubmodel->numverts * 24];
	int numVerts = 0, numElems = 0;
	Vector tmp;

	// setup all the vertices
	for( int i = 0; i < psubmodel->numverts; i++ )
		m_verts[i] = bonetransform[pvertbone[i]].VectorTransform( pstudioverts[i] );

	for( int j = 0; j < psubmodel->nummesh; j++ ) 
	{
		int i;
		mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)phdr + psubmodel->meshindex) + j;
		short *ptricmds = (short *)((byte *)phdr + pmesh->triindex);

		while( i = *( ptricmds++ ))
		{
			int	vertexState = 0;
			qboolean	tri_strip;

			if( i < 0 )
			{
				tri_strip = false;
				i = -i;
			}
			else
				tri_strip = true;

			for( ; i > 0; i--, ptricmds += 4 )
			{
				// build in indices
				if( vertexState++ < 3 )
				{
					indices[numElems++] = numVerts;
				}
				else if( tri_strip )
				{
					// flip triangles between clockwise and counter clockwise
					if( vertexState & 1 )
					{
						// draw triangle [n-2 n-1 n]
						indices[numElems++] = numVerts - 2;
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts;
					}
					else
					{
						// draw triangle [n-1 n-2 n]
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts - 2;
						indices[numElems++] = numVerts;
					}
				}
				else
				{
					// draw triangle fan [0 n-1 n]
					indices[numElems++] = numVerts - ( vertexState - 1 );
					indices[numElems++] = numVerts - 1;
					indices[numElems++] = numVerts;
				}

				verts[numVerts++] = m_verts[ptricmds[0]];
			}
		}
	}

	NxConvexMeshDesc meshDesc;
	meshDesc.numTriangles = numElems / 3;
	meshDesc.pointStrideBytes = sizeof(Vector);
	meshDesc.triangleStrideBytes	= 3 * sizeof( NxU32 );
	meshDesc.points = verts;
	meshDesc.triangles = indices;
	meshDesc.numVertices = numVerts;
	meshDesc.flags |= NX_CF_COMPUTE_CONVEX;

	m_pCooking->NxInitCooking();
	bool status = m_pCooking->NxCookConvexMesh( meshDesc, UserStream( szHullFilename, false ));

	delete [] verts;
	delete [] m_verts;
	delete [] indices;

	if( !status )
	{
		ALERT( at_error, "failed to create convex mesh from %s\n", smodel->name );
		return NULL;
	}

	pHull = m_pPhysics->createConvexMesh( UserStream( szHullFilename, true ));
	if( !pHull ) ALERT( at_error, "failed to create convex mesh from %s\n", smodel->name );

	return pHull;
}

NxTriangleMesh *CPhysicNovodex::TriangleMeshFromStudio(entvars_t *pev, int modelindex)
{
	if (UTIL_GetModelType(modelindex) != mod_studio)
	{
		ALERT(at_error, "TriangleMeshFromStudio: not a studio model\n");
		return NULL;
	}

	model_t *smodel = (model_t *)MODEL_HANDLE(modelindex);
	studiohdr_t *phdr = (studiohdr_t *)smodel->cache.data;
	int solidMeshes = 0;

	if (!phdr || phdr->numbones < 1)
	{
		ALERT(at_error, "TriangleMeshFromStudio: bad model header\n");
		return NULL;
	}

	mstudiotexture_t *ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);

	for (int i = 0; i < phdr->numtextures; i++)
	{
		// skip this mesh it's probably foliage or somewhat
		if (ptexture[i].flags & STUDIO_NF_MASKED)
			continue;
		solidMeshes++;
	}

	// model is non-solid
	if (!solidMeshes)
	{
		m_fDisableWarning = TRUE;
		return NULL;
	}

	char szMeshFilename[MAX_PATH];
	NxTriangleMesh *pMesh = NULL;

	MeshNameForModel(smodel->name, szMeshFilename, sizeof(szMeshFilename));

	if (CheckFileTimes(smodel->name, szMeshFilename) && !m_fWorldChanged)
	{
		// hull is never than studiomodel. Trying to load it
		pMesh = m_pPhysics->createTriangleMesh(UserStream(szMeshFilename, true));

		if (!pMesh)
		{
			// we failed to loading existed hull and can't cooking new :(
			if (m_pCooking == NULL)
				return NULL; // don't spam console about missed nxCooking.dll

			// trying to rebuild hull
			ALERT(at_error, "Triangle mesh for %s is corrupted. Rebuilding...\n", smodel->name);
		}
		else
		{
			// all is ok
			return pMesh;
		}
	}
	else
	{
		// can't cooking new hull because nxCooking.dll is missed
		if (m_pCooking == NULL)
			return NULL; // don't spam console about missed nxCooking.dll

		// trying to rebuild hull
		ALERT(at_console, "Triangle mesh for %s is out of Date. Rebuilding...\n", smodel->name);
	}

	// at this point nxCooking instance is always valid
	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex) + pseqdesc->seqgroup;

	// sanity check
	if (pseqdesc->seqgroup != 0)
	{
		ALERT(at_error, "TriangleMeshFromStudio: bad sequence group (must be 0)\n");
		return NULL;
	}

	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqgroup->data + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];

	for (int i = 0; i < phdr->numbones; i++, pbone++, panim++)
	{
		StudioCalcBoneQuaterion(pbone, panim, q[i]);
		StudioCalcBonePosition(pbone, panim, pos[i]);
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix4x4 transform, bonematrix, bonetransform[MAXSTUDIOBONES];

	if (pev->startpos != g_vecZero)
		transform = matrix3x4(g_vecZero, g_vecZero, pev->startpos);
	else transform.Identity();

	// compute bones for default anim
	for (int i = 0; i < phdr->numbones; i++)
	{
		// initialize bonematrix
		bonematrix = matrix3x4(pos[i], q[i]);

		if (pbone[i].parent == -1)
			bonetransform[i] = transform.ConcatTransforms(bonematrix);
		else bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms(bonematrix);
	}

	int colliderBodygroup = pev->body;
	int totalVertSize = 0;
	for (int i = 0; i < phdr->numbodyparts; i++)
	{
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + i;
		int index = (colliderBodygroup / pbodypart->base) % pbodypart->nummodels;
		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		totalVertSize += psubmodel->numverts;
	}

	Vector *verts = new Vector[totalVertSize * 8]; // allocate temporary vertices array
	NxU32 *indices = new NxU32[totalVertSize * 24];
	int numVerts = 0, numElems = 0;
	Vector tmp;

	for (int k = 0; k < phdr->numbodyparts; k++)
	{
		int i;
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + k;
		int index = (colliderBodygroup / pbodypart->base) % pbodypart->nummodels;
		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		Vector *pstudioverts = (Vector *)((byte *)phdr + psubmodel->vertindex);
		Vector *m_verts = new Vector[psubmodel->numverts];
		byte *pvertbone = ((byte *)phdr + psubmodel->vertinfoindex);

		// setup all the vertices
		for (i = 0; i < psubmodel->numverts; i++) {
			m_verts[i] = bonetransform[pvertbone[i]].VectorTransform(pstudioverts[i]);
		}

		ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);
		short *pskinref = (short *)((byte *)phdr + phdr->skinindex);

		for (int j = 0; j < psubmodel->nummesh; j++)
		{
			mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)phdr + psubmodel->meshindex) + j;
			short *ptricmds = (short *)((byte *)phdr + pmesh->triindex);

			if (phdr->numtextures != 0 && phdr->textureindex != 0)
			{
				// skip this mesh it's probably foliage or somewhat
				if (ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_MASKED)
					continue;
			}

			while (i = *(ptricmds++))
			{
				int	vertexState = 0;
				bool tri_strip;

				if (i < 0)
				{
					tri_strip = false;
					i = -i;
				}
				else
					tri_strip = true;

				for (; i > 0; i--, ptricmds += 4)
				{
					// build in indices
					if (vertexState++ < 3)
					{
						indices[numElems++] = numVerts;
					}
					else if (tri_strip)
					{
						// flip triangles between clockwise and counter clockwise
						if (vertexState & 1)
						{
							// draw triangle [n-2 n-1 n]
							indices[numElems++] = numVerts - 2;
							indices[numElems++] = numVerts - 1;
							indices[numElems++] = numVerts;
						}
						else
						{
							// draw triangle [n-1 n-2 n]
							indices[numElems++] = numVerts - 1;
							indices[numElems++] = numVerts - 2;
							indices[numElems++] = numVerts;
						}
					}
					else
					{
						// draw triangle fan [0 n-1 n]
						indices[numElems++] = numVerts - (vertexState - 1);
						indices[numElems++] = numVerts - 1;
						indices[numElems++] = numVerts;
					}

					//	verts[numVerts++] = m_verts[ptricmds[0]];
					verts[numVerts] = m_verts[ptricmds[0]];
					numVerts++;
				}
			}
		}

		delete[] m_verts;
	}

	NxTriangleMeshDesc meshDesc;
	meshDesc.numTriangles = numElems / 3;
	meshDesc.pointStrideBytes = sizeof(Vector);
	meshDesc.triangleStrideBytes = 3 * sizeof(NxU32);
	meshDesc.points = verts;
	meshDesc.triangles = indices;
	meshDesc.numVertices = numVerts;
	meshDesc.flags = 0;

	m_pCooking->NxInitCooking();
	bool status = m_pCooking->NxCookTriangleMesh(meshDesc, UserStream(szMeshFilename, false));

	delete[] verts;
	delete[] indices;

	if (!status)
	{
		ALERT(at_error, "failed to create triangle mesh from %s\n", smodel->name);
		return NULL;
	}

	pMesh = m_pPhysics->createTriangleMesh(UserStream(szMeshFilename, true));
	if (!pMesh) ALERT(at_error, "failed to create triangle mesh from %s\n", smodel->name);

	return pMesh;
}

NxConvexMesh *CPhysicNovodex :: ConvexMeshFromEntity( CBaseEntity *pObject )
{
	if( !pObject || !m_pPhysics )
		return NULL;

	// check for bspmodel
	model_t *model = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );

	if( !model || model->type == mod_bad )
	{
		ALERT( at_aiconsole, "ConvexMeshFromEntity: entity %s has NULL model\n", pObject->GetClassname( )); 
		return NULL;
	}

	NxConvexMesh *pCollision = NULL;

	// call the apropriate loader
	switch( model->type )
	{
	case mod_brush:
		pCollision = ConvexMeshFromBmodel( pObject->pev, pObject->pev->modelindex );	
		break;
	case mod_studio:
		pCollision = ConvexMeshFromStudio( pObject->pev, pObject->pev->modelindex );	
		break;
	}

	if( !pCollision && !m_fDisableWarning )
		ALERT( at_warning, "ConvexMeshFromEntity: %i has missing collision\n", pObject->pev->modelindex ); 
	m_fDisableWarning = FALSE;

	return pCollision;
}

NxTriangleMesh *CPhysicNovodex :: TriangleMeshFromEntity( CBaseEntity *pObject )
{
	if( !pObject || !m_pPhysics )
		return NULL;

	// check for bspmodel
	model_t *model = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );

	if( !model || model->type == mod_bad )
	{
		ALERT( at_aiconsole, "TriangleMeshFromEntity: entity %s has NULL model\n", pObject->GetClassname( )); 
		return NULL;
	}

	NxTriangleMesh *pCollision = NULL;

	// call the apropriate loader
	switch( model->type )
	{
	case mod_brush:
		pCollision = TriangleMeshFromBmodel( pObject->pev, pObject->pev->modelindex );	
		break;
	case mod_studio:
		pCollision = TriangleMeshFromStudio( pObject->pev, pObject->pev->modelindex );	
		break;
	}

	if( !pCollision && !m_fDisableWarning )
		ALERT( at_warning, "TriangleMeshFromEntity: %s has missing collision\n", pObject->GetClassname( )); 
	m_fDisableWarning = FALSE;

	return pCollision;
}

NxActor *CPhysicNovodex :: ActorFromEntity( CBaseEntity *pObject )
{
	if( FNullEnt( pObject ) || !pObject->m_pUserData )
		return NULL;
#if defined (HAS_PHYSIC_VEHICLE)
	if( pObject->m_iActorType == ACTOR_VEHICLE )
	{
		NxVehicle	*pVehicle = (NxVehicle *)pObject->m_pUserData;
		return pVehicle->getActor();
	}
#endif
	return (NxActor *)pObject->m_pUserData;
}

CBaseEntity *CPhysicNovodex :: EntityFromActor( NxActor *pObject )
{
	if( !pObject || !pObject->userData )
		return NULL;

	return CBaseEntity::Instance( (edict_t *)pObject->userData );
}

void *CPhysicNovodex :: CreateBodyFromEntity( CBaseEntity *pObject )
{
	NxConvexMesh *pCollision = ConvexMeshFromEntity( pObject );
	if( !pCollision ) return NULL;

	NxBodyDesc BodyDesc;
	NxActorDesc ActorDesc;
	NxConvexShapeDesc meshShapeDesc;
	BodyDesc.flags = NX_BF_VISUALIZATION|NX_BF_FILTER_SLEEP_VEL;
	BodyDesc.solverIterationCount = SOLVER_ITERATION_COUNT;

	ActorDesc.body = &BodyDesc;
	ActorDesc.density = DENSITY_FACTOR;
	ActorDesc.userData = pObject->edict();

	meshShapeDesc.meshData = pCollision;
	ActorDesc.shapes.pushBack( &meshShapeDesc );
	NxActor *pActor = m_pScene->createActor( ActorDesc );

	if( !pActor )
	{
		ALERT( at_error, "failed to create rigidbody from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	pActor->setName( pObject->GetClassname( ));

	NxMat34 pose;
	float mat[16];
	matrix4x4( pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f ).CopyToArray( mat );

	pose.setColumnMajor44( mat );
	pActor->setGlobalPose( pose );
	pActor->setLinearVelocity( pObject->GetLocalVelocity() );
	pActor->setAngularVelocity( pObject->GetLocalAvelocity() );
	pObject->m_iActorType = ACTOR_DYNAMIC;
	pObject->m_pUserData = pActor;

	return pActor;
}

/*
=================
CreateBoxFromEntity

used for characters: clients and monsters
=================
*/
void *CPhysicNovodex :: CreateBoxFromEntity( CBaseEntity *pObject )
{
	NxBodyDesc BodyDesc;
	BodyDesc.flags |= NX_BF_KINEMATIC|NX_BF_VISUALIZATION;

	NxActorDesc ActorDesc;
	NxBoxShapeDesc boxDesc;
	boxDesc.dimensions = pObject->pev->size * PADDING_FACTOR;

	ActorDesc.body = &BodyDesc;
	ActorDesc.density = DENSITY_FACTOR;
	ActorDesc.userData = pObject->edict();
	ActorDesc.shapes.pushBack( &boxDesc );

	NxActor *pActor = m_pScene->createActor( ActorDesc );

	if( !pActor )
	{
		ALERT( at_error, "failed to create rigidbody from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	Vector vecOffset = (pObject->IsMonster()) ? Vector( 0, 0, pObject->pev->maxs.z / 2.0f ) : g_vecZero;

	pActor->setName( pObject->GetClassname( ));
	pActor->setGlobalPosition( pObject->GetAbsOrigin() + vecOffset );
	pObject->m_iActorType = ACTOR_CHARACTER;
	pObject->m_pUserData = pActor;

	return pActor;
}

void *CPhysicNovodex :: CreateKinematicBodyFromEntity( CBaseEntity *pObject )
{
	NxTriangleMesh *pCollision = TriangleMeshFromEntity( pObject );
	if( !pCollision ) return NULL;

	NxBodyDesc BodyDesc;
	NxActorDesc ActorDesc;
	NxTriangleMeshShapeDesc meshShapeDesc;
	BodyDesc.flags = NX_BF_VISUALIZATION|NX_BF_KINEMATIC|NX_BF_FILTER_SLEEP_VEL;
	BodyDesc.solverIterationCount = SOLVER_ITERATION_COUNT;

	if( !UTIL_CanRotate( pObject ))
		BodyDesc.flags |= NX_BF_FROZEN_ROT; // entity missed origin-brush

	ActorDesc.body = &BodyDesc;
	ActorDesc.density = DENSITY_FACTOR;
	ActorDesc.userData = pObject->edict();

	meshShapeDesc.meshData = pCollision;
	ActorDesc.shapes.pushBack( &meshShapeDesc );
	m_ErrorStream.hideWarning( true );
	NxActor *pActor = m_pScene->createActor( ActorDesc );
	m_ErrorStream.hideWarning( false );

	if( !pActor )
	{
		ALERT( at_error, "failed to create kinematic from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	pActor->setName( pObject->GetClassname( ));

	NxMat34 pose;
	float mat[16];
	matrix4x4( pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f ).CopyToArray( mat );

	pose.setColumnMajor44( mat );
	pActor->setGlobalPose( pose );
	pObject->m_iActorType = ACTOR_KINEMATIC;
	pObject->m_pUserData = pActor;

	return pActor;
}

void *CPhysicNovodex :: CreateStaticBodyFromEntity( CBaseEntity *pObject )
{
	NxTriangleMesh *pCollision = TriangleMeshFromEntity( pObject );
	if( !pCollision ) return NULL;

	NxMat34 pose;
	float mat[16];
	matrix4x4( pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f ).CopyToArray( mat );

	pose.setColumnMajor44( mat );

	NxActorDesc ActorDesc;
	NxTriangleMeshShapeDesc meshShapeDesc;
	ActorDesc.density = DENSITY_FACTOR;
	ActorDesc.userData = pObject->edict();
	ActorDesc.globalPose = pose;

	if( pObject->pev->flags & FL_CONVEYOR )
		meshShapeDesc.materialIndex = 1;
	meshShapeDesc.meshData = pCollision;
	ActorDesc.shapes.pushBack( &meshShapeDesc );
	NxActor *pActor = m_pScene->createActor( ActorDesc );

	if( !pActor )
	{
		ALERT( at_error, "failed to create static from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	pActor->setName( pObject->GetClassname( ));
//	pActor->setGlobalPose( pose );
	pObject->m_iActorType = ACTOR_STATIC;
	pObject->m_pUserData = pActor;

	return pActor;
}

void *CPhysicNovodex :: CreateVehicle( CBaseEntity *pObject, string_t scriptName )
{
#if defined (HAS_PHYSIC_VEHICLE)
	NxBoxShapeDesc	boxShapes[MAXSTUDIOBONES];
	vehicleparams_t	vehicleParams;
	NxVehicleDesc	vehicleDesc;
	int		i, j, index;
	int		wheel_count;
	Vector		wheel_pos;

	if( UTIL_GetModelType( pObject->pev->modelindex ) != mod_studio )
	{
		ALERT( at_error, "CreateVehicle: not a studio model\n" );
		return NULL;
          }

	if( !ParseVehicleScript( STRING( scriptName ), vehicleParams ))
	{
		ALERT( at_error, "CreateVehicle: couldn't load %s\n", STRING( scriptName ));
		return NULL;
          }

	model_t *smodel = (model_t *)MODEL_HANDLE( pObject->pev->modelindex );
	studiohdr_t *phdr = (studiohdr_t *)smodel->cache.data;

	if( !phdr || phdr->numbones < 1 )
	{
		ALERT( at_error, "CreateVehicle: bad model header\n" );
		return NULL;
	}

	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex) + pseqdesc->seqgroup;
	mstudioattachment_t	*pattachment = (mstudioattachment_t *) ((byte *)phdr + phdr->attachmentindex);
	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqgroup->data + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];

	for( i = 0; i < phdr->numbones; i++, pbone++, panim++ )
	{
		StudioCalcBoneQuaterion( pbone, panim, q[i] );
		StudioCalcBonePosition( pbone, panim, pos[i] );
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix4x4	transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	transform.Identity();

	// compute bones for default anim
	for( i = 0; i < phdr->numbones; i++ ) 
	{
		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( pbone[i].parent == -1 ) 
			bonetransform[i] = transform.ConcatTransforms( bonematrix );
		else bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms( bonematrix );
	}

	// create body vehicle from hitboxes
	for( i = 0; i < phdr->numhitboxes; i++ )
	{
		mstudiobbox_t	*pbbox = (mstudiobbox_t *)((byte *)phdr + phdr->hitboxindex);
		vec3_t		tmp, p[8], mins, maxs, size, pos;

		ClearBounds( mins , maxs );

		for( j = 0; j < 8; j++ )
		{
			tmp[0] = (j & 1) ? pbbox[i].bbmin[0] : pbbox[i].bbmax[0];
			tmp[1] = (j & 2) ? pbbox[i].bbmin[1] : pbbox[i].bbmax[1];
			tmp[2] = (j & 4) ? pbbox[i].bbmin[2] : pbbox[i].bbmax[2];
			p[j] = bonetransform[pbbox[i].bone].VectorTransform( tmp );
			AddPointToBounds( p[j], mins, maxs );
		}

		boxShapes[i].dimensions.set( NxVec3( maxs - mins ) * 0.5 );		// half-size
		boxShapes[i].localPose.t.set( NxVec3(( mins + maxs ) * 0.5f ));	// origin
		vehicleDesc.carShapes.pushBack( &boxShapes[i] );
	}

	vehicleDesc.mass = 1200.0f;
	vehicleDesc.digitalSteeringDelta = 0.04f;
	vehicleDesc.steeringMaxAngle = 30.0f;
	vehicleDesc.motorForce = 3500.0f;
	vehicleDesc.centerOfMass.set( -24, 0, -16 );
	vehicleDesc.maxVelocity = 60.0f;
	float scale = 32.0f;

	NxVehicleMotorDesc motorDesc;
	NxVehicleGearDesc gearDesc;
	NxWheelDesc wheelDesc[VEHICLE_MAX_WHEEL_COUNT];
	NxReal wheelRadius = 20.0f;

	gearDesc.setToCorvette();
	motorDesc.setToCorvette();

	vehicleDesc.motorDesc = &motorDesc;
	vehicleDesc.gearDesc = &gearDesc;

	// setup wheels
	for( i = wheel_count = 0; i < vehicleParams.axleCount; i++ )
	{
		axleparams_t *axle = &vehicleParams.axles[i];

		for( j = 0; j < axle->wheelsPerAxle; j++ )
		{
			wheelparams_t *wheel = &axle->wheels[j];
			NxU32 flags = NX_WF_USE_WHEELSHAPE;

			wheelDesc[wheel_count].wheelApproximation = 10;
			wheelDesc[wheel_count].wheelRadius = wheel->radius;
			wheelDesc[wheel_count].wheelWidth = 0.1f * scale;	// FIXME
			wheelDesc[wheel_count].wheelSuspension = axle->suspension.springHeight;
			wheelDesc[wheel_count].springRestitution = axle->suspension.springConstant;
			wheelDesc[wheel_count].springDamping = axle->suspension.springDamping;
			wheelDesc[wheel_count].springBias = axle->suspension.springDampingCompression;
			wheelDesc[wheel_count].maxBrakeForce = axle->suspension.brakeForce;
			wheelDesc[wheel_count].frictionToFront = wheel->frontFriction;
			wheelDesc[wheel_count].frictionToSide = wheel->sideFriction;
			wheelDesc[wheel_count].wheelPoseParamIndex = pObject->LookupPoseParameter( wheel->wheelName );
			wheelDesc[wheel_count].suspensionPoseParamIndex = pObject->LookupPoseParameter( wheel->suspensionName );

			// set wheel flags
			if( axle->steerable )
				SetBits( flags, NX_WF_STEERABLE_INPUT );

			if( axle->driven )
				SetBits( flags, NX_WF_ACCELERATED );

			if( axle->affectBrake )
				SetBits( flags, NX_WF_AFFECTED_BY_HANDBRAKE );

			wheelDesc[wheel_count].wheelFlags = flags;

			// set wheel position
			if(( index = FindAttachmentByName( phdr, wheel->attachmentName )) != -1 )
			{
				wheel_pos = bonetransform[pattachment[index].bone].VectorTransform( pattachment[i].org );
				wheelDesc[wheel_count].position.set( NxVec3( wheel_pos ));
			}
			vehicleDesc.carWheels.pushBack( &wheelDesc[wheel_count] );
			wheel_count++;
		}
	}

	vehicleDesc.steeringSteerPoint.set( 1.8 * scale, 0, 0 );
	vehicleDesc.steeringTurnPoint.set( -1.5 * scale, 0, 0 );

	NxVehicle *pVehicle = NxVehicle :: createVehicle( m_pScene, pObject, &vehicleDesc );

	if( !pVehicle )
	{
		ALERT( at_error, "failed to create vehicle from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	// get steer controller index
	pVehicle->steerPoseParamIndex = pObject->LookupPoseParameter( vehicleParams.steering.steeringName );

	NxActor *pActor = pVehicle->getActor();

	if( !pActor )
	{
		ALERT( at_error, "failed to create vehicle from entity %s\n", pObject->GetClassname( ));
		delete pVehicle;
		return NULL;
	}

	pActor->setName( pObject->GetClassname( ));

	NxMat34 pose;
	float mat[16];
	matrix4x4( pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f ).CopyToArray( mat );

	pose.setColumnMajor44( mat );
	pActor->setGlobalPose( pose );
	pActor->setLinearVelocity( pObject->GetLocalVelocity() );
	pActor->setAngularVelocity( pObject->GetLocalAvelocity() );

	return pVehicle;
#else
	return NULL;
#endif
}

void CPhysicNovodex :: UpdateVehicle( CBaseEntity *pObject )
{
#if defined (HAS_PHYSIC_VEHICLE)
	if( !pObject || pObject->m_iActorType != ACTOR_VEHICLE )
		return;

	NxVehicle *pVehicle = (NxVehicle *)pObject->m_pUserData;

	for( NxU32 i = 0; i < pVehicle->getNbWheels(); i++ )
	{
		NxWheel *pWheel = (NxWheel *)pVehicle->getWheel( i );	

		pObject->SetPoseParameter( pWheel->wheelPoseParamIndex, -pWheel->getRollAngle( ));
		pObject->SetPoseParameter( pWheel->suspensionPoseParamIndex, pWheel->getSuspensionHeight( ));
	}

	CBaseEntity *pDriver = pObject->GetVehicleDriver();

	if( pDriver != NULL )
	{
		bool left = !!FBitSet( pDriver->pev->button, IN_MOVELEFT );
		bool right = !!FBitSet( pDriver->pev->button, IN_MOVERIGHT );
		bool forward = !!FBitSet( pDriver->pev->button, IN_FORWARD );
		bool backward = !!FBitSet( pDriver->pev->button, IN_BACK );
		bool handBrake = !!FBitSet( pDriver->pev->button, IN_JUMP );

		NxReal steering = 0;
		if( left && !right) steering = -1;
		else if (right && !left) steering = 1;
		NxReal acceleration = 0;
		if (forward && !backward) acceleration = -1;
		else if (backward && !forward) acceleration = 1;

		pVehicle->control( steering, false, acceleration, false, handBrake );
		pObject->SetPoseParameter( pVehicle->steerPoseParamIndex, pVehicle->getSteeringAngle() );
	}

	pVehicle->updateVehicle( gpGlobals->frametime );
#endif
}

bool CPhysicNovodex :: UpdateEntityPos( CBaseEntity *pEntity )
{
	NxActor *pActor = ActorFromEntity( pEntity );

	if( !pActor || pActor->isSleeping( ))
		return false;

	NxMat34 pose = pActor->getGlobalPose();
	float mat[16];

	pose.getColumnMajor44( mat );
	matrix4x4	m( mat );

	Vector angles = m.GetAngles();
	Vector origin = m.GetOrigin();

	// store actor velocities too
	pEntity->SetLocalVelocity( pActor->getLinearVelocity() );
	pEntity->SetLocalAvelocity( pActor->getAngularVelocity() );
	Vector vecPrevOrigin = pEntity->GetAbsOrigin();

	pEntity->SetLocalAngles( angles );
	pEntity->SetLocalOrigin( origin );
	pEntity->RelinkEntity( TRUE, &vecPrevOrigin );

	return true;
}

bool CPhysicNovodex :: UpdateActorPos( CBaseEntity *pEntity )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return false;

	NxMat34 pose;
	float mat[16];

	matrix4x4	m( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), 1.0f );
	m.CopyToArray( mat );

	pose.setColumnMajor44( mat );
	pActor->setGlobalPose( pose );

	if( !pActor->readBodyFlag( NX_BF_KINEMATIC ))
	{
		pActor->setLinearVelocity( pEntity->GetLocalVelocity() );
		pActor->setAngularVelocity( pEntity->GetLocalAvelocity() );
	}

	return true;
}

bool CPhysicNovodex :: IsBodySleeping( CBaseEntity *pEntity )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return false;
	return pActor->isSleeping();
}

void CPhysicNovodex :: UpdateEntityAABB( CBaseEntity *pEntity )
{
	NxActor *pActor = ActorFromEntity( pEntity );

	if( !pActor || pActor->getNbShapes() <= 0 )
		return;

	ClearBounds( pEntity->pev->absmin, pEntity->pev->absmax );

	for( uint i = 0; i < pActor->getNbShapes(); i++ )
	{
		NxShape *pShape = pActor->getShapes()[i];
		NxBounds3 bbox;

		// already transformed as OBB
		pShape->getWorldBounds( bbox );

		AddPointToBounds( bbox.min, pEntity->pev->absmin, pEntity->pev->absmax );
		AddPointToBounds( bbox.max, pEntity->pev->absmin, pEntity->pev->absmax );
	}

	// shrink AABB by 1 units in each axis
	// or pushers can't be moving them
	pEntity->pev->absmin.x += 1;
	pEntity->pev->absmin.y += 1;
	pEntity->pev->absmin.z += 1;
	pEntity->pev->absmax.x -= 1;
	pEntity->pev->absmax.y -= 1;
	pEntity->pev->absmax.z -= 1;

	pEntity->pev->mins = pEntity->pev->absmin - pEntity->pev->origin;
	pEntity->pev->maxs = pEntity->pev->absmax - pEntity->pev->origin;
	pEntity->pev->size = pEntity->pev->maxs - pEntity->pev->mins;
}

/*
===============

PHYSICS SAVE\RESTORE SYSTEM

===============
*/

// list of variables that needs to be saved	how it will be saved

/*
// actor description
NxMat34		globalPose;		pev->origin, pev->angles 
const NxBodyDesc*	body;			not needs
NxReal		density;			constant (???)
NxU32		flags;			m_iActorFlags;
NxActorGroup	group;			pev->groupinfo (???).
NxU32		contactReportFlags; 	not needs (already set by scene rules)
NxU16		forceFieldMaterial;		not needs (not used)
void*		userData;			not needs (automatically restored)
const char*	name;			not needs (automatically restored)
NxCompartment	*compartment;		not needs (not used)
NxActorDescType	type;			not needs (automatically restored) 
// body description
NxMat34		massLocalPose;		not needs (probably automatically recomputed on apply shape)
NxVec3		massSpaceInertia;		not needs (probably automatically recomputed on apply shape)
NxReal		mass;			not needs (probably automatically recomputed on apply shape)
NxVec3		linearVelocity;		pev->velocity
NxVec3		angularVelocity;		pev->avelocity
NxReal		wakeUpCounter;		not needs (use default)
NxReal		linearDamping;		not needs (use default)
NxReal		angularDamping;		not needs (use default)
NxReal		maxAngularVelocity; 	not needs (already set by scene rules)
NxReal		CCDMotionThreshold;		not needs (use default)
NxU32		flags;			m_iBodyFlags;
NxReal		sleepLinearVelocity; 	not needs (already set by scene rules)
NxReal		sleepAngularVelocity; 	not needs (already set by scene rules)
NxU32		solverIterationCount;	not needs (use default)
NxReal		sleepEnergyThreshold;	not needs (use default)
NxReal		sleepDamping;		not needs (use default)
NxReal		contactReportThreshold;	not needs (use default)
*/

/*
===============
SaveBody

collect all the info from generic actor
===============
*/
void CPhysicNovodex :: SaveBody( CBaseEntity *pEntity )
{
	NxActor *pActor = ActorFromEntity( pEntity );

	if( !pActor )
	{
		ALERT( at_warning, "SaveBody: physic entity %i missed actor!\n", pEntity->m_iActorType );
		return;
	}

	NxActorDesc actorDesc;
	NxBodyDesc bodyDesc;

	pActor->saveToDesc( actorDesc );
	pActor->saveBodyToDesc( bodyDesc );

	pEntity->m_iActorFlags = actorDesc.flags;
	pEntity->m_iBodyFlags = bodyDesc.flags;
	pEntity->m_usActorGroup = actorDesc.group;
	pEntity->m_flBodyMass = bodyDesc.mass;
	pEntity->m_fFreezed = pActor->isSleeping();

	if( pEntity->m_iActorType == ACTOR_DYNAMIC )
	{
		// update movement variables
		UpdateEntityPos( pEntity );
	}
}

/*
===============
RestoreBody

re-create shape, apply physic params
===============
*/						
void *CPhysicNovodex :: RestoreBody( CBaseEntity *pEntity )
{
	// physics not initialized?
	if( !m_pScene ) return NULL;

	NxConvexShapeDesc meshShapeDesc;
	NxTriangleMeshShapeDesc triMeshShapeDesc;
	NxBoxShapeDesc boxDesc;
	NxActorDesc ActorDesc;
	NxBodyDesc BodyDesc;

	switch( pEntity->m_iActorType )
	{
	case ACTOR_DYNAMIC:
		meshShapeDesc.meshData = ConvexMeshFromEntity( pEntity );
		if( !meshShapeDesc.meshData ) return NULL;
		ActorDesc.shapes.pushBack( &meshShapeDesc );
		break;
	case ACTOR_CHARACTER:
		boxDesc.dimensions = pEntity->pev->size * PADDING_FACTOR;
		ActorDesc.shapes.pushBack( &boxDesc );
		break;
	case ACTOR_KINEMATIC:
	case ACTOR_STATIC:
		triMeshShapeDesc.meshData = TriangleMeshFromEntity( pEntity );
		if( !triMeshShapeDesc.meshData ) return NULL;
		if( pEntity->pev->flags & FL_CONVEYOR )
			triMeshShapeDesc.materialIndex = 1;
		ActorDesc.shapes.pushBack( &triMeshShapeDesc );
		break;
	default:
		ALERT( at_error, "RestoreBody: invalid actor type %i\n", pEntity->m_iActorType );
		return NULL;
	}

	if( ActorDesc.shapes.size() <= 0 )
		return NULL; // failed to create shape

	NxMat34 pose;
	float mat[16];

	Vector angles = pEntity->GetAbsAngles();

	if( pEntity->m_iActorType == ACTOR_CHARACTER )
		angles = g_vecZero;	// no angles for NPC and client

	matrix4x4	m( pEntity->GetAbsOrigin(), angles, 1.0f );
	m.CopyToArray( mat );
	pose.setColumnMajor44( mat );

	// fill in actor description
	if( pEntity->m_iActorType != ACTOR_STATIC )
	{
		BodyDesc.flags = pEntity->m_iBodyFlags;
//		BodyDesc.mass = pEntity->m_flBodyMass;
		BodyDesc.solverIterationCount = SOLVER_ITERATION_COUNT;

		if( pEntity->m_iActorType != ACTOR_KINEMATIC )
		{
			BodyDesc.linearVelocity = pEntity->GetAbsVelocity(); 
			BodyDesc.angularVelocity = pEntity->GetAbsAvelocity();
		}

		ActorDesc.body = &BodyDesc;
          }
  
	ActorDesc.density = DENSITY_FACTOR;
	ActorDesc.userData = pEntity->edict();
	ActorDesc.flags = pEntity->m_iActorFlags;
	ActorDesc.group = pEntity->m_usActorGroup;
	ActorDesc.globalPose = pose; // saved pose

	if( pEntity->m_iActorType == ACTOR_KINEMATIC )
		m_ErrorStream.hideWarning( true );

	NxActor *pActor = m_pScene->createActor( ActorDesc );

	if( pEntity->m_iActorType == ACTOR_KINEMATIC )
		m_ErrorStream.hideWarning( false );

	if( !pActor )
	{
		ALERT( at_error, "RestoreBody: unbale to create actor with type (%i)\n", pEntity->m_iActorType );
		return NULL;
	}

	// apply specific actor params
	pActor->setName( pEntity->GetClassname( ));
	pEntity->m_pUserData = pActor;

	if( pEntity->m_fFreezed && pEntity->m_iActorType == ACTOR_DYNAMIC )
		pActor->putToSleep();

	return pActor;
}

void CPhysicNovodex :: SetAngles( CBaseEntity *pEntity, const Vector &angles )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	NxMat33 rot;
	matrix3x3	m( angles );
	rot.setRowMajor( (float *)m );
	pActor->setGlobalOrientation( rot );
}

void CPhysicNovodex :: SetOrigin( CBaseEntity *pEntity, const Vector &origin )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	pActor->setGlobalPosition( origin );
}

void CPhysicNovodex :: SetVelocity( CBaseEntity *pEntity, const Vector &velocity )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	pActor->setLinearVelocity( velocity );
}

void CPhysicNovodex :: SetAvelocity( CBaseEntity *pEntity, const Vector &velocity )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	pActor->setAngularVelocity( velocity );
}

void CPhysicNovodex :: MoveObject( CBaseEntity *pEntity, const Vector &finalPos )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	pActor->moveGlobalPosition( finalPos );
}

void CPhysicNovodex :: RotateObject( CBaseEntity *pEntity, const Vector &finalAngle )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	NxMat33 rot;
	matrix3x3	m( finalAngle );
	rot.setRowMajor( (float *)m );
	pActor->moveGlobalOrientation( rot );
}

void CPhysicNovodex :: SetLinearMomentum( CBaseEntity *pEntity, const Vector &velocity )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	pActor->setLinearMomentum( velocity );
}

void CPhysicNovodex :: AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	NxF32 coeff = (1000.0f / pActor->getMass()) * factor;

	// prevent to apply too much impulse
	if( pActor->getMass() < 8.0f )
	{
		coeff *= 0.0001f;
	}

	pActor->addForceAtPos( NxVec3(impulse * coeff), (NxVec3)position, NX_IMPULSE );
}

void CPhysicNovodex :: AddForce( CBaseEntity *pEntity, const Vector &force )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) return;

	pActor->addForce( force, NX_FORCE );
}

void *CPhysicNovodex :: CreateForceField( CBaseEntity *pEntity, const Vector &force )
{
	// create a forcefield kernel
	NxForceFieldLinearKernelDesc	linearKernelDesc;

	linearKernelDesc.constant = NxVec3( 1, 0, 0.2 );//force;
//	linearKernelDesc.velocityTarget = NxVec3( 0.5, 0, 0.2 );
//	linearKernelDesc.falloffLinear = NxVec3( 100, 0, 0.2 );
	NxForceFieldLinearKernel *pLinearKernel;
	pLinearKernel = m_pScene->createForceFieldLinearKernel( linearKernelDesc );

	// The forcefield descriptor
	NxForceFieldDesc fieldDesc;
	fieldDesc.kernel = pLinearKernel;
	fieldDesc.rigidBodyType = NX_FF_TYPE_OTHER;

	// A box forcefield shape descriptor
	NxBoxForceFieldShapeDesc box;
	box.dimensions = (pEntity->pev->size - Vector( 2, 2, 2 )) * 0.5f;
	box.pose.t = pEntity->Center() + Vector( 0, 0, pEntity->pev->size.z - 2 );

	fieldDesc.includeGroupShapes.push_back( &box );
	return m_pScene->createForceField( fieldDesc );
}

int CPhysicNovodex :: FLoadTree( char *szMapName )
{
	if( !szMapName || !*szMapName || !m_pPhysics )
		return 0;

	if( m_fLoaded )
	{
		if( !Q_stricmp( szMapName, m_szMapName ))
		{
			// stay on same map - no reload
			return 1;
		}

		if( m_pSceneActor )
			m_pScene->releaseActor( *m_pSceneActor );
		m_pSceneActor = NULL;

		if( m_pSceneMesh )
			m_pPhysics->releaseTriangleMesh( *m_pSceneMesh );
		m_pSceneMesh = NULL;

		m_fLoaded = FALSE; // trying to load new collision tree 
	}

	// save off mapname
	strcpy ( m_szMapName, szMapName );

	char szHullFilename[MAX_PATH];

	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s.bin", szMapName );
	m_pSceneMesh = m_pPhysics->createTriangleMesh( UserStream( szHullFilename, true ));
	m_fWorldChanged = FALSE;

	return (m_pSceneMesh != NULL) ? TRUE : FALSE;
}

int CPhysicNovodex :: CheckBINFile( char *szMapName )
{
	if( !szMapName || !*szMapName || !m_pPhysics )
		return FALSE;

	char szBspFilename[MAX_PATH];
	char szHullFilename[MAX_PATH];

	Q_snprintf( szBspFilename, sizeof( szBspFilename ), "maps/%s.bsp", szMapName );
	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s.bin", szMapName );

	BOOL retValue = TRUE;

	int iCompare;
	if ( COMPARE_FILE_TIME( szBspFilename, szHullFilename, &iCompare ))
	{
		if( iCompare > 0 )
		{
			// BSP file is newer.
			ALERT ( at_console, ".BIN file will be updated\n\n" );
			retValue = FALSE;
		}
	}
	else
	{
		retValue = FALSE;
	}

	return retValue;
}

void CPhysicNovodex :: HullNameForModel( const char *model, char *hullfile, size_t size )
{
	if( !model || !*model || !hullfile )
		return;

	size_t baseFolderLength = Q_strlen( "models/" );

	char szModelBasename[MAX_PATH];
	char szModelFilepath[MAX_PATH];

	COM_ExtractFilePath( model + baseFolderLength, szModelFilepath );
	COM_FileBase( model, szModelBasename );

	if( szModelFilepath[0] )
		Q_snprintf( hullfile, size, "cache/%s/%s.hull", szModelFilepath, szModelBasename );
	else
		Q_snprintf( hullfile, size, "cache/%s.hull", szModelBasename );
}

void CPhysicNovodex :: MeshNameForModel( const char *model, char *hullfile, size_t size )
{
	if( !model || !*model || !hullfile )
		return;

	size_t baseFolderLength = Q_strlen( "models/" );

	char szModelBasename[MAX_PATH];
	char szModelFilepath[MAX_PATH];

	COM_ExtractFilePath( model + baseFolderLength, szModelFilepath );
	COM_FileBase( model, szModelBasename );

	if( szModelFilepath[0] )
		Q_snprintf( hullfile, size, "cache/%s/%s.mesh", szModelFilepath, szModelBasename );
	else
		Q_snprintf( hullfile, size, "cache/%s.mesh", szModelBasename );
}

//-----------------------------------------------------------------------------
// hulls - convex hulls cooked with NxCookingConvexMesh routine and stored into cache\*.hull
// meshes - triangle meshes cooked with NxCookingTriangleMesh routine and stored into cache\*.mesh
//-----------------------------------------------------------------------------
int CPhysicNovodex :: CheckFileTimes( const char *szFile1, const char *szFile2 )
{
	if( !szFile1 || !*szFile1 || !szFile2 || !*szFile2 )
		return FALSE;

	BOOL retValue = TRUE;

	int iCompare;
	if ( COMPARE_FILE_TIME( (char *)szFile1, (char *)szFile2, &iCompare ))
	{
		if( iCompare > 0 )
		{
			// MDL file is newer.
			retValue = FALSE;
		}
	}
	else
	{
		retValue = FALSE;
	}

	return retValue;
}

//-----------------------------------------------------------------------------
// assume m_pWorldModel is valid
//-----------------------------------------------------------------------------
int CPhysicNovodex :: ConvertEdgeToIndex( model_t *model, int edge )
{
	int e = model->surfedges[edge];
	return (e > 0) ? model->edges[e].v[0] : model->edges[-e].v[1];
}

int CPhysicNovodex :: BuildCollisionTree( char *szMapName )
{
	if( !m_pPhysics )
		return FALSE;
	 
	// get a world struct
	if(( m_pWorldModel = (model_t *)MODEL_HANDLE( 1 )) == NULL )
	{
		ALERT( at_error, "BuildCollisionTree: unbale to fetch world pointer %s\n", szMapName );
		return FALSE;
	}

	if( m_pSceneActor )
		m_pScene->releaseActor( *m_pSceneActor );
	m_pSceneActor = NULL;

	if( m_pSceneMesh )
		m_pPhysics->releaseTriangleMesh( *m_pSceneMesh );
	m_pSceneMesh = NULL;

	// save off mapname
	Q_strcpy( m_szMapName, szMapName );

	ALERT( at_console, "Tree Collision out of Date. Rebuilding...\n" );

	// convert world from polygons to tri-list
	int i, numElems = 0, totalElems = 0;
	msurface_t *psurf;

	// compute vertexes count
	for( i = 0; i < m_pWorldModel->nummodelsurfaces; i++ )
	{
		psurf = &m_pWorldModel->surfaces[m_pWorldModel->firstmodelsurface + i];

		if( psurf->flags & ( SURF_DRAWTURB|SURF_DRAWSKY ))
			continue;

		totalElems += (psurf->numedges - 2);
	}

	NxU32 *indices = new NxU32[totalElems * 3];

	for( i = 0; i < m_pWorldModel->nummodelsurfaces; i++ )
	{
		psurf = &m_pWorldModel->surfaces[m_pWorldModel->firstmodelsurface + i];
		int k = psurf->firstedge;

		// don't create collision for water
		if( psurf->flags & ( SURF_DRAWTURB|SURF_DRAWSKY ))
			continue;

		for( int j = 0; j < psurf->numedges - 2; j++ )
		{
			indices[numElems*3+0] = ConvertEdgeToIndex( m_pWorldModel, k );
			indices[numElems*3+1] = ConvertEdgeToIndex( m_pWorldModel, k + j + 2 );
			indices[numElems*3+2] = ConvertEdgeToIndex( m_pWorldModel, k + j + 1 );
			numElems++;
		}
	}

	NX_ASSERT( totalElems == numElems );

	// build physical model
	NxTriangleMeshDesc levelDesc;
	levelDesc.pointStrideBytes = sizeof( mvertex_t );
	levelDesc.triangleStrideBytes	= 3 * sizeof( NxU32 );
	levelDesc.points = (const NxPoint*)&(m_pWorldModel->vertexes[0].position);
	levelDesc.numVertices = m_pWorldModel->numvertexes;
	levelDesc.numTriangles = numElems;
	levelDesc.triangles = indices;
	levelDesc.flags = 0;

	char szHullFilename[MAX_PATH];
	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s.bin", szMapName );

	if( m_pCooking )
	{
		m_pCooking->NxInitCooking();
		bool status = m_pCooking->NxCookTriangleMesh( levelDesc, UserStream( szHullFilename, false ));
          }

	delete [] indices;

	m_pSceneMesh = m_pPhysics->createTriangleMesh( UserStream( szHullFilename, true ));
	m_fWorldChanged = TRUE;

	return (m_pSceneMesh != NULL) ? TRUE : FALSE;
}

void CPhysicNovodex :: SetupWorld( void )
{
	if( m_pSceneActor )
		return;	// already loaded

	if( !m_pSceneMesh )
	{
		ALERT( at_error, "*collision tree not ready!\n" );
		return;
	}

	// get a world struct
	if(( m_pWorldModel = (model_t *)MODEL_HANDLE( 1 )) == NULL )
	{
		ALERT( at_error, "SetupWorld: unbale to fetch world pointer %s\n", m_szMapName );
		return;
	}

	NxTriangleMeshShapeDesc levelShapeDesc;
	NxActorDesc ActorDesc;

	ActorDesc.userData = g_pWorld->edict();
	levelShapeDesc.meshData = m_pSceneMesh;
	ActorDesc.shapes.pushBack( &levelShapeDesc );
	m_pSceneActor = m_pScene->createActor( ActorDesc );
	m_fLoaded = true;

	// update the world bounds for NX_BP_TYPE_SAP_MULTI
	NxShape *pSceneShape = m_pSceneActor->getShapes()[0];
	pSceneShape->getWorldBounds( worldBounds );
}
	
void CPhysicNovodex :: DebugDraw( void )
{
	if( !m_pPhysics || !m_pScene )
		return;

	gDebugRenderer.renderData( *m_pScene->getDebugRenderable( ));
}

/*
===============
P_SpeedsMessage
===============
*/
bool CPhysicNovodex :: P_SpeedsMessage( char *out, size_t size )
{
	if( !p_speeds || p_speeds->value <= 0.0f )
		return false;

	if( !out || !size ) return false;
	Q_strncpy( out, p_speeds_msg, size );

	return true;
}

/*
================
SCR_RSpeeds
================
*/
void CPhysicNovodex :: DrawPSpeeds( void )
{
	char	msg[1024];
	int	iScrWidth = CVAR_GET_FLOAT( "width" );

	if( P_SpeedsMessage( msg, sizeof( msg )))
	{
		int	x, y, height;
		char	*p, *start, *end;

		x = iScrWidth - 320;
		y = 128;

		DrawConsoleStringLen( NULL, NULL, &height );
		DrawSetTextColor( 1.0f, 1.0f, 1.0f );

		p = start = msg;
		do
		{
			end = Q_strchr( p, '\n' );
			if( end ) msg[end-start] = '\0';

			DrawConsoleString( x, y, p );
			y += height;

			if( end )
				p = end + 1;
			else
				break;
		} while( 1 );
	}
}

void CPhysicNovodex :: FreeAllBodies( void )
{
	if( !m_pScene ) return;

	// throw all bodies
	while( m_pScene->getNbActors())
	{
		NxActor *actor = m_pScene->getActors()[0];
		m_pScene->releaseActor( *actor );
	}
	m_pSceneActor = NULL;
}

void CPhysicNovodex :: TeleportCharacter( CBaseEntity *pEntity )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor || pActor->getNbShapes() <= 0 )
		return;

	if( m_fNeedFetchResults )
		return;

	NxBoxShape *pShape = (NxBoxShape *)pActor->getShapes()[0];
	Vector vecOffset = (pEntity->IsMonster()) ? Vector( 0, 0, pEntity->pev->maxs.z / 2.0f ) : g_vecZero;

	pShape->setDimensions( pEntity->pev->size * PADDING_FACTOR );
	pActor->setGlobalPosition( pEntity->GetAbsOrigin() + vecOffset );
}

void CPhysicNovodex :: TeleportActor( CBaseEntity *pEntity )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor || pActor->getNbShapes() <= 0 )
		return;

	NxMat34 pose;
	float mat[16];

	matrix4x4	m( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles( ), 1.0f );
	m.CopyToArray( mat );

	// complex move for kinematic entities
	pose.setColumnMajor44( mat );
	pActor->setGlobalPose( pose );
}

void CPhysicNovodex :: MoveCharacter( CBaseEntity *pEntity )
{
	if( !pEntity || pEntity->m_vecOldPosition == pEntity->pev->origin )
		return;

	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor || pActor->getNbShapes() <= 0 )
		return;

	if( m_fNeedFetchResults )
		return;

	NxBoxShape *pShape = (NxBoxShape *)pActor->getShapes()[0];

	// if were in NOCLIP or FLY (ladder climbing) mode - disable collisions
	if( pEntity->pev->movetype != MOVETYPE_WALK )
		pActor->raiseActorFlag( NX_AF_DISABLE_COLLISION );
	else pActor->clearActorFlag( NX_AF_DISABLE_COLLISION );

	Vector vecOffset = (pEntity->IsMonster()) ? Vector( 0, 0, pEntity->pev->maxs.z / 2.0f ) : g_vecZero;

	pShape->setDimensions( pEntity->pev->size * PADDING_FACTOR );
	pActor->moveGlobalPosition( pEntity->GetAbsOrigin() + vecOffset );
	pEntity->m_vecOldPosition = pEntity->GetAbsOrigin(); // update old position
}

void CPhysicNovodex :: MoveKinematic( CBaseEntity *pEntity )
{
	if( !pEntity || ( pEntity->pev->movetype != MOVETYPE_PUSH && pEntity->pev->movetype != MOVETYPE_PUSHSTEP ))
		return;	// probably not a mover

	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor || pActor->getNbShapes() <= 0 )
		return;

	if( m_fNeedFetchResults )
		return;

	if( pEntity->pev->solid == SOLID_NOT || pEntity->pev->solid == SOLID_TRIGGER )
		pActor->raiseActorFlag( NX_AF_DISABLE_COLLISION );
	else pActor->clearActorFlag( NX_AF_DISABLE_COLLISION );

	NxMat34 pose;
	float mat[16];

	matrix4x4	m( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles( ), 1.0f );
	m.CopyToArray( mat );

	// complex move for kinematic entities
	pose.setColumnMajor44( mat );
	pActor->moveGlobalPose( pose );
}

void CPhysicNovodex :: EnableCollision( CBaseEntity *pEntity, int fEnable )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor || pActor->getNbShapes() <= 0 )
		return;

	if( fEnable )
	{
		pActor->clearActorFlag( NX_AF_DISABLE_COLLISION );
		pActor->raiseBodyFlag( NX_BF_VISUALIZATION );
	}
	else
	{
		pActor->raiseActorFlag( NX_AF_DISABLE_COLLISION );
		pActor->clearBodyFlag( NX_BF_VISUALIZATION );
	}
}

void CPhysicNovodex :: MakeKinematic( CBaseEntity *pEntity, int fEnable )
{
	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor || pActor->getNbShapes() <= 0 )
		return;

	if( fEnable )
		pActor->raiseBodyFlag( NX_BF_KINEMATIC );
	else
		pActor->clearBodyFlag( NX_BF_KINEMATIC );
}

void CPhysicNovodex :: SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, trace_t *tr )
{
	NxActor *pActor = ActorFromEntity( pTouch );

	if( !pActor || pActor->getNbShapes() <= 0 || pActor->readActorFlag( NX_AF_DISABLE_COLLISION ))
	{
		// bad actor?
		tr->allsolid = false;
		return;
	}

	Vector trace_mins, trace_maxs;
	UTIL_MoveBounds( start, mins, maxs, end, trace_mins, trace_maxs );

	// NOTE: pmove code completely ignore a bounds checking. So we need to do it here
	if( !BoundsIntersect( trace_mins, trace_maxs, pTouch->pev->absmin, pTouch->pev->absmax ))
	{
		tr->allsolid = false;
		return;
	}

	vec3_t scale = pTouch->pev->startpos.Length() < 0.001f ? vec3_t(1.0f) : pTouch->pev->startpos;
	matrix4x4 worldToLocalMat = matrix4x4(pTouch->pev->origin, pTouch->pev->angles, scale).InvertFull();
	model_t *mod = (model_t *)MODEL_HANDLE(pTouch->pev->modelindex);
	NxShape *pShape = pActor->getShapes()[0];
	NxShapeType shapeType = pShape->getType();
	clipfile::GeometryType geomType = ShapeTypeToGeomType(shapeType);

	if (geomType == clipfile::GeometryType::Unknown)
	{
		// unsupported mesh type, so skip them
		tr->allsolid = false;
		return;
	}

	auto &meshDescFactory = CMeshDescFactory::Instance();
	CMeshDesc &cookedMesh = meshDescFactory.CreateObject(pTouch->pev->modelindex, pTouch->pev->body, pTouch->pev->skin, geomType);
	if (!cookedMesh.GetMesh())
	{
		const bool presentInCache = cookedMesh.PresentInCache();
		if (!presentInCache) 
		{
			// update cache or build from scratch
			const NxU32 *indices;
			const NxVec3 *verts;
			Vector triangle[3];
			NxU32 NbTris;

			if( shapeType == NX_SHAPE_CONVEX )
			{
				NxConvexShape *pConvexShape = (NxConvexShape *)pShape;
				NxConvexMesh& cm = pConvexShape->getConvexMesh();

				NbTris = cm.getCount( 0, NX_ARRAY_TRIANGLES );
				indices = (const NxU32 *)cm.getBase( 0, NX_ARRAY_TRIANGLES );
				verts = (const NxVec3 *)cm.getBase( 0, NX_ARRAY_VERTICES );
			}
			else if( shapeType == NX_SHAPE_MESH )
			{
				NxTriangleMeshShape *pTriangleMeshShape = (NxTriangleMeshShape *)pShape;
				NxTriangleMesh& trm = pTriangleMeshShape->getTriangleMesh();

				NbTris = trm.getCount( 0, NX_ARRAY_TRIANGLES );
				indices = (const NxU32 *)trm.getBase( 0, NX_ARRAY_TRIANGLES );
				verts = (const NxVec3 *)trm.getBase( 0, NX_ARRAY_VERTICES );
			}

			if( shapeType == NX_SHAPE_BOX )
			{
				NxVec3	points[8];
				NxVec3	ext, cnt;
				NxBounds3	bounds;
				NxBox	obb;

				// each box shape contain 12 triangles
				cookedMesh.InitMeshBuild(pActor->getNbShapes() * 12);

				for( uint i = 0; i < pActor->getNbShapes(); i++ )
				{
					NxBoxShape *pBoxShape = (NxBoxShape *)pActor->getShapes()[i];
					NxMat33 absRot = pBoxShape->getGlobalOrientation();
					NxVec3 absPos = pBoxShape->getGlobalPosition();

					// don't use pBoxShape->getWorldAABB it's caused to broke suspension and deadlocks !!!
					pBoxShape->getWorldBounds( bounds );
					bounds.getExtents( ext );
					bounds.getCenter( cnt );
					obb = NxBox( cnt, ext, absRot );

					indices = (const NxU32 *)m_pUtils->NxGetBoxTriangles();
					m_pUtils->NxComputeBoxPoints( obb, points );
					verts = (const NxVec3 *)points;

					for( int j = 0; j < 12; j++ )
					{
						NxU32 i0 = *indices++;
						NxU32 i1 = *indices++;
						NxU32 i2 = *indices++;
						triangle[0] = verts[i0];
						triangle[1] = verts[i1];
						triangle[2] = verts[i2];

						// transform from world to model space
						triangle[0] = worldToLocalMat.VectorTransform(triangle[0]);
						triangle[1] = worldToLocalMat.VectorTransform(triangle[1]);
						triangle[2] = worldToLocalMat.VectorTransform(triangle[2]);
						cookedMesh.AddMeshTrinagle( triangle );
					}
				}
			}
			else
			{
				cookedMesh.InitMeshBuild(NbTris);
				while( NbTris-- )
				{
					NxU32 i0 = *indices++;
					NxU32 i1 = *indices++;
					NxU32 i2 = *indices++;

					triangle[0] = verts[i0];
					triangle[1] = verts[i1];
					triangle[2] = verts[i2];
					cookedMesh.AddMeshTrinagle( triangle );
				}
			}
		}
		else {
			cookedMesh.LoadFromCacheFile();
		}

		if (!cookedMesh.FinishMeshBuild())
		{
			ALERT(at_error, "failed to build cooked mesh from %s\n", pTouch->GetModel());
			tr->allsolid = false;
			return;
		}
		else 
		{
			if (!presentInCache) {
				cookedMesh.SaveToCacheFile();
			}
			cookedMesh.FreeMeshBuild();
		}
	}

	mmesh_t *pMesh;
	areanode_t *pHeadNode;
	if (mod->type == mod_studio && FBitSet(gpGlobals->trace_flags, FTRACE_MATERIAL_TRACE))
	{
		CMeshDesc &originalMesh = meshDescFactory.CreateObject(pTouch->pev->modelindex, pTouch->pev->body, pTouch->pev->skin, clipfile::GeometryType::Original);
		if (!originalMesh.GetMesh())
		{
			originalMesh.StudioConstructMesh();
			if (!originalMesh.GetMesh()) {
				ALERT(at_error, "failed to build original mesh from %s\n", pTouch->GetModel());
			}
		}

		pMesh = originalMesh.GetMesh();
		pHeadNode = originalMesh.GetHeadNode();
	} 
	else
	{
		pMesh = cookedMesh.GetMesh();
		pHeadNode = cookedMesh.GetHeadNode();
	}

	TraceMesh trm;
	trm.SetTraceMesh(pMesh, pHeadNode, pTouch->pev->modelindex);
	trm.SetMeshOrientation(pTouch->pev->origin, pTouch->pev->angles, scale); 
	trm.SetupTrace(start, mins, maxs, end, tr);

	if (trm.DoTrace())
	{
		if( tr->fraction < 1.0f || tr->startsolid )
			tr->ent = pTouch->edict();
		tr->materialHash = COM_GetMaterialHash(trm.GetLastHitSurface());
	}
}

void CPhysicNovodex :: SweepEntity( CBaseEntity *pEntity, const Vector &start, const Vector &end, TraceResult *tr )
{
	// make trace default
	memset( tr, 0, sizeof( *tr ));
	tr->flFraction = 1.0f;
	tr->vecEndPos = end;

	NxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor || pActor->getNbShapes() <= 0 || pEntity->pev->solid == SOLID_NOT )
		return; // only dynamic solid objects can be traced

	NxBox testBox;

	// setup trace box
	testBox.center = pEntity->Center();
	testBox.rot = pActor->getGlobalOrientation();
	testBox.extents = (pEntity->pev->size * 0.5f);

	// test for stuck entity into another
	if( start == end )
	{
		// update cache or build from scratch
		NxShape *pShape = pActor->getShapes()[0];
		int shapeType = pShape->getType();
		Vector triangle[3], dirs[3];
		const NxU32 *indices;
		const NxVec3 *verts;
		NxU32 NbTris;

		if( shapeType == NX_SHAPE_CONVEX )
		{
			NxConvexShape *pConvexShape = (NxConvexShape *)pShape;
			NxConvexMesh& cm = pConvexShape->getConvexMesh();

			NbTris = cm.getCount( 0, NX_ARRAY_TRIANGLES );
			indices = (const NxU32 *)cm.getBase( 0, NX_ARRAY_TRIANGLES );
			verts = (const NxVec3 *)cm.getBase( 0, NX_ARRAY_VERTICES );
		}
		else if( shapeType == NX_SHAPE_MESH )
		{
			NxTriangleMeshShape *pTriangleMeshShape = (NxTriangleMeshShape *)pShape;
			NxTriangleMesh& trm = pTriangleMeshShape->getTriangleMesh();

			NbTris = trm.getCount( 0, NX_ARRAY_TRIANGLES );
			indices = (const NxU32 *)trm.getBase( 0, NX_ARRAY_TRIANGLES );
			verts = (const NxVec3 *)trm.getBase( 0, NX_ARRAY_VERTICES );
		}
		else if( shapeType != NX_SHAPE_BOX )
		{
			// unsupported mesh type, so skip them
			return;
		}

		if( shapeType == NX_SHAPE_BOX )
		{
			NxVec3	points[8];
			NxVec3	ext, cnt;
			NxBounds3	bounds;
			NxBox	obb;

			for( uint i = 0; i < pActor->getNbShapes(); i++ )
			{
				NxBoxShape *pBoxShape = (NxBoxShape *)pActor->getShapes()[i];
				NxMat33 absRot = pBoxShape->getGlobalOrientation();
				NxVec3 absPos = pBoxShape->getGlobalPosition();

				// don't use pBoxShape->getWorldAABB it's caused to broke suspension and deadlocks !!!
				pBoxShape->getWorldBounds( bounds );
				bounds.getExtents( ext );
				bounds.getCenter( cnt );
				obb = NxBox( cnt, ext, absRot );

				indices = (const NxU32 *)m_pUtils->NxGetBoxTriangles();
				m_pUtils->NxComputeBoxPoints( obb, points );
				verts = (const NxVec3 *)points;

				for( int j = 0; j < 12; j++ )
				{
					NxU32 i0 = *indices++;
					NxU32 i1 = *indices++;
					NxU32 i2 = *indices++;
					triangle[0] = verts[i0];
					triangle[1] = verts[i1];
					triangle[2] = verts[i2];

					for( int k = 0; k < 3; k++ )
					{
						dirs[k] = absPos - triangle[k];
						triangle[k] += dirs[k] * -2.0f;

						UTIL_TraceLine( triangle[k], triangle[k], ignore_monsters, pEntity->edict(), tr );
						if( tr->fStartSolid ) return;	// one of points in solid
					}
				}
			}
		}
		else
		{
			NxMat33 absRot = pShape->getGlobalOrientation();
			NxVec3 absPos = pShape->getGlobalPosition();

			// NOTE: we compute triangles in abs coords because player AABB
			// can't be transformed as done for not axial cases
			// FIXME: store all meshes as local and use capsule instead of bbox
			while( NbTris-- )
			{
				NxU32 i0 = *indices++;
				NxU32 i1 = *indices++;
				NxU32 i2 = *indices++;
				NxVec3 v0 = verts[i0];
				NxVec3 v1 = verts[i1];
				NxVec3 v2 = verts[i2];

				absRot.multiply( v0, v0 );
				absRot.multiply( v1, v1 );
				absRot.multiply( v2, v2 );
				triangle[0] = v0 + absPos;
				triangle[1] = v1 + absPos;
				triangle[2] = v2 + absPos;

				for( int i = 0; i < 3; i++ )
				{
					dirs[i] = absPos - triangle[i];
					triangle[i] += dirs[i] * -2.0f;

					UTIL_TraceLine( triangle[i], triangle[i], ignore_monsters, pEntity->edict(), tr );
					if( tr->fStartSolid ) return;	// one of points in solid
				}
			}
		}
		return;
	}

	// compute motion
	Vector vecDir = end - start;
	float flLength = vecDir.Length();
	vecDir = vecDir.Normalize();
	testBox.extents = (pEntity->pev->size * 0.5f);
	NxSweepQueryHit result;

	// make a linear sweep through the world
	pActor->raiseActorFlag( NX_AF_DISABLE_COLLISION );
	int numHits = m_pScene->linearOBBSweep( testBox, vecDir * flLength, NX_SF_STATICS|NX_SF_DYNAMICS, NULL, 1, &result, NULL );
	pActor->clearActorFlag( NX_AF_DISABLE_COLLISION );
	if( !numHits || !result.hitShape || result.t > flLength )
		return; // no intersection

	// compute fraction
	tr->flFraction = (result.t / flLength);
	tr->flFraction = bound( 0.0f, tr->flFraction, 1.0f );
	VectorLerp( start, tr->flFraction, end, tr->vecEndPos );

	CBaseEntity *pHit = EntityFromActor( &result.hitShape->getActor( ));
	if( pHit ) tr->pHit = pHit->edict();

	tr->vecPlaneNormal = result.normal;
	tr->flPlaneDist = DotProduct( tr->vecEndPos, tr->vecPlaneNormal );
	float flDot = DotProduct( vecDir, tr->vecPlaneNormal );
	float moveDot = Q_round( flDot, 0.1f );

	// FIXME: this is incorrect. Find a better method?
	if(( tr->flFraction < 0.1f ) && ( moveDot < 0.0f ))
		tr->fAllSolid = true;
}

#endif//USE_PHYSICS_ENGINE
