/*
meshdesc.h - cached mesh for tracing custom objects
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

#ifndef MESHDESC_H
#define MESHDESC_H

#include "const.h"
#include "vector.h"
#include "com_model.h"
#include "physint.h"

#define AREA_NODES			32
#define AREA_DEPTH			4

typedef struct
{
	Vector	mins, maxs;
	link_t	area;				// linked to a division node or leaf
	int		skinref;			// pointer to texture for special effects
	int		numplanes;
	mplane_t *planes;
} mfacet_t;

typedef struct
{
	Vector	mins, maxs;
	int	numfacets;
	mfacet_t	*facets;
} mmesh_t;

typedef struct
{
	int	count;
	int	maxcount;
	bool	overflowed;
	short	*list;
	Vector	mins, maxs;
	int	topnode;				// for overflows where each leaf can't be stored individually
} leaflist_t;

class CMeshDesc
{
private:
	mmesh_t		m_mesh;
	const char	*m_debugName;		// just for debug purpoces
	int		m_iTotalPlanes;		// just for stats
	int		m_iNumTris;		// if > 0 we are in build mode
	Vector		m_origin, m_angles;		// cached values to compare with
	areanode_t	areanodes[AREA_NODES];	// AABB tree for speedup trace test
	int		numareanodes;
	bool		has_tree;			// build AABB tree
public:
	CMeshDesc();
	~CMeshDesc();

	// mesh construction
	bool InitMeshBuild( const char *debug_name, int numTrinagles ); 
	bool AddMeshTrinagle( const Vector triangle[3], int skinref = 0 );
	bool FinishMeshBuild( void );
	void FreeMesh( void );

	// linked list operations
	void InsertLinkBefore( link_t *l, link_t *before );
	void RemoveLink( link_t *l );
	void ClearLink( link_t *l );

	// AABB tree contsruction
	areanode_t *CreateAreaNode( int depth, const Vector &mins, const Vector &maxs );
	void RelinkFacet( mfacet_t *facet );
	_inline areanode_t *GetHeadNode( void ) { return (has_tree) ? &areanodes[0] : NULL; }

	// check for cache
	mmesh_t *CheckMesh( const Vector &origin, const Vector &angles );
	_inline mmesh_t *GetMesh() { return &m_mesh; } 
};

#endif//MESHDESC_H
