/*
meshdesc.cpp - cached mesh for tracing custom objects
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

#include <alert.h>
#include "vector.h"
#include "matrix.h"
#include "const.h"
#include "com_model.h"
#include "trace.h"
#include "mathlib.h"
#include "stringlib.h"
#include "virtualfs.h"
#include "port.h"
#include "filesystem_utils.h"
#include "meshdesc_factory.h"

#ifdef CLIENT_DLL
#include "utils.h"
#include "render_api.h"
#else
#include "edict.h"
#include "eiface.h"
#include "physcallback.h"
#include "enginecallback.h"
#endif

#include <vector>

template<typename T, int boundary>
constexpr T AlignTo(T x)
{
	constexpr bool powerOfTwo = !(boundary == 0) && !(boundary & (boundary - 1));
	static_assert(powerOfTwo, "Boundary number is not power of 2");
	return (x + (boundary - 1)) & ~(boundary - 1);
}

CMeshDesc::CMeshDesc()
{
	memset(&m_mesh, 0, sizeof(m_mesh));
	mesh_size = 0;
	m_debugName = NULL;
	m_srcPlaneElems = NULL;
	m_curPlaneElems = NULL;
	m_srcPlaneHash = NULL;
	m_srcPlanePool = NULL;
	m_srcFacets = NULL;
	m_pModel = NULL;
	m_bMeshBuilt = false;
	has_tree = false;
	ClearBounds( m_mesh.mins, m_mesh.maxs );
	memset( areanodes, 0, sizeof( areanodes ));
	numareanodes = 0;
	m_iNumTris = 0;
	m_iBodyNumber = 0;
	m_iSkinNumber = 0;
	m_iGeometryType = clipfile::GeometryType::Unknown;
}

CMeshDesc::~CMeshDesc()
{
	FreeMesh();
}

void CMeshDesc::SetModel(const model_t *mod, int32_t body, int32_t skin) 
{ 
	m_pModel = (model_t *)mod; 
	m_iBodyNumber = body;
	m_iSkinNumber = skin;
}

void CMeshDesc::SetGeometryType(clipfile::GeometryType geomType)
{
	m_iGeometryType = geomType;
}

bool CMeshDesc::Matches(const std::string &name, int32_t body, int32_t skin, clipfile::GeometryType geomType)
{
	if (!m_debugName || name.compare(m_debugName) != 0)
		return false;
	if (m_iBodyNumber != body)
		return false;
	if (m_iSkinNumber != skin)
		return false;
	if (m_iGeometryType != geomType)
		return false;

	return true;
}

void CMeshDesc :: InsertLinkBefore( link_t *l, link_t *before )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void CMeshDesc :: RemoveLink( link_t *l )
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void CMeshDesc :: ClearLink( link_t *l )
{
	l->prev = l->next = l;
}

void CMeshDesc :: StartPacifier( void )
{
	m_iOldPercent = -1;
	UpdatePacifier( 0.001f );
}

void CMeshDesc :: UpdatePacifier( float percent )
{
	int	f;

	f = (int)(percent * (float)PACIFIER_STEP);
	f = bound( m_iOldPercent, f, PACIFIER_STEP );
	
	if( f != m_iOldPercent )
	{
		for( int i = m_iOldPercent + 1; i <= f; i++ )
		{
			if(( i % PACIFIER_REM ) == 0 )
			{
				Msg( "%d%%%%", ( i / PACIFIER_REM ) * 10 );
			}
			else
			{
				if( i != PACIFIER_STEP )
				{
					Msg( "." );
				}
			}
		}
		
		m_iOldPercent = f;
	}
}

void CMeshDesc :: EndPacifier( float total )
{
	UpdatePacifier( 1.0f );
	Msg( " (%.2f secs)", total );
	Msg( "\n" );
}

/*
===============
CreateAreaNode

builds a uniformly subdivided tree for the given mesh size
===============
*/
areanode_t *CMeshDesc :: CreateAreaNode( int depth, const Vector &mins, const Vector &maxs )
{
	areanode_t	*anode;
	Vector		size;
	Vector		mins1, maxs1;
	Vector		mins2, maxs2;

	anode = &areanodes[numareanodes++];

	// use 'solid_edicts' to store facets
	ClearLink( &anode->solid_edicts );
	
	if( depth == MAX_AREA_DEPTH )
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	size = maxs - mins;

	if( size[0] > size[1] )
		anode->axis = 0;
	else anode->axis = 1;
	
	anode->dist = 0.5f * ( maxs[anode->axis] + mins[anode->axis] );
	mins1 = mins;	
	mins2 = mins;	
	maxs1 = maxs;	
	maxs2 = maxs;	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	anode->children[0] = CreateAreaNode( depth+1, mins2, maxs2 );
	anode->children[1] = CreateAreaNode( depth+1, mins1, maxs1 );

	return anode;
}

void CMeshDesc :: FreeMesh( void )
{
	has_tree = false;
	m_bMeshBuilt = false;

	if( m_mesh.numfacets <= 0 )
		return;

	m_iTotalPlanes = 0;
	m_iAllocPlanes = 0;
	m_iHashPlanes = 0;
	m_iNumTris = 0;

	// single memory block
	Mem_Free( m_mesh.planes );

	FreeMeshBuild();

	memset( &m_mesh, 0, sizeof( m_mesh ));
}

/*
================
PlaneEqual
================
*/
bool CMeshDesc :: PlaneEqual( const mplane_t *p, const Vector &normal, float dist )
{
	vec_t	t;

	if( -PLANE_DIST_EPSILON < ( t = p->dist - dist ) && t < PLANE_DIST_EPSILON
	 && -PLANE_DIR_EPSILON < ( t = p->normal[0] - normal[0] ) && t < PLANE_DIR_EPSILON
	 && -PLANE_DIR_EPSILON < ( t = p->normal[1] - normal[1] ) && t < PLANE_DIR_EPSILON
	 && -PLANE_DIR_EPSILON < ( t = p->normal[2] - normal[2] ) && t < PLANE_DIR_EPSILON )
		return true;

	return false;
}

/*
=================
PlaneTypeForNormal
=================
*/
int CMeshDesc :: PlaneTypeForNormal( const Vector &normal )
{
	vec_t	ax, ay, az;

	ax = fabs( normal[0] );
	ay = fabs( normal[1] );
	az = fabs( normal[2] );

	if(( ax > 1.0 - PLANE_DIR_EPSILON ) && ( ay < PLANE_DIR_EPSILON ) && ( az < PLANE_DIR_EPSILON ))
		return PLANE_X;

	if(( ay > 1.0 - PLANE_DIR_EPSILON ) && ( az < PLANE_DIR_EPSILON ) && ( ax < PLANE_DIR_EPSILON ))
		return PLANE_Y;

	if(( az > 1.0 - PLANE_DIR_EPSILON ) && ( ax < PLANE_DIR_EPSILON ) && ( ay < PLANE_DIR_EPSILON ))
		return PLANE_Z;

	return PLANE_NONAXIAL;
}

