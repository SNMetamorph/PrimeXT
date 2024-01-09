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

#ifdef USE_PHYSICS_ENGINE
#include "novodex.h"
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
#include "NxErrorStream.h"
#include "PxMat33.h"
#include "PxMat44.h"
#include "contact_report.h"
#include "debug_renderer.h"
#include "decomposed_shape.h"
#include "meshdesc_factory.h"
#include "crclib.h"
#include <vector>
#include <thread>
#include <sstream>

#if defined (HAS_PHYSIC_VEHICLE)
#include "NxVehicle.h"
#include "vehicles/NxParser.h"
#endif

constexpr float k_PaddingFactor = 0.49f;
constexpr float k_DensityFactor = 2.00f;
constexpr uint32_t k_SolverIterationCount = 4;
constexpr double k_SimulationStepSize = 1.0 / 100.0;

using namespace physx;

CPhysicNovodex	NovodexPhysic;
IPhysicLayer	*WorldPhysic = &NovodexPhysic;

CPhysicNovodex::CPhysicNovodex()
{
	m_pPhysics = nullptr;
	m_pFoundation = nullptr;
	m_pDispatcher = nullptr;
	m_pScene = nullptr;	
	m_pWorldModel = nullptr;
	m_pSceneMesh = nullptr;
	m_pSceneActor = nullptr;
	m_pDefaultMaterial = nullptr;
	m_pConveyorMaterial = nullptr;
	m_pCooking = nullptr;
	m_pVisualDebugger = nullptr;
	
	m_fLoaded = false;
	m_fDisableWarning = false;
	m_fWorldChanged = false;
	m_flAccumulator = 0.0;

	m_szMapName[0] = '\0';
	p_speeds_msg[0] = '\0';
}

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

	m_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);
	if (!m_pFoundation)
	{
		ALERT(at_error, "InitPhysic: failed to create foundation\n");
		GameInitNullPhysics();
		return;
	}

	PxTolerancesScale scale;
	scale.length = 39.3701;   // typical length of an object
	scale.speed = 800.0;   // typical speed of an object, gravity*1s is a reasonable choice
	
	m_pVisualDebugger = PxCreatePvd(*m_pFoundation);
	if (DebugEnabled()) {
		PxPvdTransport *transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 1000);
		m_pVisualDebugger->connect(*transport, PxPvdInstrumentationFlag::eALL);
	}

	m_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_pFoundation, scale, false, m_pVisualDebugger);
	m_pDispatcher = PxDefaultCpuDispatcherCreate(std::thread::hardware_concurrency());
	if (!m_pPhysics)
	{
		ALERT( at_error, "InitPhysic: failed to initalize physics engine\n" );
		GameInitNullPhysics();
		return;
	}

	m_pCooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_pFoundation, PxCookingParams(scale));
	if (!m_pCooking)
	{
		ALERT( at_warning, "InitPhysic: failed to initalize cooking library\n" );
	}

	if (!PxInitExtensions(*m_pPhysics, m_pVisualDebugger)) {
		ALERT( at_warning, "InitPhysic: failed to initalize extensions\n" );
	}

	// create a scene
	PxSceneDesc sceneDesc(scale);
	sceneDesc.simulationEventCallback = &ContactReport::getInstance();
	sceneDesc.gravity = PxVec3(0.0f, 0.0f, -800.0f);
	sceneDesc.flags = PxSceneFlag::eENABLE_CCD;
	sceneDesc.cpuDispatcher = m_pDispatcher;
	sceneDesc.filterShader = [](
		PxFilterObjectAttributes attributes0, PxFilterData filterData0,
		PxFilterObjectAttributes attributes1, PxFilterData filterData1,
		PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize) -> PxFilterFlags {
			pairFlags = PxPairFlag::eCONTACT_DEFAULT
					  | PxPairFlag::eDETECT_CCD_CONTACT
					  | PxPairFlag::eNOTIFY_TOUCH_CCD
					  |	PxPairFlag::eNOTIFY_TOUCH_FOUND
					  |	PxPairFlag::eNOTIFY_TOUCH_PERSISTS
					  | PxPairFlag::eNOTIFY_CONTACT_POINTS
					  | PxPairFlag::eCONTACT_EVENT_POSE;
			return PxFilterFlag::eDEFAULT;
	};

	m_worldBounds.minimum = PxVec3(-32768, -32768, -32768);
	m_worldBounds.maximum = PxVec3(32768, 32768, 32768);
	sceneDesc.sanityBounds = m_worldBounds;
	m_pScene = m_pPhysics->createScene(sceneDesc);

	if (DebugEnabled())
	{
		PxPvdSceneClient *pvdClient = m_pScene->getScenePvdClient();
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 1.0f); // disable on release build because performance impact
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 1.0f);
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_POINT, 1.0f);
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eCONTACT_NORMAL, 1.0f);
		m_pScene->setVisualizationParameter(PxVisualizationParameter::eBODY_AXES, 1.0f);

		if (pvdClient)
		{
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
			pvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
		}
	}

	m_pDefaultMaterial = m_pPhysics->createMaterial(0.5f, 0.5f, 0.0f);
	m_pConveyorMaterial = m_pPhysics->createMaterial(1.0f, 1.0f, 0.0f);
}

void CPhysicNovodex :: FreePhysic( void )
{
	if (m_pScene) m_pScene->release();
	if (m_pCooking) m_pCooking->release();
	if (m_pDispatcher) m_pDispatcher->release();
	if (m_pPhysics) m_pPhysics->release();
	if (m_pVisualDebugger) 
	{
		PxPvdTransport *transport = m_pVisualDebugger->getTransport();
		m_pVisualDebugger->release();
		if (transport) {
			transport->release();
		}
	}

	PxCloseExtensions();
	if (m_pFoundation) m_pFoundation->release();

	m_pScene = nullptr;
	m_pCooking = nullptr;
	m_pPhysics = nullptr;
	m_pVisualDebugger = nullptr;
	m_pFoundation = nullptr;
}

