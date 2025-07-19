/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#ifdef HLRAD_RAYTRACE
#ifndef RAYTRACER_H
#define RAYTRACER_H

#include <assert.h>
#include "cmdlib.h"
#include "mathlib.h"
#include <utlarray.h>

struct tface_t;
struct trace_t;

#define KDNODE_STATE_XSPLIT		0	// this node is an x split
#define KDNODE_STATE_YSPLIT		1	// this node is a ysplit
#define KDNODE_STATE_ZSPLIT		2	// this node is a zsplit
#define KDNODE_STATE_LEAF		3	// this node is a leaf

#define NEVER_SPLIT			0	// set to 1 to test tree efficiency

#define MAILBOX_HASH_SIZE		256
#define MAX_TREE_DEPTH		21
#define MAX_NODE_STACK_LEN		(40 * MAX_TREE_DEPTH)

#define COST_OF_TRAVERSAL		75	// approximate #operations
#define COST_OF_INTERSECTION		167	// approximate #operations

struct KDNode
{
	// this is the cache intensive data structure. "Tricks" are used to fit it into 8 bytes:
	//
	// A) the right child is always stored after the left child, which means we only need one pointer
	// B) The type of node (KDNODE_xx) is stored in the lower 2 bits of the pointer.
	// C) for leaf nodes, we store the number of triangles in the leaf in the same place as the floating
	//    point splitting parameter is stored in a non-leaf node

	int	m_iChild;			// child idx, or'ed with flags above
	float	m_flSplitValue;		// for non-leaf nodes, the nodes on the
					// "high" side of the splitting plane
					// are on the right
	inline KDNode()
	{
		m_flSplitValue = 0.0f;
		m_iChild = 0;
	}

	inline int NodeType( void ) const
	{
		return m_iChild & 3;
	}

	inline int TriangleIndexStart( void ) const
	{
		assert( NodeType() == KDNODE_STATE_LEAF );
		return m_iChild >> 2;
	}

	inline int LeftChild( void ) const
	{
		assert( NodeType() != KDNODE_STATE_LEAF );
		return m_iChild >> 2;
	}

	inline int RightChild( void ) const
	{
		return LeftChild() + 1;
	}

	inline int NumberOfTrianglesInLeaf( void ) const
	{
		assert( NodeType() == KDNODE_STATE_LEAF );
		return *((int32 *)&m_flSplitValue);
	}

	inline void SetNumberOfTrianglesInLeafNode( int n )
	{
		*((int32 *)&m_flSplitValue) = n;
	}
};

struct NodeToVisit
{
	KDNode const *node;
	vec_t p1f;
	vec_t p2f;
};

class CWorldRayTrace
{
private:
	vec3_t			m_AbsMins;
	vec3_t			m_AbsMaxs;
	float			m_flBackFrac;	// to prevent light leaks
	tmesh_t			*mesh;		// pointer to mesh

	CUtlArray<KDNode>		m_KDTree;		//< the packed kdtree. root is 0
	CUtlBlockVector<tface_t*>	m_TriangleList;	//< the packed triangles
	CUtlArray<int>		m_TriangleIndexList;//< the list of triangle indices.
public:
	CWorldRayTrace()
	{
		m_KDTree.Purge();
		m_TriangleList.Purge();
		m_TriangleIndexList.Purge();
		ClearBounds( m_AbsMins, m_AbsMaxs );
		m_flBackFrac = 0.0f;
		mesh = NULL;
	}

	// make pointers to model triangles
	void AddTriangle( tface_t *tf );

	void SetBackFraction( float back ) { m_flBackFrac = back * 100.0f; }
	// SetupAccelerationStructure to prepare for tracing
	void BuildTree( tmesh_t *src );

	void TraceRay( const vec3_t start, const vec3_t stop, trace_t *trace );
private:
	// lowest level intersection routine - fire 4 rays through the scene. all 4 rays must pass the
	// Check() function, and t extents must be initialized. skipid can be set to exclude a
	// particular id (such as the origin surface). This function finds the closest intersection.
	void TraceRay( vec_t p1f, vec_t p2f, const vec3_t start, const vec3_t direction, trace_t *trace );

	bool TraceTexture( const tface_t *face, float u, float v, float w );

	int MakeLeafNode( int first_tri, int last_tri );

	float CalculateCostsOfSplit( int plane, int *list, int ntris, vec3_t min, vec3_t max, float &value, int &nleft, int &nright, int &nboth );
		
	void RefineNode( int node_number, int *list, int ntris, vec3_t absmin, vec3_t absmax, int depth );
	
	void CalculateTriangleListBounds( const int *tris, int ntris, vec3_t minout, vec3_t maxout );

	float BoxSurfaceArea( const vec3_t boxmin, const vec3_t boxmax );
};



#define BVH_EXIT_NODE		INT_MAX

struct bbox_t
{
	vec3_t	min, max;	
};

struct bvhnode_t
{
	int		start_id, end_id;
	int 	hit, miss;
	bbox_t	bbox;
};

class CWorldRayTraceBVH
{
private:
	bvhnode_t	*nodes;
	tmesh_t		*mesh;	
public:
	CWorldRayTraceBVH()
	{
		nodes = NULL;
		mesh = NULL;
	}

	void BuildTree( tmesh_t *src );

	void TraceRay( const vec3_t start, const vec3_t stop, trace_t *trace, bool stop_on_first_intersection = false  );
private:
	float SAH( const bbox_t box );
	bool TraceTexture( const tface_t *face, float u, float v, float w );	
};

#endif//RAYTRACER_H
#endif//HLRAD_RAYTRACE