/*
================
SnapNormal
================
*/
int CMeshDesc :: SnapNormal( Vector &normal )
{
	int	type = PlaneTypeForNormal( normal );
	bool	renormalize = false;

	// snap normal to nearest axial if possible
	if( type <= PLANE_LAST_AXIAL )
	{
		for( int i = 0; i < 3; i++ )
		{
			if( i == type )
				normal[i] = normal[i] > 0 ? 1 : -1;
			else normal[i] = 0;
		}
		renormalize = true;
	}
	else
	{
		for( int i = 0; i < 3; i++ )
		{
			if( fabs( fabs( normal[i] ) - 0.707106 ) < PLANE_DIR_EPSILON )
			{
				normal[i] = normal[i] > 0 ? 0.707106 : -0.707106;
				renormalize = true;
			}
		}
	}

	if( renormalize )
		normal = normal.Normalize();

	return type;
}

/*
================
CreateNewFloatPlane
================
*/
int CMeshDesc :: CreateNewFloatPlane( const Vector &srcnormal, float dist, int hash )
{
	hashplane_t *p;

	if( srcnormal.Length() < 0.5f )
		return -1;

	// sanity check
	if( m_mesh.numplanes >= m_iAllocPlanes )
		return -1;

	// snap plane normal
	Vector normal = srcnormal;
	int type = SnapNormal( normal );

	// only snap distance if the normal is an axis. Otherwise there
	// is nothing "natural" about snapping the distance to an integer.
	if( VectorIsOnAxis( normal ) && fabs( dist - Q_rint( dist )) < PLANE_DIST_EPSILON )
		dist = Q_rint( dist ); // catch -0.0

	// create a new one
	p = &m_srcPlanePool[m_mesh.numplanes];
	p->hash = m_srcPlaneHash[hash];
	m_srcPlaneHash[hash] = p;

	// record the new plane
	SetPlane( &p->pl, normal, dist, type );

	return m_mesh.numplanes++;
}

/*
=============
FindFloatPlane

=============
*/
int CMeshDesc :: FindFloatPlane( const Vector &normal, float dist )
{
	int	hash;

	// trying to find equal plane
	hash = (int)fabs( dist / 0.2f );
	hash &= (PLANE_HASHES - 1);

	// search the border bins as well
	for( int i = -1; i <= 1; i++ )
	{
		int h = (hash + i) & (PLANE_HASHES - 1);
		for( hashplane_t *p = m_srcPlaneHash[h]; p; p = p->hash )
		{
			if( PlaneEqual( &p->pl, normal, dist ))
				return (int)(p - m_srcPlanePool);	// already exist
		}
	}

	// allocate a new two opposite planes
	return CreateNewFloatPlane( normal, dist, hash );
}

/*
================
PlaneFromPoints
================
*/
int CMeshDesc :: PlaneFromPoints( const Vector &p0, const Vector &p1, const Vector &p2 )
{
	Vector t1 = p0 - p1;
	Vector t2 = p2 - p1;
	Vector normal = CrossProduct( t1, t2 );

	if( !normal.NormalizeLength())
		return -1;

	return FindFloatPlane( normal, DotProduct( normal, p0 ));
}

void CMeshDesc :: ExtractAnimValue( int frame, mstudioanim_t *panim, int dof, float scale, float &v1 )
{
	if( !panim || panim->offset[dof] == 0 )
	{
		v1 = 0.0f;
		return;
	}

	const mstudioanimvalue_t *panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[dof]);
	int k = frame;

	while( panimvalue->num.total <= k )
	{
		k -= panimvalue->num.total;
		panimvalue += panimvalue->num.valid + 1;

		if( panimvalue->num.total == 0 )
		{
			v1 = 0.0f;
			return;
		}
	}

	// Bah, missing blend!
	if( panimvalue->num.valid > k )
	{
		v1 = panimvalue[k+1].value * scale;
	}
	else
	{
		// get last valid data block
		v1 = panimvalue[panimvalue->num.valid].value * scale;
	}
}

void CMeshDesc :: StudioCalcBoneTransform( int frame, mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos, Vector4D &q )
{
	vec3_t	origin;
	Radian	angles;

	ExtractAnimValue( frame, panim, 0, pbone->scale[0], origin.x );
	ExtractAnimValue( frame, panim, 1, pbone->scale[1], origin.y );
	ExtractAnimValue( frame, panim, 2, pbone->scale[2], origin.z );
	ExtractAnimValue( frame, panim, 3, pbone->scale[3], angles.x );
	ExtractAnimValue( frame, panim, 4, pbone->scale[4], angles.y );
	ExtractAnimValue( frame, panim, 5, pbone->scale[5], angles.z );

	for( int j = 0; j < 3; j++ )
	{
		origin[j] = pbone->value[j+0] + origin[j];
		angles[j] = pbone->value[j+3] + angles[j];
	}

	AngleQuaternion( angles, q );
	VectorCopy( origin, pos );
}