void CPhysicNovodex :: Update( float flTimeDelta )
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

		PxVec3 gravity = m_pScene->getGravity();
		if( gravity.z != -( g_psv_gravity->value ))
		{
			ALERT( at_aiconsole, "gravity changed from %g to %g\n", gravity.z, -(g_psv_gravity->value));
			gravity.z = -(g_psv_gravity->value);
			m_pScene->setGravity( gravity );
		}
	}

	m_flAccumulator += flTimeDelta;
	while (m_flAccumulator > k_SimulationStepSize)
	{
		m_flAccumulator -= k_SimulationStepSize;
		m_pScene->simulate(k_SimulationStepSize);
		m_pScene->fetchResults(true);
	}
}

void CPhysicNovodex :: EndFrame( void )
{
	if( !m_pScene || GET_SERVER_STATE() != SERVER_ACTIVE )
		return;

	// fill physics stats
	if( !p_speeds || p_speeds->value <= 0.0f )
		return;

	PxSimulationStatistics stats;
	m_pScene->getSimulationStatistics( stats );

	switch( (int)p_speeds->value )
	{
	case 1:
		Q_snprintf(p_speeds_msg, sizeof(p_speeds_msg), 
			"%3i active dynamic bodies\n%3i static bodies\n%3i dynamic bodies",
			stats.nbActiveDynamicBodies, 
			stats.nbStaticBodies, 
			stats.nbDynamicBodies
		);
		break;		
	}
}

void CPhysicNovodex :: RemoveBody( edict_t *pEdict )
{
	if( !m_pScene || !pEdict || pEdict->free )
		return; // scene purge all the objects automatically

	CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
	PxActor *pActor = ActorFromEntity( pEntity );

	if( pActor ) 
		pActor->release();
	pEntity->m_pUserData = NULL;
}

PxConvexMesh *CPhysicNovodex :: ConvexMeshFromBmodel( entvars_t *pev, int modelindex )
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
	PxConvexMesh *pHull = NULL;
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

	PxConvexMeshDesc meshDesc;
	meshDesc.points.data = verts;
	meshDesc.points.stride = sizeof(Vector);
	meshDesc.points.count = numVerts;
	meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	MemoryWriteBuffer outputBuffer;
	bool status = m_pCooking->cookConvexMesh(meshDesc, outputBuffer);
	delete [] verts;

	if( !status )
	{
		ALERT( at_error, "failed to create convex mesh from %s\n", bmodel->name );
		return NULL;
	}

	MemoryReadBuffer inputBuffer(outputBuffer.getData(), outputBuffer.getSize());
	pHull = m_pPhysics->createConvexMesh(inputBuffer);
	if( !pHull ) ALERT( at_error, "failed to create convex mesh from %s\n", bmodel->name );

	return pHull;
}

PxTriangleMesh *CPhysicNovodex :: TriangleMeshFromBmodel( entvars_t *pev, int modelindex )
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
	PxTriangleMesh *pMesh = NULL;
	msurface_t *psurf;

	// compute vertexes count
	for( i = 0; i < bmodel->nummodelsurfaces; i++ )
	{
		psurf = &bmodel->surfaces[bmodel->firstmodelsurface + i];
		totalElems += (psurf->numedges - 2);
	}

	// create a temp indices array
	PxU32 *indices = new PxU32[totalElems * 3];

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

	PxTriangleMeshDesc meshDesc;
	meshDesc.points.count = bmodel->numvertexes;
	meshDesc.points.stride = sizeof(mvertex_t);
	meshDesc.points.data = (const PxVec3*)&(bmodel->vertexes[0].position);	// pointer to all vertices in the map
	meshDesc.triangles.count = numElems;
	meshDesc.triangles.stride = 3 * sizeof(PxU32);
	meshDesc.triangles.data = indices;
	meshDesc.flags = (PxMeshFlags)0;

#ifdef _DEBUG
	// mesh should be validated before cooked without the mesh cleaning
	bool res = m_pCooking->validateTriangleMesh(meshDesc);
	PX_ASSERT(res);
#endif

	MemoryWriteBuffer outputBuffer;
	bool status = m_pCooking->cookTriangleMesh(meshDesc, outputBuffer);
	delete [] indices;

	if( !status )
	{
		ALERT( at_error, "failed to create triangle mesh from %s\n", bmodel->name );
		return NULL;
	}

	MemoryReadBuffer inputBuffer(outputBuffer.getData(), outputBuffer.getSize());
	pMesh = m_pPhysics->createTriangleMesh(inputBuffer);
	if( !pMesh ) 
		ALERT( at_error, "failed to create triangle mesh from %s\n", bmodel->name );

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

clipfile::GeometryType CPhysicNovodex::ShapeTypeToGeomType(PxGeometryType::Enum geomType)
{
	switch (geomType)
	{
		case PxGeometryType::eCONVEXMESH: return clipfile::GeometryType::CookedConvexHull;
		case PxGeometryType::eTRIANGLEMESH: return clipfile::GeometryType::CookedTriangleMesh;
		case PxGeometryType::eBOX: return clipfile::GeometryType::CookedBox;
		default: return clipfile::GeometryType::Unknown;
	}
}

PxConvexMesh *CPhysicNovodex :: ConvexMeshFromStudio( entvars_t *pev, int modelindex, int32_t body, int32_t skin )
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

	PxConvexMesh *pHull = NULL;
	fs::Path cacheFileName;
	uint32_t modelStateHash = GetHashForModelState(smodel, body, skin);
	CacheNameForModel(smodel, cacheFileName, modelStateHash, ".hull");

	if (CheckFileTimes(smodel->name, cacheFileName.string().c_str()))
	{
		// hull is never than studiomodel. Trying to load it
		UserStream cacheFileStream(cacheFileName.string().c_str(), true);
		pHull = m_pPhysics->createConvexMesh(cacheFileStream);

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
	PxU32 *indices = new PxU32[psubmodel->numverts * 24];
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

	PxConvexMeshDesc meshDesc;
	meshDesc.indices.count = numElems / 3;
	meshDesc.indices.data = indices;
	meshDesc.indices.stride = 3 * sizeof( PxU32 );
	meshDesc.points.count = numVerts;
	meshDesc.points.data = verts;
	meshDesc.points.stride = sizeof(Vector);
	meshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	UserStream outputFileStream(cacheFileName.string().c_str(), false);
	bool status = m_pCooking->cookConvexMesh(meshDesc, outputFileStream);

	delete [] verts;
	delete [] m_verts;
	delete [] indices;

	if( !status )
	{
		ALERT( at_error, "failed to create convex mesh from %s\n", smodel->name );
		return NULL;
	}

	UserStream inputFileStream(cacheFileName.string().c_str(), true);
	pHull = m_pPhysics->createConvexMesh(inputFileStream);
	if( !pHull ) ALERT( at_error, "failed to create convex mesh from %s\n", smodel->name );

	return pHull;
}

