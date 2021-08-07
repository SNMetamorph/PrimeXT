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

#include "studio.h"
#include "areanode.h"

#define MAX_AREA_DEPTH	5
#define MAX_AREANODES	BIT( MAX_AREA_DEPTH + 1 )

#define MAX_FACET_PLANES	32		// can be increased up to 255
#define MAX_TRIANGLES	524288		// studio triangles
#define PLANE_HASHES	m_iHashPlanes
#define PACIFIER_STEP	40
#define PACIFIER_REM	( PACIFIER_STEP / 10 )

typedef struct hashplane_s
{
	mplane_t		pl;
	struct hashplane_s	*hash;
} hashplane_t;

typedef struct mvert_s
{
	Vector		point;
	float		st[2];			// for alpha-texture test
} mvert_t;

typedef struct mfacet_s
{
	link_t		area;			// linked to a division node or leaf
	int		skinref;			// pointer to texture for special effects
	mvert_t		triangle[3];		// store triangle points
	Vector		mins, maxs;		// an individual size of each facet
	vec3_t		edge1, edge2;		// new trace stuff
	byte		numplanes;		// because numplanes for each facet can't exceeds MAX_FACET_PLANES!
	uint		*indices;			// a indexes into mesh plane pool
} mfacet_t;

typedef struct
{
	Vector		mins, maxs;
	int		numfacets;
	int		numplanes;
	mfacet_t		*facets;
	mplane_t		*planes;			// shared plane pool
} mmesh_t;

class CMeshDesc
{
private:
	mmesh_t		m_mesh;
	const char	*m_debugName;		// just for debug purpoces
	areanode_t	areanodes[MAX_AREANODES];	// AABB tree for speedup trace test
	int		numareanodes;
	bool		has_tree;			// build AABB tree
	int		m_iTotalPlanes;		// just for stats
	int		m_iAllocPlanes;		// allocated count of planes
	int		m_iHashPlanes;		// total count of hashplanes
	int		m_iNumTris;		// if > 0 we are in build mode
	size_t		mesh_size;		// mesh total size
	model_t		*m_pModel;		// parent model pointer

	// used only while mesh is constructing
	mfacet_t		*m_srcFacets;
	hashplane_t	**m_srcPlaneHash;
	hashplane_t	*m_srcPlanePool;
	uint		*m_srcPlaneElems;
	uint		*m_curPlaneElems;		// sliding pointer

	// pacifier stuff
	bool		m_bShowPacifier;
	int		m_iOldPercent;
public:
	CMeshDesc();
	~CMeshDesc();

	// mesh construction
	bool InitMeshBuild( int numTrinagles ); 
	bool AddMeshTrinagle( const mvert_t triangle[3], int skinref );
	bool FinishMeshBuild( void );
	void FreeMeshBuild( void );
	void FreeMesh( void );

	void SetDebugName( const char *name ) { m_debugName = name; }
	void SetModel( const model_t *mod ) { m_pModel = (model_t *)mod; }
	void PrintMeshInfo( void );

	// pacifier stuff
	void StartPacifier( void );
	void UpdatePacifier( float percent );
	void EndPacifier( float total );

	// studio models processing
	void ExtractAnimValue( int frame, mstudioanim_t *panim, int dof, float scale, float &v1 );
	void StudioCalcBoneTransform( int frame, mstudiobone_t *pbone, mstudioanim_t *panim, Vector &pos, Vector4D &q );
	bool StudioLoadCache( const char *pCacheName );
	bool StudioSaveCache( const char *pCacheName );
	bool StudioConstructMesh( void );

	// linked list operations
	void InsertLinkBefore( link_t *l, link_t *before );
	void RemoveLink( link_t *l );
	void ClearLink( link_t *l );

	// AABB tree contsruction
	areanode_t *CreateAreaNode( int depth, const Vector &mins, const Vector &maxs );
	void RelinkFacet( mfacet_t *facet );
	areanode_t *GetHeadNode( void ) { return (has_tree) ? areanodes : NULL; }

	// plane cache
	int PlaneFromPoints( const Vector &p0, const Vector &p1, const Vector &p2 );
	int FindFloatPlane( const Vector &normal, float dist );
	int CreateNewFloatPlane( const Vector &srcnormal, float dist, int hash );
	bool PlaneEqual( const mplane_t *p, const Vector &normal, float dist );
	int PlaneTypeForNormal( const Vector &normal );
	int SnapNormal( Vector &normal );

	// get cached collision
	mmesh_t *GetMesh() { return (mesh_size) ? &m_mesh : NULL; } 
};

CMeshDesc *UTIL_GetCollisionMesh( int modelindex );

#endif//MESHDESC_H