bool CMeshDesc :: AddMeshTrinagle( const mvert_t triangle[3], int skinref )
{
	int	i, planenum;

	if( m_iNumTris <= 0 )
		return false; // were not in a build mode!

	if( m_mesh.numfacets >= m_iNumTris )
		return false; // not possible?

	mfacet_t *facet = &m_srcFacets[m_mesh.numfacets];
	mplane_t *mainplane;

	// calculate plane for this triangle
	if(( planenum = PlaneFromPoints( triangle[0].point, triangle[1].point, triangle[2].point )) == -1 )
		return false; // bad plane

	mplane_t planes[MAX_FACET_PLANES];
	Vector normal;
	int numplanes;
	float dist;

	mainplane = &m_srcPlanePool[planenum].pl;
	facet->numplanes = numplanes = 0;

	planes[numplanes].normal = mainplane->normal;
	planes[numplanes].dist = mainplane->dist;
	numplanes++;

	// calculate mins & maxs
	ClearBounds( facet->mins, facet->maxs );

	for( i = 0; i < 3; i++ )
	{
		AddPointToBounds( triangle[i].point, facet->mins, facet->maxs );
		facet->triangle[i] = triangle[i];
	}

	facet->edge1 = facet->triangle[1].point - facet->triangle[0].point;
	facet->edge2 = facet->triangle[2].point - facet->triangle[0].point;

	// add the axial planes
	for( int axis = 0; axis < 3; axis++ )
	{
		for( int dir = -1; dir <= 1; dir += 2 )
		{
			for( i = 0; i < numplanes; i++ )
			{
				if( planes[i].normal[axis] == dir )
					break;
			}

			if( i == numplanes )
			{
				normal = g_vecZero;
				normal[axis] = dir;
				if( dir == 1 )
					dist = facet->maxs[axis];
				else dist = -facet->mins[axis];

				planes[numplanes].normal = normal;
				planes[numplanes].dist = dist;
				numplanes++;
			}

			if( numplanes >= MAX_FACET_PLANES )
				return false;
		}
	}

	// add the edge bevels
	for( i = 0; i < 3; i++ )
	{
		int j = (i + 1) % 3;
		int k = (i + 2) % 3;

		Vector vec = triangle[i].point - triangle[j].point;
		if( vec.Length() < 0.5f ) continue;

		vec = vec.Normalize();
//		SnapNormal( vec );

		for( j = 0; j < 3; j++ )
		{
			if( vec[j] == 1.0f || vec[j] == -1.0f )
				break; // axial
		}

		if( j != 3 ) continue; // only test non-axial edges

		// try the six possible slanted axials from this edge
		for( int axis = 0; axis < 3; axis++ )
		{
			for( int dir = -1; dir <= 1; dir += 2 )
			{
				// construct a plane
				Vector vec2 = g_vecZero;
				vec2[axis] = dir;
				normal = CrossProduct( vec, vec2 );

				if( normal.Length() < 0.5f )
					continue;

				normal = normal.Normalize();
				dist = DotProduct( triangle[i].point, normal );

				for( j = 0; j < numplanes; j++ )
				{
					// if this plane has already been used, skip it
					if( PlaneEqual( &planes[j], normal, dist ))
						break;
				}

				if( j != numplanes ) continue;

				// if all other points are behind this plane, it is a proper edge bevel
				for( j = 0; j < 3; j++ )
				{
					if( j != i )
					{
						float d = DotProduct( triangle[j].point, normal ) - dist;
						// point in front: this plane isn't part of the outer hull
						if( d > 0.1f ) break;
					}
				}

				if( j != 3 ) continue;

				// add this plane
				planes[numplanes].normal = normal;
				planes[numplanes].dist = dist;
				numplanes++;

				if( numplanes >= MAX_FACET_PLANES )
					return false;
			}
		}
	}

	// add triangle to bounds
	for( i = 0; i < 3; i++ )
		AddPointToBounds( triangle[i].point, m_mesh.mins, m_mesh.maxs );

	facet->indices = m_curPlaneElems;
	m_curPlaneElems += numplanes;
	facet->numplanes = numplanes;
	facet->skinref = skinref;

	for( i = 0; i < facet->numplanes; i++ )
	{
		// add plane to global pool
		facet->indices[i] = FindFloatPlane( planes[i].normal, planes[i].dist );
	}

	for( i = 0; i < 3; i++ )
	{
		// spread the mins / maxs by a pixel
		facet->mins[i] -= 1.0f;
		facet->maxs[i] += 1.0f;
	}

	// added
	m_iTotalPlanes += numplanes;
	m_mesh.numfacets++;
	return true;
}

bool CMeshDesc::AddMeshTrinagle(const Vector triangle[3])
{
	int	i;

	if (m_iNumTris <= 0)
		return false; // were not in a build mode!

	if (m_mesh.numfacets >= m_iNumTris)
	{
		ALERT(at_error, "AddMeshTriangle: %s overflow (%i >= %i)\n", m_debugName, m_mesh.numfacets, m_iNumTris);
		return false;
	}

	int planenum;
	mfacet_t *facet = &m_srcFacets[m_mesh.numfacets];
	mplane_t *mainplane;

	if ((planenum = PlaneFromPoints(triangle[0], triangle[1], triangle[2])) == -1)
		return false; // bad plane

	mainplane = &m_srcPlanePool[planenum].pl;

	mplane_t planes[MAX_FACET_PLANES];
	Vector normal;
	int numplanes;
	float dist;

	facet->numplanes = numplanes = 0;

	// add front plane
	SnapPlaneToGrid(mainplane);

	planes[numplanes].normal = mainplane->normal;
	planes[numplanes].dist = mainplane->dist;
	numplanes++;

	// calculate mins & maxs
	ClearBounds(facet->mins, facet->maxs);

	for (i = 0; i < 3; i++)
	{
		AddPointToBounds(triangle[i], facet->mins, facet->maxs);
		facet->triangle[i].point = triangle[i];
	}

	facet->edge1 = facet->triangle[1].point - facet->triangle[0].point;
	facet->edge2 = facet->triangle[2].point - facet->triangle[0].point;

	// add the axial planes
	for (int axis = 0; axis < 3; axis++)
	{
		for (int dir = -1; dir <= 1; dir += 2)
		{
			for (i = 0; i < numplanes; i++)
			{
				if (planes[i].normal[axis] == dir)
					break;
			}

			if (i == numplanes)
			{
				normal = g_vecZero;
				normal[axis] = dir;
				if (dir == 1)
					dist = facet->maxs[axis];
				else dist = -facet->mins[axis];

				planes[numplanes].normal = normal;
				planes[numplanes].dist = dist;
				numplanes++;
			}
		}
	}

	// add the edge bevels
	for (i = 0; i < 3; i++)
	{
		int j = (i + 1) % 3;
		int k = (i + 2) % 3;

		Vector vec = triangle[i] - triangle[j];
		if (vec.Length() < 0.5f) continue;

		vec = vec.Normalize();
		SnapVectorToGrid(vec);

		for (j = 0; j < 3; j++)
		{
			if (vec[j] == 1.0f || vec[j] == -1.0f)
				break; // axial
		}

		if (j != 3) continue; // only test non-axial edges

		// try the six possible slanted axials from this edge
		for (int axis = 0; axis < 3; axis++)
		{
			for (int dir = -1; dir <= 1; dir += 2)
			{
				// construct a plane
				Vector vec2 = g_vecZero;
				vec2[axis] = dir;
				normal = CrossProduct(vec, vec2);

				if (normal.Length() < 0.5f)
					continue;

				normal = normal.Normalize();
				dist = DotProduct(triangle[i], normal);

				for (j = 0; j < numplanes; j++)
				{
					// if this plane has already been used, skip it
					if (ComparePlanes(&planes[j], normal, dist))
						break;
				}

				if (j != numplanes) continue;

				// if all other points are behind this plane, it is a proper edge bevel
				for (j = 0; j < 3; j++)
				{
					if (j != i)
					{
						float d = DotProduct(triangle[j], normal) - dist;
						// point in front: this plane isn't part of the outer hull
						if (d > 0.1f) break;
					}
				}

				if (j != 3) continue;

				// add this plane
				planes[numplanes].normal = normal;
				planes[numplanes].dist = dist;
				numplanes++;
			}
		}
	}

	// add triangle to bounds
	for (i = 0; i < 3; i++)
		AddPointToBounds(triangle[i], m_mesh.mins, m_mesh.maxs);

	facet->indices = m_curPlaneElems;
	m_curPlaneElems += numplanes;
	facet->numplanes = numplanes;
	facet->skinref = -1;

	for (i = 0; i < facet->numplanes; i++)
	{
		// add plane to global pool
		SnapPlaneToGrid(&planes[i]);
		facet->indices[i] = FindFloatPlane(planes[i].normal, planes[i].dist);
	}

	for (i = 0; i < 3; i++)
	{
		// spread the mins / maxs by a pixel
		facet->mins[i] -= 1.0f;
		facet->maxs[i] += 1.0f;
	}

	// added
	m_iTotalPlanes += numplanes;
	m_mesh.numfacets++;
	return true;
}