PxTriangleMesh *CPhysicNovodex::TriangleMeshFromStudio(entvars_t *pev, int modelindex, int32_t body, int32_t skin)
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

	PxTriangleMesh *pMesh = NULL;
	fs::Path cacheFilePath;
	uint32_t modelStateHash = GetHashForModelState(smodel, body, skin);
	CacheNameForModel(smodel, cacheFilePath, modelStateHash, ".mesh");

	if (CheckFileTimes(smodel->name, cacheFilePath.string().c_str()) && !m_fWorldChanged)
	{
		// hull is never than studiomodel. Trying to load it
		UserStream cacheFileStream(cacheFilePath.string().c_str(), true);
		pMesh = m_pPhysics->createTriangleMesh(cacheFileStream);

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
		int32_t index = (pev->body / pbodypart->base) % pbodypart->nummodels;
		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		totalVertSize += psubmodel->numverts;
	}

	Vector *verts = new Vector[totalVertSize * 8]; // allocate temporary vertices array
	PxU32 *indices = new PxU32[totalVertSize * 24];
	int32_t skinNumber = bound( 0, pev->skin, phdr->numskinfamilies );
	int numVerts = 0, numElems = 0;
	Vector tmp;

	for (int k = 0; k < phdr->numbodyparts; k++)
	{
		int i;
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + k;
		int32_t index = (pev->body / pbodypart->base) % pbodypart->nummodels;
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
		if (skinNumber != 0 && skinNumber < phdr->numskinfamilies)
			pskinref += (skinNumber * phdr->numskinref);

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

	PxTriangleMeshDesc meshDesc;
	meshDesc.triangles.data = indices;
	meshDesc.triangles.count = numElems / 3;
	meshDesc.triangles.stride = 3 * sizeof(PxU32);
	meshDesc.points.data = verts;
	meshDesc.points.count = numVerts;
	meshDesc.points.stride = sizeof(Vector);
	meshDesc.flags = PxMeshFlag::eFLIPNORMALS;

#ifdef _DEBUG
	// mesh should be validated before cooked without the mesh cleaning
	bool res = m_pCooking->validateTriangleMesh(meshDesc);
	PX_ASSERT(res);
#endif

	UserStream outputFileStream(cacheFilePath.string().c_str(), false);
	bool status = m_pCooking->cookTriangleMesh(meshDesc, outputFileStream);
	delete[] verts;
	delete[] indices;

	if (!status)
	{
		ALERT(at_error, "failed to create triangle mesh from %s\n", smodel->name);
		return NULL;
	}

	UserStream inputFileStream(cacheFilePath.string().c_str(), true);
	pMesh = m_pPhysics->createTriangleMesh(inputFileStream);
	if (!pMesh) ALERT(at_error, "failed to create triangle mesh from %s\n", smodel->name);

	return pMesh;
}

PxConvexMesh *CPhysicNovodex :: ConvexMeshFromEntity( CBaseEntity *pObject )
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

	PxConvexMesh *pCollision = NULL;

	// call the apropriate loader
	switch( model->type )
	{
	case mod_brush:
		pCollision = ConvexMeshFromBmodel( pObject->pev, pObject->pev->modelindex );	
		break;
	case mod_studio:
		pCollision = ConvexMeshFromStudio( pObject->pev, pObject->pev->modelindex, pObject->pev->body, pObject->pev->skin );	
		break;
	}

	if( !pCollision && !m_fDisableWarning )
		ALERT( at_warning, "ConvexMeshFromEntity: %i has missing collision\n", pObject->pev->modelindex ); 
	m_fDisableWarning = FALSE;

	return pCollision;
}

PxTriangleMesh *CPhysicNovodex :: TriangleMeshFromEntity( CBaseEntity *pObject )
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

	PxTriangleMesh *pCollision = NULL;

	// call the apropriate loader
	switch( model->type )
	{
	case mod_brush:
		pCollision = TriangleMeshFromBmodel( pObject->pev, pObject->pev->modelindex );	
		break;
	case mod_studio:
		pCollision = TriangleMeshFromStudio( pObject->pev, pObject->pev->modelindex, pObject->pev->body, pObject->pev->skin );	
		break;
	}

	if( !pCollision && !m_fDisableWarning )
		ALERT( at_warning, "TriangleMeshFromEntity: %s has missing collision\n", pObject->GetClassname( )); 
	m_fDisableWarning = FALSE;

	return pCollision;
}

PxActor *CPhysicNovodex :: ActorFromEntity( CBaseEntity *pObject )
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
	return (PxActor *)pObject->m_pUserData;
}

CBaseEntity *CPhysicNovodex :: EntityFromActor( PxActor *pObject )
{
	if( !pObject || !pObject->userData )
		return NULL;

	return CBaseEntity::Instance( (edict_t *)pObject->userData );
}

bool CPhysicNovodex::CheckCollision(physx::PxRigidActor *pActor)
{
	std::vector<PxShape*> shapes;
	if (pActor->getNbShapes() > 0)
	{
		shapes.resize(pActor->getNbShapes());
		pActor->getShapes(&shapes[0], shapes.size());
		for (PxShape *shape : shapes) 
		{
			if (shape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE) {
				return true;
			}
		}
	}
	return false;
}

void CPhysicNovodex::ToggleCollision(physx::PxRigidActor *pActor, bool enabled)
{
	std::vector<PxShape*> shapes;
	shapes.resize(pActor->getNbShapes());
	pActor->getShapes(&shapes[0], shapes.size());

	for (PxShape *shape : shapes) {
		shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, enabled);
	}
}

