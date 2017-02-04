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

#include	"extdll.h"
#include  "util.h"
#include	"cbase.h"
#include	"com_model.h"
#include  "meshdesc.h"
#include  "trace.h"

CMeshDesc :: CMeshDesc( void )
{
	m_origin = m_angles = g_vecZero;
	m_mesh.numfacets = 0;
	m_mesh.facets = NULL;
	m_debugName = NULL;
	m_iNumTris = 0;
}

CMeshDesc :: ~CMeshDesc( void )
{
	FreeMesh ();
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
	
	if( depth == AREA_DEPTH )
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
	if( m_mesh.numfacets <= 0 )
		return;

	// free all allocated memory by this mesh
	for( int i = 0; i < m_mesh.numfacets; i++ )
		delete [] m_mesh.facets[i].planes;
	delete [] m_mesh.facets;

	m_mesh.facets = NULL;
	m_mesh.numfacets = 0;
}

bool CMeshDesc :: InitMeshBuild( const char *debug_name, int numTriangles )
{
	if( numTriangles <= 0 )
		return false;

	// perfomance warning
	if( numTriangles >= 16384 )
		ALERT( at_warning, "%s have too many triangles (%i)\n", debug_name, numTriangles );

	if( numTriangles >= 256 )
		has_tree = true;	// too many triangles invoke to build AABB tree
	else has_tree = false;

	ClearBounds( m_mesh.mins, m_mesh.maxs );

	memset( areanodes, 0, sizeof( areanodes ));
	numareanodes = 0;

	m_debugName = debug_name;
	m_mesh.facets = (mfacet_t *)calloc( sizeof( mfacet_t ), numTriangles );
	m_iNumTris = numTriangles;
	m_iTotalPlanes = 0;

	return true;
}

bool CMeshDesc :: AddMeshTrinagle( const Vector triangle[3] )
{
	int	i;

	if( m_iNumTris <= 0 )
		return false; // were not in a build mode!

	if( m_mesh.numfacets >= m_iNumTris )
	{
		ALERT( at_error, "AddMeshTriangle: %s overflow (%i >= %i)\n", m_debugName, m_mesh.numfacets, m_iNumTris );
		return false;
	}

	// add triangle to bounds
	for( i = 0; i < 3; i++ )
		AddPointToBounds( triangle[i], m_mesh.mins, m_mesh.maxs );

	mfacet_t *facet = &m_mesh.facets[m_mesh.numfacets];
	mplane_t mainplane;

	// calculate plane for this triangle
	PlaneFromPoints( triangle, &mainplane );

	if( ComparePlanes( &mainplane, g_vecZero, 0.0f ))
		return false; // bad plane

	mplane_t planes[32];
	Vector normal;
	int numplanes;
	float dist;

	facet->numplanes = numplanes = 0;

	// add front plane
	SnapPlaneToGrid( &mainplane );

	planes[numplanes].normal = mainplane.normal;
	planes[numplanes].dist = mainplane.dist;
	numplanes++;

	// calculate mins & maxs
	ClearBounds( facet->mins, facet->maxs );

	for( i = 0; i < 3; i++ )
		AddPointToBounds( triangle[i], facet->mins, facet->maxs );

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
		}
	}

	// add the edge bevels
	for( i = 0; i < 3; i++ )
	{
		int j = (i + 1) % 3;
		int k = (i + 2) % 3;

		Vector vec = triangle[i] - triangle[j];
		if( vec.Length() < 0.5f ) continue;

		vec = vec.Normalize();
		SnapVectorToGrid( vec );

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
				dist = DotProduct( triangle[i], normal );

				for( j = 0; j < numplanes; j++ )
				{
					// if this plane has already been used, skip it
					if( ComparePlanes( &planes[j], normal, dist ))
						break;
				}

				if( j != numplanes ) continue;

				// if all other points are behind this plane, it is a proper edge bevel
				for( j = 0; j < 3; j++ )
				{
					if( j != i )
					{
						float d = DotProduct( triangle[j], normal ) - dist;
						// point in front: this plane isn't part of the outer hull
						if( d > 0.1f ) break;
					}
				}

				if( j != 3 ) continue;

				// add this plane
				planes[numplanes].normal = normal;
				planes[numplanes].dist = dist;
				numplanes++;
			}
		}
	}

	facet->planes = new mplane_t[numplanes];
	facet->numplanes = numplanes;

	for( i = 0; i < facet->numplanes; i++ )
	{
		facet->planes[i] = planes[i];
		SnapPlaneToGrid( &facet->planes[i] );
		CategorizePlane( &facet->planes[i] );
	}

	for( i = 0; i < 3; i++ )
	{
		// spread the mins / maxs by a pixel
		facet->mins[i] -= 1.0f;
		facet->maxs[i] += 1.0f;
	}

	// added
	m_mesh.numfacets++;
	m_iTotalPlanes += numplanes;

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

bool CMeshDesc :: FinishMeshBuild( void )
{
	if( m_mesh.numfacets <= 0 )
	{
		FreeMesh();
		ALERT( at_error, "FinishMeshBuild: failed to build triangle mesh (no sides)\n" );
		return false;
	}

	for( int i = 0; i < 3; i++ )
	{
		// spread the mins / maxs by a pixel
		m_mesh.mins[i] -= 1.0f;
		m_mesh.maxs[i] += 1.0f;
	}

	if( has_tree )
	{
		// create tree
		CreateAreaNode( 0, m_mesh.mins, m_mesh.maxs );

		for( int i = 0; i < m_mesh.numfacets; i++ )
			RelinkFacet( &m_mesh.facets[i] );
	}

#if 0
	size_t size = sizeof( m_mesh ) + ( m_mesh.numfacets * sizeof( mfacet_t )) + ( m_iTotalPlanes * sizeof( mplane_t ));
	ALERT( at_aiconsole, "FinishMeshBuild: %s %i k\n", m_debugName, ( size / 1024 ));
#endif
	return true;
}

mmesh_t *CMeshDesc :: CheckMesh( const Vector &origin, const Vector &angles )
{
	if( origin == m_origin && angles == m_angles )
		return &m_mesh;

	// release old copy
	FreeMesh ();

	// position are changed. Cache new values and rebuild mesh
	m_origin = origin;
	m_angles = angles;

	return NULL;
}