void CMeshDesc :: RelinkFacet( mfacet_t *facet )
{
	// find the first node that the facet box crosses
	areanode_t *node = areanodes;

	while( 1 )
	{
		if( node->axis == -1 ) break;
		if( facet->mins[node->axis] > node->dist )
			node = node->children[0];
		else if( facet->maxs[node->axis] < node->dist )
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in	
	InsertLinkBefore( &facet->area, &node->solid_edicts );
}

CMeshDesc::LoadStatus CMeshDesc::StudioLoadCache()
{
	int	i, length, iCompare;
	std::string filePath;

	GetCacheFilePath(filePath);
	if( COMPARE_FILE_TIME( m_pModel->name, const_cast<char*>(filePath.c_str()), &iCompare))
	{
		// MDL file is newer.
		if( iCompare > 0 )
			return LoadStatus::Error;
	}
	else {
		return LoadStatus::Error;
	}

	byte *aMemFile = LOAD_FILE( filePath.c_str(), &length );
	if( !aMemFile ) return LoadStatus::Error;

	CVirtualFS file( aMemFile, length );
	clipfile::CacheDataLump *lump;
	clipfile::Header hdr;
	clipfile::Facet facet;
	clipfile::Plane plane;
	clipfile::CacheTable table;
	clipfile::CacheData data;
	std::vector<clipfile::CacheEntry> entriesList;
	const clipfile::CacheEntry *desiredEntry = nullptr;

	FREE_FILE( aMemFile );
	file.Read( &hdr, sizeof( hdr ));
	if( hdr.id != clipfile::kFileMagic )
	{
		ALERT( at_warning, "%s has wrong file signature %X\n", filePath.c_str(), hdr.id );
		return LoadStatus::Error;
	}

	if( hdr.version != clipfile::kFormatVersion )
	{
		ALERT( at_warning, "%s has wrong version (%i should be %i)\n", filePath.c_str(), hdr.version, clipfile::kFormatVersion );
		return LoadStatus::Error;
	}

	if( hdr.modelCRC != m_pModel->modelCRC )
	{
		ALERT( at_console, "%s was changed, CLIP cache will be updated\n", filePath.c_str() );
		return LoadStatus::Error;
	}

	ClearBounds( m_mesh.mins, m_mesh.maxs );
	memset( areanodes, 0, sizeof( areanodes ));
	numareanodes = 0;

	// read cache entries table
	uint32_t entriesCount;
	file.Seek(hdr.tableOffset, SEEK_SET);
	file.Read(&entriesCount, sizeof(entriesCount));

	entriesList.reserve(entriesCount);
	for (size_t i = 0; i < entriesCount; i++)
	{
		auto &entry = entriesList.emplace_back();
		file.Read(&entry, sizeof(table.entries));
	}

	for (size_t i = 0; i < entriesCount; i++)
	{
		const clipfile::CacheEntry &entry = entriesList[i];
		if (entry.body == m_iBodyNumber && entry.skin == m_iSkinNumber && entry.geometryType == m_iGeometryType) {
			desiredEntry = &entry;
			break;
		}
	}

	if (!desiredEntry)
	{
		ALERT(at_console, "%s doesn't contain required cache entry (body %d / skin %d / geom. type %d)\n", filePath.c_str(), m_iBodyNumber, m_iSkinNumber, m_iGeometryType);
		return LoadStatus::EntryNotFound;
	}

	// read needed cache data lumps
	file.Seek(desiredEntry->dataOffset, SEEK_SET);
	file.Read(&data, sizeof(data));

	// read plane representation table
	lump = &data.lumps[clipfile::kDataLumpPlaneIndexes];
	m_iAllocPlanes = m_iTotalPlanes = lump->filelen / sizeof( uint32_t );

	if( lump->filelen <= 0 || lump->filelen % sizeof( uint32_t ))
	{
		ALERT( at_warning, "%s has funny size of LUMP_CLIP_PLANE_INDEXES\n", filePath.c_str() );
		return LoadStatus::Error;
	}

	m_srcPlaneElems = (uint *)calloc( sizeof( uint32_t ), m_iAllocPlanes );
	m_curPlaneElems = m_srcPlaneElems;
	file.Seek( lump->fileofs, SEEK_SET );

	// fill in plane indexes
	file.Read( m_srcPlaneElems, lump->filelen );

	// read unique planes array (hash is unused)
	lump = &data.lumps[clipfile::kDataLumpPlanes];
	m_mesh.numplanes = lump->filelen / sizeof( clipfile::Plane );

	if( lump->filelen <= 0 || lump->filelen % sizeof( clipfile::Plane ))
	{
		ALERT( at_warning, "%s has funny size of LUMP_CLIP_PLANES\n", filePath.c_str() );
		return LoadStatus::Error;
	}

	m_srcPlanePool = (hashplane_t *)calloc( sizeof( hashplane_t ), m_mesh.numplanes );
	file.Seek( lump->fileofs, SEEK_SET );

	for( i = 0; i < m_mesh.numplanes; i++ )
	{
		file.Read( &plane, sizeof( plane ));
		m_srcPlanePool[i].pl.normal = plane.normal;
		m_srcPlanePool[i].pl.dist = plane.dist;
		m_srcPlanePool[i].pl.type = plane.type;
		// categorize it
		CategorizePlane( &m_srcPlanePool[i].pl );
	}

	lump = &data.lumps[clipfile::kDataLumpFacets];
	m_mesh.numfacets = m_iNumTris = lump->filelen / sizeof( clipfile::Facet );

	if( lump->filelen <= 0 || lump->filelen % sizeof( clipfile::Facet ))
	{
		ALERT( at_warning, "%s has funny size of LUMP_CLIP_FACETS\n", filePath.c_str() );
		return LoadStatus::Error;
	}

	m_srcFacets = (mfacet_t *)calloc( sizeof( mfacet_t ), m_iNumTris );
	file.Seek( lump->fileofs, SEEK_SET );

	for( i = 0; i < m_iNumTris; i++ )
	{
		file.Read( &facet, sizeof( facet ));
		m_srcFacets[i].skinref = facet.skinref;
		m_srcFacets[i].mins = facet.mins;
		m_srcFacets[i].maxs = facet.maxs;
		m_srcFacets[i].edge1 = facet.edge1;
		m_srcFacets[i].edge2 = facet.edge2;
		m_srcFacets[i].numplanes = facet.numplanes;

		// bounds checking
		if( m_curPlaneElems != &m_srcPlaneElems[facet.firstindex] )
			Msg( "bad mem %p != %p\n", m_curPlaneElems, &m_srcPlaneElems[facet.firstindex] );

		// just setup pointer to index array
		m_srcFacets[i].indices = m_curPlaneElems;
		m_curPlaneElems += m_srcFacets[i].numplanes;

		for( int k = 0; k < 3; k++ )
		{
			m_srcFacets[i].triangle[k] = facet.triangle[k];
			AddPointToBounds( facet.triangle[k].point, m_mesh.mins, m_mesh.maxs );
		}
	}

	if( m_iNumTris >= 256 )
		has_tree = true;	// too many triangles invoke to build AABB tree
	else has_tree = false;

	return LoadStatus::Success;
}

bool CMeshDesc::StudioSaveCache()
{
	std::string filePath;
	clipfile::Header fileHeader;
	clipfile::CacheData data;
	clipfile::CacheDataLump *lump;
	uint32_t entriesCount;
	std::vector<clipfile::CacheEntry> cacheEntries;

	GetCacheFilePath(filePath);
	fs::File cacheFile(filePath, "a+b");
	if (!cacheFile.IsOpen()) {
		// msg
		return false;
	}

	cacheFile.Seek(0, fs::SeekType::Set);
	cacheFile.Read(&fileHeader, sizeof(fileHeader));
	cacheFile.Seek(fileHeader.tableOffset, fs::SeekType::Set);
	cacheFile.Read(&entriesCount, sizeof(entriesCount));

	// store all entries in local buffer
	cacheEntries.reserve(entriesCount);
	for (size_t i = 0; i < entriesCount; i++)
	{
		clipfile::CacheEntry &entry = cacheEntries.emplace_back();
		cacheFile.Read(&entry, sizeof(entry));
	}

	std::vector<clipfile::Facet> facets;
	std::vector<clipfile::Plane> planes;
	facets.resize(m_mesh.numfacets);
	planes.resize(m_mesh.numplanes);
	clipfile::Facet *out_facets = facets.data();
	clipfile::Plane *out_planes = planes.data();
	
	// copy planes into mesh array (probably aligned block)
	int32_t curIndex = 0;
	for (size_t i = 0; i < m_mesh.numfacets; i++)
	{
		out_facets[i].mins = m_srcFacets[i].mins;
		out_facets[i].maxs = m_srcFacets[i].maxs;
		out_facets[i].edge1 = m_srcFacets[i].edge1;
		out_facets[i].edge2 = m_srcFacets[i].edge2;
		out_facets[i].numplanes = m_srcFacets[i].numplanes;
		out_facets[i].skinref = m_srcFacets[i].skinref;
		out_facets[i].firstindex = curIndex;
		curIndex += m_srcFacets[i].numplanes;

		for (int k = 0; k < 3; k++) {
			out_facets[i].triangle[k] = m_srcFacets[i].triangle[k];
		}
	}

	if (curIndex != m_iTotalPlanes) {
		ALERT(at_error, "StudioSaveCache: invalid planecount! %d != %d\n", curIndex, m_iTotalPlanes);
	}

	for (size_t i = 0; i < m_mesh.numplanes; i++)
	{
		VectorCopy(m_srcPlanePool[i].pl.normal, out_planes[i].normal);
		out_planes[i].dist = m_srcPlanePool[i].pl.dist;
		out_planes[i].type = m_srcPlanePool[i].pl.type;
	}

	// goto end of file
	cacheFile.Seek(0, fs::SeekType::End);

	// add new data entry to list
	auto &currentEntry = cacheEntries.emplace_back();
	currentEntry.body = m_iBodyNumber;
	currentEntry.skin = m_iSkinNumber;
	currentEntry.geometryType = m_iGeometryType;
	currentEntry.dataOffset = cacheFile.Tell();
	
	// write lumps 
	cacheFile.Write(&data, sizeof(data));

	lump = &data.lumps[clipfile::kDataLumpFacets];
	lump->fileofs = cacheFile.Tell();
	lump->filelen = sizeof(clipfile::Facet) * m_mesh.numfacets;
	cacheFile.Write(out_facets, AlignTo<size_t, 4>(lump->filelen));

	lump = &data.lumps[clipfile::kDataLumpPlanes];
	lump->fileofs = cacheFile.Tell();
	lump->filelen = sizeof(clipfile::Plane) * m_mesh.numplanes;
	cacheFile.Write(out_planes, AlignTo<size_t, 4>(lump->filelen));

	lump = &data.lumps[clipfile::kDataLumpPlaneIndexes];
	lump->fileofs = cacheFile.Tell();
	lump->filelen = sizeof(uint32_t) * m_iTotalPlanes;
	cacheFile.Write(m_srcPlaneElems, AlignTo<size_t, 4>(lump->filelen));

	currentEntry.dataLength = cacheFile.Tell() - currentEntry.dataOffset;

	// write cache table back to file
	fileHeader.tableOffset = cacheFile.Tell();
	entriesCount += 1;
	cacheFile.Write(&entriesCount, sizeof(entriesCount));

	for (size_t i = 0; i < cacheEntries.size(); i++) {
		clipfile::CacheEntry &entry = cacheEntries[i];
		cacheFile.Write(&entry, sizeof(entry));
	}
	cacheFile.Close();

	// update file header
	cacheFile.Open(filePath, "r+b");
	cacheFile.Seek(0, fs::SeekType::Set);
	cacheFile.Write(&fileHeader, sizeof(fileHeader));

	// update lumps
	cacheFile.Seek(currentEntry.dataOffset, fs::SeekType::Set);
	cacheFile.Write(&data, sizeof(data));
	cacheFile.Close();

	return true;
}

bool CMeshDesc :: StudioCreateCache()
{
	clipfile::CacheDataLump *lump;
	clipfile::Header hdr;
	clipfile::CacheTable table;
	clipfile::CacheData data;
	CVirtualFS file;
	int i, curIndex;

	// something went wrong
	if( m_mesh.numfacets <= 0 )
		return false;

	memset(&hdr, 0, sizeof(hdr));
	hdr.id = clipfile::kFileMagic;
	hdr.version = clipfile::kFormatVersion;
	hdr.modelCRC = m_pModel->modelCRC;

	file.Write(&hdr, sizeof(hdr));
	table.entriesCount = 1;
	table.entries[0].body = m_iBodyNumber;
	table.entries[0].skin = m_iSkinNumber;
	table.entries[0].geometryType = m_iGeometryType;
	table.entries[0].dataOffset = file.Tell();
	file.Write(&data, sizeof(data));

	std::vector<clipfile::Facet> facets;
	std::vector<clipfile::Plane> planes;
	facets.resize(m_mesh.numfacets);
	planes.resize(m_mesh.numplanes);
	clipfile::Facet *out_facets = facets.data();
	clipfile::Plane *out_planes = planes.data();

	// copy planes into mesh array (probably aligned block)
	for( i = 0, curIndex = 0; i < m_mesh.numfacets; i++ )
	{
		out_facets[i].mins = m_srcFacets[i].mins;
		out_facets[i].maxs = m_srcFacets[i].maxs;
		out_facets[i].edge1 = m_srcFacets[i].edge1;
		out_facets[i].edge2 = m_srcFacets[i].edge2;
		out_facets[i].numplanes = m_srcFacets[i].numplanes;
		out_facets[i].skinref = m_srcFacets[i].skinref;
		out_facets[i].firstindex = curIndex;
		curIndex += m_srcFacets[i].numplanes;

		for( int k = 0; k < 3; k++ )
			out_facets[i].triangle[k] = m_srcFacets[i].triangle[k];
	}

	if( curIndex != m_iTotalPlanes ) 
		ALERT( at_error, "StudioCreateCache: invalid planecount! %d != %d\n", curIndex, m_iTotalPlanes );

	for( i = 0; i < m_mesh.numplanes; i++ )
	{
		VectorCopy( m_srcPlanePool[i].pl.normal, out_planes[i].normal );
		out_planes[i].dist = m_srcPlanePool[i].pl.dist;
		out_planes[i].type = m_srcPlanePool[i].pl.type;
	}

	lump = &data.lumps[clipfile::kDataLumpFacets];
	lump->fileofs = file.Tell();
	lump->filelen = sizeof(clipfile::Facet) * m_mesh.numfacets;
	file.Write(out_facets, AlignTo<size_t, 4>(lump->filelen));

	lump = &data.lumps[clipfile::kDataLumpPlanes];
	lump->fileofs = file.Tell();
	lump->filelen = sizeof(clipfile::Plane) * m_mesh.numplanes;
	file.Write(out_planes, AlignTo<size_t, 4>(lump->filelen));

	lump = &data.lumps[clipfile::kDataLumpPlaneIndexes];
	lump->fileofs = file.Tell();
	lump->filelen = sizeof(uint32_t) * m_iTotalPlanes;
	file.Write(m_srcPlaneElems, AlignTo<size_t, 4>(lump->filelen));

	// fill valid data length for cache table entry
	table.entries[0].dataLength = file.Tell() - table.entries[0].dataOffset;

	// write cache table to file & write offset to table in header
	hdr.tableOffset = file.Tell();
	file.Write(&table, sizeof(table));

	// update data lumps
	file.Seek(table.entries[0].dataOffset, SEEK_SET);
	file.Write(&data, sizeof(data));

	// update header
	file.Seek(0, SEEK_SET);
	file.Write(&hdr, sizeof(hdr));

	std::string filePath;
	GetCacheFilePath(filePath);
	if( SAVE_FILE( filePath.c_str(), file.GetBuffer(), file.GetSize()))
		return true;

	ALERT( at_error, "StudioCreateCache: couldn't store %s\n", filePath.c_str() );
	return false;
}

void CMeshDesc::GetCacheFilePath(std::string &filePath) const
{
	std::string fileName = m_pModel->name;
	fileName.erase(0, fileName.find_first_of("/\\") + 1);
	fileName.erase(fileName.find_last_of("."));
	filePath = "cache/" + fileName + ".clip";
}

bool CMeshDesc::CacheFileExists() const
{
	std::string filePath;
	GetCacheFilePath(filePath);
	if (fs::FileExists(filePath)) {
		return true;
	}
	return false;
}

bool CMeshDesc :: InitMeshBuild( int numTriangles )
{
	if( numTriangles <= 0 )
		return false;

	// perfomance warning
	if( numTriangles >= (MAX_TRIANGLES_SOFT / 2))
		ALERT( at_warning, "%s have too many triangles (%i)\n", m_debugName, numTriangles );

	// show the pacifier for user is knowledge what engine is not hanging
	m_bShowPacifier = numTriangles >= (MAX_TRIANGLES_SOFT / 8);

	if( numTriangles >= 256 )
		has_tree = true;	// too many triangles invoke to build AABB tree
	else has_tree = false;

	ClearBounds( m_mesh.mins, m_mesh.maxs );
	memset( areanodes, 0, sizeof( areanodes ));
	numareanodes = 0;

	// bevels for each triangle can't exceeds MAX_FACET_PLANES
	m_iAllocPlanes = numTriangles * MAX_FACET_PLANES;
	m_iHashPlanes = (m_iAllocPlanes>>2);
	m_iNumTris = numTriangles;
	m_iTotalPlanes = 0;

	// create pools for construct mesh
	m_srcFacets = (mfacet_t *)calloc( sizeof( mfacet_t ), numTriangles );
	m_srcPlaneHash = (hashplane_t **)calloc( sizeof( hashplane_t* ), m_iHashPlanes );
	m_srcPlanePool = (hashplane_t *)calloc( sizeof( hashplane_t ), m_iAllocPlanes );
	m_srcPlaneElems = (uint *)calloc( sizeof( uint32_t ), m_iAllocPlanes );
	m_curPlaneElems = m_srcPlaneElems;

	return true;
}

bool CMeshDesc :: StudioConstructMesh( void )
{
	float start_time = Sys_DoubleTime();

	if (!m_pModel || m_pModel->type != mod_studio)
		return false;

	studiohdr_t *phdr = (studiohdr_t *)m_pModel->cache.data;
	if (!phdr) {
		return false;
	}

	if (LoadFromCacheFile())
	{
		if (!FinishMeshBuild())
			return false;

		FreeMeshBuild();
		ALERT(at_aiconsole, "%s: load  time %g secs, size %s\n", m_debugName, Sys_DoubleTime() - start_time, Q_memprint(mesh_size));
		PrintMeshInfo();

		return true;
	}

	if (phdr->numbones < 1) {
		return false;
	}

	// compute default pose for building mesh from
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex);
	mstudioanim_t *panim = (mstudioanim_t *)((byte *)phdr + pseqdesc->animindex);
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static Vector pos[MAXSTUDIOBONES];
	static Vector4D q[MAXSTUDIOBONES];
	int totalVertSize = 0;

	int i;
	for( i = 0; i < phdr->numbones; i++, pbone++, panim++ ) 
	{
		StudioCalcBoneTransform( 0, pbone, panim, pos[i], q[i] );
	}

	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix3x4	transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	transform = matrix3x4( g_vecZero, g_vecZero, Vector( 1.0f, 1.0f, 1.0f ));

	// compute bones for default anim
	for( i = 0; i < phdr->numbones; i++ ) 
	{
		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( pbone[i].parent == -1 ) 
			bonetransform[i] = transform.ConcatTransforms( bonematrix );
		else bonetransform[i] = bonetransform[pbone[i].parent].ConcatTransforms( bonematrix );
	}

	// through all bodies to determine max vertices count
	for( i = 0; i < phdr->numbodyparts; i++ )
	{
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + i;

		int index = m_iBodyNumber / pbodypart->base;
		index = index % pbodypart->nummodels;

		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		totalVertSize += psubmodel->numverts;
	}

	Vector *verts = new Vector[totalVertSize * 8]; // allocate temporary vertices array
	float *coords = new float[totalVertSize * 16]; // allocate temporary texcoords array
	uint *skinrefs = new uint[totalVertSize * 8]; // store material pointers
	unsigned int *indices = new unsigned int[totalVertSize * 24];
	int numVerts = 0, numElems = 0, numTris = 0;
	mvert_t triangle[3];

	for( int k = 0; k < phdr->numbodyparts; k++ )
	{
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + k;

		int index = m_iBodyNumber / pbodypart->base;
		index = index % pbodypart->nummodels;
		int m_skinnum = bound( 0, m_iSkinNumber, MAXSTUDIOSKINS );

		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;
		Vector *pstudioverts = (Vector *)((byte *)phdr + psubmodel->vertindex);
		Vector *m_verts = new Vector[psubmodel->numverts];
		byte *pvertbone = ((byte *)phdr + psubmodel->vertinfoindex);

		// setup all the vertices
		for( i = 0; i < psubmodel->numverts; i++ )
			m_verts[i] = bonetransform[pvertbone[i]].VectorTransform( pstudioverts[i] );

		mstudiotexture_t *ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);
		short *pskinref = (short *)((byte *)phdr + phdr->skinindex);
		if( m_skinnum != 0 && m_skinnum < phdr->numskinfamilies )
			pskinref += (m_skinnum * phdr->numskinref);

		for( int j = 0; j < psubmodel->nummesh; j++ ) 
		{
			mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)phdr + psubmodel->meshindex) + j;
			short *ptricmds = (short *)((byte *)phdr + pmesh->triindex);
			float s = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].width;
			float t = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].height;
			int flags = ptexture[pskinref[pmesh->skinref]].flags;

			while(( i = *( ptricmds++ )))
			{
				int	vertexState = 0;
				bool	tri_strip;

				if( i < 0 )
				{
					tri_strip = false;
					i = -i;
				}
				else tri_strip = true;

				numTris += (i - 2);

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

					verts[numVerts] = m_verts[ptricmds[0]];
					skinrefs[numVerts] = pmesh->skinref;

					if( flags & STUDIO_NF_CHROME )
					{
						// probably always equal 64 (see studiomdl.c for details)
						coords[numVerts*2+0] = s;
						coords[numVerts*2+1] = t;
					}
					else if( flags & STUDIO_NF_UV_COORDS )
					{
						coords[numVerts*2+0] = HalfToFloat( ptricmds[2] );
						coords[numVerts*2+1] = HalfToFloat( ptricmds[3] );
					}
					else
					{
						coords[numVerts*2+0] = ptricmds[2] * s;
						coords[numVerts*2+1] = ptricmds[3] * t;
					}
					numVerts++;
				}
			}
		}

		delete [] m_verts;	// don't keep this because different submodels may have difference count of vertices
	}

	if( numTris != ( numElems / 3 ))
		ALERT( at_error, "StudioConstructMesh: mismatch triangle count (%i should be %i)\n", (numElems / 3), numTris );

	InitMeshBuild( numTris );

	if( m_bShowPacifier )
	{
		if( numTris >= 262144 )
			Msg( "StudioConstructMesh: ^1%s^7\n", m_debugName );
		else if( numTris >= 131072 )
			Msg( "StudioConstructMesh: ^3%s^7\n", m_debugName );
		else Msg( "StudioConstructMesh: ^2%s^7\n", m_debugName );
		StartPacifier();
	}

	for( i = 0; i < numElems; i += 3 )
	{
		// fill the triangle
		triangle[0].point = verts[indices[i+0]];
		triangle[1].point = verts[indices[i+1]];
		triangle[2].point = verts[indices[i+2]];

		triangle[0].st[0] = coords[indices[i+0]*2+0];
		triangle[0].st[1] = coords[indices[i+0]*2+1];
		triangle[1].st[0] = coords[indices[i+1]*2+0];
		triangle[1].st[1] = coords[indices[i+1]*2+1];
		triangle[2].st[0] = coords[indices[i+2]*2+0];
		triangle[2].st[1] = coords[indices[i+2]*2+1];

		// add it to mesh
		AddMeshTrinagle( triangle, skinrefs[indices[i]] );

		if( m_bShowPacifier )
		{
			UpdatePacifier( (float)m_mesh.numfacets / numTris );
		}
	}

	if( m_bShowPacifier )
	{
		float end_time = Sys_DoubleTime();
		EndPacifier( end_time - start_time );
	}

	delete [] skinrefs;
	delete [] coords;
	delete [] verts;
	delete [] indices;

	if( !FinishMeshBuild( ))
		return false;

	SaveToCacheFile();
	FreeMeshBuild();

	if( !m_bShowPacifier )
	{
		ALERT( at_console, "%s: CLIP build time %g secs\n", m_debugName, Sys_DoubleTime() - start_time );
		PrintMeshInfo();
	}

	// done
	return true;
}