void *CPhysicNovodex :: CreateBodyFromEntity( CBaseEntity *pObject )
{
	PxConvexMesh *pCollision = ConvexMeshFromEntity(pObject);
	if (!pCollision)
		return NULL;

	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(PxIdentity));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxConvexMeshGeometry(pCollision), *m_pDefaultMaterial);

	if (!pActor)
	{
		ALERT(at_error, "failed to create rigidbody from entity %s\n", pObject->GetClassname());
		return NULL;
	}

	float mat[16];
	float maxVelocity = CVAR_GET_FLOAT("sv_maxvelocity");
	matrix4x4(pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f).CopyToArray(mat);
	PxTransform pose = PxTransform(PxMat44(mat));

	pActor->setGlobalPose(pose);
	pActor->setName(pObject->GetClassname());
	pActor->setSolverIterationCounts(k_SolverIterationCount);
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
	pActor->setLinearVelocity(pObject->GetLocalVelocity());
	pActor->setAngularVelocity(pObject->GetLocalAvelocity());
	pActor->setMaxLinearVelocity(maxVelocity);
	pActor->setMaxAngularVelocity(720.0);
	pActor->userData = pObject->edict();
	PxRigidBodyExt::updateMassAndInertia(*pActor, k_DensityFactor);

	m_pScene->addActor(*pActor);
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
	PxBoxGeometry boxGeometry;
	boxGeometry.halfExtents = PxVec3(
		pObject->pev->size.x * k_PaddingFactor / 2.f,
		pObject->pev->size.y * k_PaddingFactor / 2.f,
		pObject->pev->size.z * k_PaddingFactor / 2.f
	);

	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(PxIdentity));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, boxGeometry, *m_pDefaultMaterial);

	if (!pActor)
	{
		ALERT( at_error, "failed to create rigidbody from entity %s\n", pObject->GetClassname());
		return NULL;
	}

	Vector origin = (pObject->IsMonster()) ? Vector( 0, 0, pObject->pev->maxs.z / 2.0f ) : g_vecZero;
	origin += pObject->GetAbsOrigin();
	PxTransform pose = PxTransform(origin);

	pActor->setName(pObject->GetClassname());
	pActor->setGlobalPose(pose);
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	pActor->userData = pObject->edict();
	m_pScene->addActor(*pActor);

	pObject->m_iActorType = ACTOR_CHARACTER;
	pObject->m_pUserData = pActor;

	return pActor;
}

void *CPhysicNovodex :: CreateKinematicBodyFromEntity( CBaseEntity *pObject )
{
	PxTriangleMesh *pCollision = TriangleMeshFromEntity( pObject );
	if (!pCollision) 
		return NULL;

	PxRigidDynamic *pActor = m_pPhysics->createRigidDynamic(PxTransform(PxIdentity));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(pCollision), *m_pDefaultMaterial);

	if( !pActor )
	{
		ALERT( at_error, "failed to create kinematic from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	if (!UTIL_CanRotate(pObject)) 
	{ 
		// entity missed origin-brush
		pActor->setRigidDynamicLockFlags(
			PxRigidDynamicLockFlag::eLOCK_ANGULAR_X | 
			PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y | 
			PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z);
	}

	float mat[16];
	matrix4x4( pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f ).CopyToArray( mat );

	PxTransform pose = PxTransform(PxMat44(mat));
	pActor->setName(pObject->GetClassname());
	pActor->setGlobalPose( pose );
	pActor->setSolverIterationCounts(k_SolverIterationCount);
	pActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	pActor->userData = pObject->edict();
	//m_ErrorCallback.hideWarning(true);
	m_pScene->addActor(*pActor);
	//m_ErrorCallback.hideWarning(false);
	pObject->m_iActorType = ACTOR_KINEMATIC;
	pObject->m_pUserData = pActor;
	
	return pActor;
}

void *CPhysicNovodex :: CreateStaticBodyFromEntity( CBaseEntity *pObject )
{
	PxTriangleMesh *pCollision = TriangleMeshFromEntity( pObject );
	if( !pCollision ) 
		return NULL;

	float mat[16];
	matrix4x4(pObject->GetAbsOrigin(), pObject->GetAbsAngles(), 1.0f).CopyToArray(mat);

	PxTransform pose = PxTransform(PxMat44(mat));
	PxMaterial *material = (pObject->pev->flags & FL_CONVEYOR) ? m_pConveyorMaterial : m_pDefaultMaterial;
	PxRigidStatic *pActor = m_pPhysics->createRigidStatic(pose);
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(pCollision), *material);

	if( !pActor )
	{
		ALERT( at_error, "failed to create static from entity %s\n", pObject->GetClassname( ));
		return NULL;
	}

	pActor->setName( pObject->GetClassname( ));
	pActor->userData = pObject->edict();
	m_pScene->addActor(*pActor);

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

bool CPhysicNovodex::UpdateEntityPos( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
		return false;

	PxRigidDynamic *pDynamicActor = pActor->is<PxRigidDynamic>();
	if (!pDynamicActor || pDynamicActor->isSleeping())
		return false;

	PxTransform pose = pDynamicActor->getGlobalPose();
	matrix4x4 m( PxMat44(pose).front() );

	Vector angles = m.GetAngles();
	Vector origin = m.GetOrigin();

	// store actor velocities too
	pEntity->SetLocalVelocity( pDynamicActor->getLinearVelocity() );
	pEntity->SetLocalAvelocity( pDynamicActor->getAngularVelocity() );
	Vector vecPrevOrigin = pEntity->GetAbsOrigin();

	pEntity->SetLocalAngles( angles );
	pEntity->SetLocalOrigin( origin );
	pEntity->RelinkEntity( TRUE, &vecPrevOrigin );

	return true;
}

bool CPhysicNovodex :: UpdateActorPos( CBaseEntity *pEntity )
{
	PxRigidActor *pActor = ActorFromEntity( pEntity )->is<PxRigidActor>();
	if( !pActor ) 
		return false;

	float mat[16];
	matrix4x4 m( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), 1.0f );
	m.CopyToArray( mat );

	PxTransform pose = PxTransform(PxMat44(mat));
	pActor->setGlobalPose( pose );

	PxRigidDynamic *pRigidDynamic = ActorFromEntity(pEntity)->is<PxRigidDynamic>();
	if (!(pRigidDynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
	{
		pRigidDynamic->setLinearVelocity( pEntity->GetLocalVelocity() );
		pRigidDynamic->setAngularVelocity( pEntity->GetLocalAvelocity() );
	}

	return true;
}

bool CPhysicNovodex :: IsBodySleeping( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
		return false;

	PxRigidDynamic *pDynamicActor = ActorFromEntity(pEntity)->is<PxRigidDynamic>();
	if (!pDynamicActor)
		return false;

	return pDynamicActor->isSleeping();
}

void CPhysicNovodex :: UpdateEntityAABB( CBaseEntity *pEntity )
{
	PxU32 boundsCount;
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
		return;

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor || pRigidActor->getNbShapes() <= 0)
		return;

	ClearBounds( pEntity->pev->absmin, pEntity->pev->absmax );
	PxTransform globalPose = pRigidActor->getGlobalPose();
	PxBounds3 *boundsList = PxRigidActorExt::getRigidActorShapeLocalBoundsList(*pRigidActor, boundsCount);
	
	for (PxU32 i = 0; i < boundsCount; i++)
	{
		const PxBounds3 &bbox = boundsList[i];
		AddPointToBounds( globalPose.transform(bbox.minimum), pEntity->pev->absmin, pEntity->pev->absmax );
		AddPointToBounds( globalPose.transform(bbox.maximum), pEntity->pev->absmin, pEntity->pev->absmax );
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
	PxRigidDynamic *pActor = ActorFromEntity( pEntity )->is<PxRigidDynamic>();

	if( !pActor )
	{
		ALERT( at_warning, "SaveBody: physic entity %i missed actor!\n", pEntity->m_iActorType );
		return;
	}

	PxShape *shape;
	pActor->getShapes(&shape, 1);
	PxFilterData filterData = shape->getSimulationFilterData();

	// filter data get and stored only for one shape, but it can be more than one
	// assumed that all attached shapes have same filter data
	pEntity->m_iFilterData[0] = filterData.word0;
	pEntity->m_iFilterData[1] = filterData.word1;
	pEntity->m_iFilterData[2] = filterData.word2;
	pEntity->m_iFilterData[3] = filterData.word3;

	pEntity->m_iActorFlags = pActor->getActorFlags();
	pEntity->m_iBodyFlags = pActor->getRigidBodyFlags();
	pEntity->m_flBodyMass = pActor->getMass();
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
	if (!m_pScene) 
		return NULL;

	PxShape *pShape;
	PxRigidDynamic *pActor;
	PxTransform pose;
	Vector angles = pEntity->GetAbsAngles();

	if (pEntity->m_iActorType == ACTOR_CHARACTER) {
		angles = g_vecZero;	// no angles for NPC and client
	}

	float mat[16];
	matrix4x4 m(pEntity->GetAbsOrigin(), angles, 1.0f);
	m.CopyToArray(mat);

	pose = PxTransform(PxMat44(mat));
	pActor = m_pPhysics->createRigidDynamic(pose);

	if (!pActor)
	{
		ALERT(at_error, "RestoreBody: unbale to create actor with type (%i)\n", pEntity->m_iActorType);
		return NULL;
	}

	switch( pEntity->m_iActorType )
	{
		case ACTOR_DYNAMIC:
		{
			PxConvexMesh *convexMesh = ConvexMeshFromEntity(pEntity);
			if (!convexMesh)
				return NULL;

			pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxConvexMeshGeometry(convexMesh), *m_pDefaultMaterial);
			break;
		}
		case ACTOR_CHARACTER:
		{
			PxBoxGeometry box;
			box.halfExtents = pEntity->pev->size * k_PaddingFactor;
			pShape = PxRigidActorExt::createExclusiveShape(*pActor, box, *m_pDefaultMaterial);
			break;
		}
		case ACTOR_KINEMATIC:
		case ACTOR_STATIC:
		{
			PxTriangleMesh *triangleMesh = TriangleMeshFromEntity(pEntity);
			if (!triangleMesh)
				return NULL;

			PxMaterial *pMaterial = m_pDefaultMaterial;
			if (pEntity->pev->flags & FL_CONVEYOR) {
				pMaterial = m_pConveyorMaterial;
			}
			pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(triangleMesh), *pMaterial);
			break;
		}
		default: 
		{
			ALERT(at_error, "RestoreBody: invalid actor type %i\n", pEntity->m_iActorType);
			return NULL;
		}
	}

	if (!pShape)
		return NULL; // failed to create shape

	// fill in actor description
	if (pEntity->m_iActorType != ACTOR_STATIC)
	{
		pActor->setRigidBodyFlags(static_cast<PxRigidBodyFlags>(pEntity->m_iBodyFlags));
		pActor->setMass(pEntity->m_flBodyMass);
		pActor->setSolverIterationCounts(k_SolverIterationCount);

		if (pEntity->m_iActorType != ACTOR_KINEMATIC)
		{
			pActor->setLinearVelocity(pEntity->GetAbsVelocity());
			pActor->setAngularVelocity(pEntity->GetAbsAvelocity());
		}
    }
  
	//ActorDesc.density = DENSITY_FACTOR;
	pActor->userData = pEntity->edict();
	pActor->setActorFlags(static_cast<PxActorFlags>(pEntity->m_iActorFlags));

	//if( pEntity->m_iActorType == ACTOR_KINEMATIC )
	//	m_ErrorCallback.hideWarning( true );

	//if( pEntity->m_iActorType == ACTOR_KINEMATIC )
	//	m_ErrorCallback.hideWarning( false );

	// apply specific actor params
	pActor->setName( pEntity->GetClassname() );
	pEntity->m_pUserData = pActor;

	if (pEntity->m_fFreezed && pEntity->m_iActorType == ACTOR_DYNAMIC) {
		pActor->putToSleep();
	}

	m_pScene->addActor(*pActor);
	return pActor;
}

void CPhysicNovodex :: SetAngles( CBaseEntity *pEntity, const Vector &angles )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
		return;

	float mat[9];
	matrix3x3 m(angles);
	m.CopyToArray(mat);

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform transform = pRigidDynamic->getGlobalPose();
	transform.q = PxQuat(PxMat33( mat ));
	pRigidDynamic->setGlobalPose(transform);
}