bool CMeshDesc :: FinishMeshBuild( void )
{
	if( m_mesh.numfacets <= 0 )
	{
		ALERT( at_aiconsole, "%s: failed to build triangle mesh\n", m_debugName );
		FreeMesh();
		return false;
	}

	int i;
	for( i = 0; i < 3; i++ )
	{
		// spread the mins / maxs by a pixel
		m_mesh.mins[i] -= 1.0f;
		m_mesh.maxs[i] += 1.0f;
	}

	size_t memsize = (sizeof( mfacet_t ) * m_mesh.numfacets) + (sizeof( mplane_t ) * m_mesh.numplanes) + (sizeof( uint32_t ) * m_iTotalPlanes);
	mesh_size = sizeof( m_mesh ) + memsize;

	// create non-fragmented memory piece and move mesh
	byte *buffer = (byte *)Mem_Alloc( memsize );
	byte *bufend = buffer + memsize;

	// setup pointers
	m_mesh.planes = (mplane_t *)buffer; // so we free mem with planes
	buffer += (sizeof( mplane_t ) * m_mesh.numplanes);
	m_mesh.facets = (mfacet_t *)buffer;
	buffer += (sizeof( mfacet_t ) * m_mesh.numfacets);

	// setup mesh pointers
	for( i = 0; i < m_mesh.numfacets; i++ )
	{
		m_mesh.facets[i].indices = (uint *)buffer;
		buffer += (sizeof( uint32_t ) * m_srcFacets[i].numplanes);
	}

	if( buffer != bufend )
		ALERT( at_error, "FinishMeshBuild: memory representation error! %x != %x\n", buffer, bufend );

	// re-use pointer
	m_curPlaneElems = m_srcPlaneElems;

	// copy planes into mesh array (probably aligned block)
	for( i = 0; i < m_mesh.numplanes; i++ )
		m_mesh.planes[i] = m_srcPlanePool[i].pl;

	// copy planes into mesh array (probably aligned block)
	for( i = 0; i < m_mesh.numfacets; i++ )
	{
		m_mesh.facets[i].mins = m_srcFacets[i].mins;
		m_mesh.facets[i].maxs = m_srcFacets[i].maxs;
		m_mesh.facets[i].edge1 = m_srcFacets[i].edge1;
		m_mesh.facets[i].edge2 = m_srcFacets[i].edge2;
		m_mesh.facets[i].area.next = m_mesh.facets[i].area.prev = NULL;
		m_mesh.facets[i].numplanes = m_srcFacets[i].numplanes;
		m_mesh.facets[i].skinref = m_srcFacets[i].skinref;

		for( int j = 0; j < m_srcFacets[i].numplanes; j++ )
			m_mesh.facets[i].indices[j] = m_curPlaneElems[j];
		m_curPlaneElems += m_srcFacets[i].numplanes;

		for( int k = 0; k < 3; k++ )
			m_mesh.facets[i].triangle[k] = m_srcFacets[i].triangle[k];
	}

	if( has_tree )
	{
		// create tree
		CreateAreaNode( 0, m_mesh.mins, m_mesh.maxs );

		for( i = 0; i < m_mesh.numfacets; i++ )
			RelinkFacet( &m_mesh.facets[i] );
	}

	m_bMeshBuilt = true;
	return true;
}