void CPhysicNovodex :: SetOrigin( CBaseEntity *pEntity, const Vector &origin )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
		return;

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform transform = pRigidDynamic->getGlobalPose();
	transform.p = origin;
	pRigidDynamic->setGlobalPose(transform);
}

void CPhysicNovodex :: SetVelocity( CBaseEntity *pEntity, const Vector &velocity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
		return;

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	pRigidDynamic->setLinearVelocity( velocity );
}

void CPhysicNovodex :: SetAvelocity( CBaseEntity *pEntity, const Vector &velocity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
		return;

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	pRigidDynamic->setAngularVelocity( velocity );
}

void CPhysicNovodex :: MoveObject( CBaseEntity *pEntity, const Vector &finalPos )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
		return;

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform pose = pRigidDynamic->getGlobalPose();
	pose.p = finalPos;
	pRigidDynamic->setKinematicTarget(pose);
}

void CPhysicNovodex :: RotateObject( CBaseEntity *pEntity, const Vector &finalAngle )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
		return;

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	PxTransform pose = pRigidDynamic->getGlobalPose();
	matrix3x3 m(finalAngle);
	pose.q = PxQuat(PxMat33(m));
	pRigidDynamic->setKinematicTarget(pose);
}

void CPhysicNovodex :: SetLinearMomentum( CBaseEntity *pEntity, const Vector &velocity )
{
	PxActor *pActor = ActorFromEntity(pEntity);
	if (!pActor)
		return;

	PxRigidDynamic *pRigidDynamic = pActor->is<PxRigidDynamic>();
	pRigidDynamic->setForceAndTorque(velocity, PxVec3(0.f), PxForceMode::eIMPULSE);
}

void CPhysicNovodex :: AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor) 
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	PxF32 coeff = (1000.0f / pRigidBody->getMass()) * factor;

	// prevent to apply too much impulse
	if (pRigidBody->getMass() < 8.0f)
	{
		coeff *= 0.0001f;
	}

	PxRigidBodyExt::addForceAtPos(*pRigidBody, PxVec3(impulse * coeff), position, PxForceMode::eIMPULSE);
}

void CPhysicNovodex :: AddForce( CBaseEntity *pEntity, const Vector &force )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor ) 
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	pRigidBody->addForce(force);
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

		if (m_pSceneActor)
			m_pSceneActor->release();
		m_pSceneActor = NULL;

		if (m_pSceneMesh)
			m_pSceneMesh->release();
		m_pSceneMesh = NULL;

		m_fLoaded = FALSE; // trying to load new collision tree 
	}

	// save off mapname
	strcpy(m_szMapName, szMapName);

	char szHullFilename[MAX_PATH];

	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s.bin", szMapName );
	UserStream cacheFileStream(szHullFilename, true);
	m_pSceneMesh = m_pPhysics->createTriangleMesh(cacheFileStream);
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

void CPhysicNovodex::CacheNameForModel( model_t *model, fs::Path &hullfile, uint32_t hash, const char *suffix )
{
	if (!model->name || model->name[0] == '\0')
		return;

	std::stringstream stream;
	std::string pathStr = model->name;
	pathStr.erase(0, pathStr.find_first_of("/\\") + 1);
	fs::Path modelPath = pathStr;
	stream << std::nouppercase << std::setfill('0') << std::setw(8) << std::hex << hash;
	hullfile = "cache" / modelPath.parent_path() / (modelPath.stem().string() + "_" + stream.str() + suffix);
}

uint32_t CPhysicNovodex::GetHashForModelState( model_t *model, int32_t body, int32_t skin )
{
	uint32_t hash;
	CRC32_Init(&hash);
	CRC32_ProcessBuffer(&hash, &body, sizeof(body));
	CRC32_ProcessBuffer(&hash, &skin, sizeof(skin));
	CRC32_ProcessBuffer(&hash, &model->modelCRC, sizeof(model->modelCRC));
	return CRC32_Final(hash);
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

bool CPhysicNovodex::DebugEnabled() const
{
#ifdef _DEBUG
	return true;
#else
	return false;
#endif
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

	if (m_pSceneActor)
		m_pSceneActor->release();
	m_pSceneActor = NULL;

	if (m_pSceneMesh)
		m_pSceneMesh->release();
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

	PxU32 *indices = new PxU32[totalElems * 3];

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

	PX_ASSERT( totalElems == numElems );

	// build physical model
	PxTriangleMeshDesc levelDesc;
	levelDesc.points.count = m_pWorldModel->numvertexes;
	levelDesc.points.data = (const PxVec3*)&(m_pWorldModel->vertexes[0].position);
	levelDesc.points.stride = sizeof(mvertex_t);
	levelDesc.triangles.count = numElems;
	levelDesc.triangles.data = indices;
	levelDesc.triangles.stride = 3 * sizeof(PxU32);
	levelDesc.flags = (PxMeshFlags)0;

	char szHullFilename[MAX_PATH];
	Q_snprintf( szHullFilename, sizeof( szHullFilename ), "cache/maps/%s.bin", szMapName );

	if (m_pCooking)
	{
		UserStream outputFileStream(szHullFilename, false);
		bool status = m_pCooking->cookTriangleMesh(levelDesc, outputFileStream);
	}

	delete [] indices;

	UserStream inputFileStream(szHullFilename, true);
	m_pSceneMesh = m_pPhysics->createTriangleMesh(inputFileStream);
	m_fWorldChanged = TRUE;

	return (m_pSceneMesh != NULL) ? TRUE : FALSE;
}

void CPhysicNovodex::SetupWorld(void)
{
	if (m_pSceneActor)
		return;	// already loaded

	if (!m_pSceneMesh)
	{
		ALERT(at_error, "*collision tree not ready!\n");
		return;
	}

	// get a world struct
	if ((m_pWorldModel = (model_t *)MODEL_HANDLE(1)) == NULL)
	{
		ALERT(at_error, "SetupWorld: unbale to fetch world pointer %s\n", m_szMapName);
		return;
	}

	PxRigidStatic *pActor = m_pPhysics->createRigidStatic(PxTransform(PxVec3(PxZero), PxQuat(PxIdentity)));
	PxShape *pShape = PxRigidActorExt::createExclusiveShape(*pActor, PxTriangleMeshGeometry(m_pSceneMesh), *m_pDefaultMaterial);

	pActor->setName(g_pWorld->GetClassname());
	pActor->userData = g_pWorld->edict();
	m_pScene->addActor(*pActor);
	m_pSceneActor = pActor;
	m_fLoaded = true;

	PxU32 boundsCount;
	PxBounds3 *boundsList = PxRigidActorExt::getRigidActorShapeLocalBoundsList(*pActor, boundsCount);
	m_worldBounds = boundsList[0];
}
	
void CPhysicNovodex :: DebugDraw( void )
{
	if( !m_pPhysics || !m_pScene )
		return;

	DebugRenderer::GetInstance().RenderData(m_pScene->getRenderBuffer());
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

void CPhysicNovodex :: FreeAllBodies()
{
	if( !m_pScene ) 
		return;

	PxActorTypeFlags actorFlags = (
		PxActorTypeFlag::eRIGID_STATIC |
		PxActorTypeFlag::eRIGID_DYNAMIC
	);

	std::vector<PxActor*> actors;
	actors.assign(m_pScene->getNbActors(actorFlags), nullptr);
	m_pScene->getActors(actorFlags, actors.data(), actors.size() * sizeof(actors[0]));

	// throw all bodies
	for (PxActor *actor : actors)
	{
		m_pScene->removeActor(*actor);
		actor->release();
	}
	m_pSceneActor = NULL;
}

void CPhysicNovodex :: TeleportCharacter( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (pRigidBody->getNbShapes() <= 0)
		return;

	PxShape *pShape;
	pRigidBody->getShapes(&pShape, sizeof(pShape)); // get only first shape, but it can be several
	Vector vecOffset = (pEntity->IsMonster()) ? Vector( 0, 0, pEntity->pev->maxs.z / 2.0f ) : g_vecZero;

	if (pShape->getGeometryType() == PxGeometryType::eBOX)
	{
		PxBoxGeometry &box = pShape->getGeometry().box();
		PxTransform pose = pRigidBody->getGlobalPose();
		box.halfExtents = PxVec3(pEntity->pev->size * k_PaddingFactor);
		pose.p = (pEntity->GetAbsOrigin() + vecOffset);
		pRigidBody->setGlobalPose(pose);
	}
	else {
		ALERT(at_error, "TeleportCharacter: shape geometry type is not a box\n");
	}
}

void CPhysicNovodex :: TeleportActor( CBaseEntity *pEntity )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	matrix4x4 m(pEntity->GetAbsOrigin(), pEntity->GetAbsAngles(), 1.0f);
	PxTransform pose = PxTransform(PxMat44(m));
	pRigidBody->setGlobalPose( pose );
}

void CPhysicNovodex :: MoveCharacter( CBaseEntity *pEntity )
{
	if( !pEntity || pEntity->m_vecOldPosition == pEntity->pev->origin )
		return;

	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (pRigidBody->getNbShapes() <= 0)
		return;

	PxShape *pShape;
	pRigidBody->getShapes(&pShape, sizeof(pShape)); // get only first shape, but it can be several
	if (pShape->getGeometryType() == PxGeometryType::eBOX)
	{
		PxBoxGeometry &box = pShape->getGeometry().box();

		// if were in NOCLIP or FLY (ladder climbing) mode - disable collisions
		if (pEntity->pev->movetype != MOVETYPE_WALK)
			ToggleCollision(pRigidBody, false);
		else 
			ToggleCollision(pRigidBody, true);

		Vector vecOffset = (pEntity->IsMonster()) ? Vector( 0, 0, pEntity->pev->maxs.z / 2.0f ) : g_vecZero;
		box.halfExtents = ( pEntity->pev->size * k_PaddingFactor );
		pShape->setGeometry(box); // should we do this?
		PxTransform pose = pRigidBody->getGlobalPose();
		pose.p = (pEntity->GetAbsOrigin() + vecOffset);
		pRigidBody->setKinematicTarget(pose);
		pEntity->m_vecOldPosition = pEntity->GetAbsOrigin(); // update old position
	}
}

void CPhysicNovodex :: MoveKinematic( CBaseEntity *pEntity )
{
	if( !pEntity || ( pEntity->pev->movetype != MOVETYPE_PUSH && pEntity->pev->movetype != MOVETYPE_PUSHSTEP ))
		return;	// probably not a mover

	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (pRigidBody->getNbShapes() <= 0)
		return;

	if( pEntity->pev->solid == SOLID_NOT || pEntity->pev->solid == SOLID_TRIGGER )
		ToggleCollision(pRigidBody, false);
	else 
		ToggleCollision(pRigidBody, true);

	PxTransform pose;
	matrix4x4 m( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles( ), 1.0f );

	// complex move for kinematic entities
	pose = PxTransform(PxMat44(m));
	pRigidBody->setKinematicTarget( pose );
}

void CPhysicNovodex :: EnableCollision( CBaseEntity *pEntity, int fEnable )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if( !pActor )
		return;

	PxRigidDynamic *pRigidBody = pActor->is<PxRigidDynamic>();
	if (pRigidBody->getNbShapes() <= 0)
		return;

	if( fEnable )
	{
		ToggleCollision(pRigidBody, false);
		pActor->setActorFlag(PxActorFlag::eVISUALIZATION, true);
	}
	else
	{
		ToggleCollision(pRigidBody, true);
		pActor->setActorFlag(PxActorFlag::eVISUALIZATION, false);
	}
}

void CPhysicNovodex :: MakeKinematic( CBaseEntity *pEntity, int fEnable )
{
	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (!pRigidBody || pRigidBody->getNbShapes() <= 0)
		return;

	if (fEnable)
		pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	else
		pRigidBody->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
}