void CMeshDesc :: PrintMeshInfo( void )
{
#if 0	// g-cont. just not needs
	ALERT( at_console, "FinishMesh: %s %s", m_debugName, Q_memprint( mesh_size ));
	ALERT( at_console, " (planes reduced from %i to %i)", m_iTotalPlanes, m_mesh.numplanes );
	ALERT( at_console, "\n" );
#endif
}

void CMeshDesc :: FreeMeshBuild( void )
{
	// no reason to keep these arrays
	free( m_srcPlaneElems );
	free( m_srcPlaneHash );
	free( m_srcPlanePool );
	free( m_srcFacets );

	m_srcPlaneElems = NULL;
	m_curPlaneElems = NULL;
	m_srcPlaneHash = NULL;
	m_srcPlanePool = NULL;
	m_srcFacets = NULL;
}

void CMeshDesc::SaveToCacheFile()
{
	if (m_pModel->type == mod_studio)
	{
		// only studio models can be cached
		// now dump the collision into cachefile
		if (CacheFileExists()) {
			StudioSaveCache();
		}
		else {
			StudioCreateCache();
		}
	}
}

bool CMeshDesc::LoadFromCacheFile()
{
	LoadStatus status = StudioLoadCache();
	if (status == LoadStatus::Error && CacheFileExists()) 
	{
		// remove legacy/incompatible cache file
		std::string filePath;
		GetCacheFilePath(filePath);
		fs::RemoveFile(filePath);
	}
	return status == LoadStatus::Success;
}