void CPhysicNovodex :: SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, trace_t *tr )
{
	PxActor *pActor = ActorFromEntity( pTouch );
	if (!pActor)
	{
		// bad actor?
		tr->allsolid = false;
		return;
	}

	PxRigidActor *pRigidActor = pActor->is<PxRigidActor>();
	if (!pRigidActor || pRigidActor->getNbShapes() <= 0 || !CheckCollision(pRigidActor))
	{
		// bad actor?
		tr->allsolid = false;
		return;
	}

	Vector trace_mins, trace_maxs;
	UTIL_MoveBounds(start, mins, maxs, end, trace_mins, trace_maxs);

	// NOTE: pmove code completely ignore a bounds checking. So we need to do it here
	if (!BoundsIntersect(trace_mins, trace_maxs, pTouch->pev->absmin, pTouch->pev->absmax))
	{
		tr->allsolid = false;
		return;
	}

	DecomposedShape shape;
	vec3_t scale = pTouch->pev->startpos.Length() < 0.001f ? vec3_t(1.0f) : pTouch->pev->startpos;
	model_t *mod = (model_t *)MODEL_HANDLE(pTouch->pev->modelindex);

	auto &meshDescFactory = CMeshDescFactory::Instance();
	clipfile::GeometryType geomType = ShapeTypeToGeomType(shape.GetGeometryType(pRigidActor));
	CMeshDesc &cookedMesh = meshDescFactory.CreateObject(pTouch->pev->modelindex, pTouch->pev->body, pTouch->pev->skin, geomType);

	if (!cookedMesh.GetMesh())
	{
		const bool presentInCache = cookedMesh.PresentInCache();
		if (!presentInCache)
		{
			// update cache or build from scratch
			Vector triangle[3];
			if (!shape.Triangulate(pRigidActor))
			{
				// failed to triangulate, unsupported mesh type, so skip them
				tr->allsolid = false;
				return;
			}

			cookedMesh.SetDebugName(pTouch->GetModel());
			cookedMesh.InitMeshBuild(shape.GetTrianglesCount());

			// FIXME: store all meshes as local and use capsule instead of bbox
			const auto &indexBuffer = shape.GetIndexBuffer();
			const auto &vertexBuffer = shape.GetVertexBuffer();
			for (size_t i = 0; i < shape.GetTrianglesCount(); i++)
			{
				uint32_t i0 = indexBuffer[3 * i];
				uint32_t i1 = indexBuffer[3 * i + 1];
				uint32_t i2 = indexBuffer[3 * i + 2];
				vec3_t v0 = vertexBuffer[i0];
				vec3_t v1 = vertexBuffer[i1];
				vec3_t v2 = vertexBuffer[i2];
				triangle[0] = v0;
				triangle[1] = v1;
				triangle[2] = v2;
				cookedMesh.AddMeshTrinagle(triangle);
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

	PxActor *pActor = ActorFromEntity( pEntity );
	if (!pActor)
		return;

	PxRigidBody *pRigidBody = pActor->is<PxRigidBody>();
	if (!pRigidBody || pRigidBody->getNbShapes() <= 0 || pEntity->pev->solid == SOLID_NOT)
		return; // only dynamic solid objects can be traced

	// test for stuck entity into another
	if (start == end)
	{
		Vector triangle[3], dirs[3];
		PxTransform globalPose = pRigidBody->getGlobalPose();
		DecomposedShape shape;
		if (!shape.Triangulate(pRigidBody)) {
			return; // failed to triangulate
		}

		const auto &indexBuffer = shape.GetIndexBuffer();
		const auto &vertexBuffer = shape.GetVertexBuffer();
		for (size_t i = 0; i < shape.GetTrianglesCount(); i++)
		{
			uint32_t i0 = indexBuffer[3 * i];
			uint32_t i1 = indexBuffer[3 * i + 1];
			uint32_t i2 = indexBuffer[3 * i + 2];
			vec3_t v0 = vertexBuffer[i0];
			vec3_t v1 = vertexBuffer[i1];
			vec3_t v2 = vertexBuffer[i2];

			// transform triangles from local to world space
			triangle[0] = globalPose.transform(v0);
			triangle[1] = globalPose.transform(v1);
			triangle[2] = globalPose.transform(v2);

			for (size_t j = 0; j < 3; j++)
			{
				dirs[j] = globalPose.p - triangle[j];
				triangle[j] += dirs[j] * -2.0f;
				UTIL_TraceLine(triangle[j], triangle[j], ignore_monsters, pEntity->edict(), tr);
				if (tr->fStartSolid) 
					return;	// one of points in solid
			}
		}
		return;
	}

	// compute motion
	Vector vecDir = end - start;
	float flLength = vecDir.Length();
	vecDir = vecDir.Normalize();

	// setup trace box
	PxBoxGeometry testBox;
	PxTransform initialPose = pRigidBody->getGlobalPose();
	initialPose.p = pEntity->Center(); // does we really need to do this?
	testBox.halfExtents = pEntity->pev->size * 0.5f;

	// make a linear sweep through the world
	PxSweepBuffer sweepResult;
	ToggleCollision(pRigidBody, false);
	bool hitOccured = m_pScene->sweep(testBox, initialPose, vecDir, flLength, sweepResult, PxHitFlag::eNORMAL);
	ToggleCollision(pRigidBody, true);

	if (!hitOccured || !sweepResult.getNbTouches())
		return; // no intersection

	const PxSweepHit &hit = sweepResult.getTouch(0);
	if (hit.distance > flLength || !hit.actor)
		return; // hit missed

	// compute fraction
	tr->flFraction = (hit.distance / flLength);
	tr->flFraction = bound( 0.0f, tr->flFraction, 1.0f );
	VectorLerp( start, tr->flFraction, end, tr->vecEndPos );

	CBaseEntity *pHit = EntityFromActor( hit.actor );
	if (pHit) {
		tr->pHit = pHit->edict();
	}

	tr->vecPlaneNormal = hit.normal;
	tr->flPlaneDist = DotProduct( tr->vecEndPos, tr->vecPlaneNormal );
	float flDot = DotProduct( vecDir, tr->vecPlaneNormal );
	float moveDot = Q_round( flDot, 0.1f );

	// FIXME: this is incorrect. Find a better method?
	if(( tr->flFraction < 0.1f ) && ( moveDot < 0.0f ))
		tr->fAllSolid = true;
}

#endif // USE_PHYSICS_ENGINE