bool CMeshDesc::PresentInCache() const
{
	clipfile::Header hdr;
	clipfile::CacheTable table;
	std::vector<clipfile::CacheEntry> entriesList;
	const clipfile::CacheEntry *desiredEntry = nullptr;

	if (m_pModel->type != mod_studio) {
		return false; // only studio model can be cached
	}

	std::string fileName = m_pModel->name;
	fileName.erase(0, fileName.find_first_of("/\\") + 1);
	fileName.erase(fileName.find_last_of("."));

	fs::Path filePath = "cache/" + fileName + ".clip";
	fs::File cacheFile(filePath, "rb");
	if (!fs::FileExists(filePath) || !cacheFile.IsOpen()) {
		return false;
	}

	// read header
	cacheFile.Read(&hdr, sizeof(hdr));

	// read cache entries table
	uint32_t entriesCount;
	cacheFile.Seek(hdr.tableOffset, fs::SeekType::Set);
	cacheFile.Read(&entriesCount, sizeof(entriesCount));

	// read cache entries list
	entriesList.reserve(entriesCount);
	for (size_t i = 0; i < entriesCount; i++)
	{
		auto &entry = entriesList.emplace_back();
		cacheFile.Read(&entry, sizeof(table.entries));
	}
	cacheFile.Close();

	for (size_t i = 0; i < entriesCount; i++)
	{
		const clipfile::CacheEntry &entry = entriesList[i];
		if (entry.body == m_iBodyNumber && entry.skin == m_iSkinNumber && entry.geometryType == m_iGeometryType) {
			return true;
		}
	}
	return false;
}
