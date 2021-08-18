/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// bsp5.h

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "threads.h"
#include "polylib.h"
#include "stringlib.h"
#include "filesystem.h"

#include <windows.h>

#ifndef DOUBLEVEC_T
#error you must add -dDOUBLEVEC_T to the project!
#endif

#define DEFAULT_SUBDIVIDE_SIZE 	((MAX_SURFACE_EXTENT - 1) * TEXTURE_STEP)
#define DEFAULT_MAXNODE_SIZE		-1
#define DEFAULT_MERGE_LEVEL		1
#define DEFAULT_NOFILL		false
#define DEFAULT_NOTJUNC		false
#define DEFAULT_NOCLIP		false
#define DEFAULT_FORCEVIS		true
#define BSPCHOP_EPSILON		( Q_min( g_prtepsilon * 4.0, ON_EPSILON ))

// the exact bounding box of the brushes is expanded some for the headnode
// volume.  is this still needed?
#define SIDESPACE			24

// !!! if this is changed, it must be changed in csg.h too !!!
typedef struct
{
	vec3_t		normal;
	vec3_t		origin;		// unused
	vec_t		dist;
	int		type;
	int		outplanenum;	// mapped planenum (in CSG it was called a hash_chain)
} plane_t;

//============================================================================
typedef enum
{
	face_normal = 0,
	face_hint,
	face_discardable,			// contents must not differ between front and back
} facestyle_e;

typedef struct face_s
{
	struct face_s	*next;
	
	int		planenum;
	int		texturenum;
	int		contents;		// contents in front of face

	struct face_s	*original;	// face on node
	int		outputnumber;	// only valid for original faces after
	int		referenced;	// only valid for original faces
	facestyle_e	facestyle;
	int		bite;

	int		detaillevel;	// defined by hlcsg
	int		*outputedges;	// used in WriteDrawNodes
	int		numedges;		// outcount
					// write surfaces
	winding_t		*w;
} face_t;


typedef struct surface_s
{
	struct surface_s	*next;
	int		planenum;
	vec3_t		mins, maxs;
	struct node_s	*onnode;		// true if surface has already been used
					// as a splitting node
	int		detaillevel;	// minimum detail level of its faces
	face_t		*faces;		// links to all the faces on either side of the surf
} surface_t;

// detail brushes stuff
typedef struct side_s
{
	struct side_s	*next;
	plane_t		plane;		// facing inside (reversed when loading brush file)
	winding_t		*w;		// (also reversed)
} side_t;

typedef struct brush_s
{
	struct brush_s	*next;
	side_t		*sides;
} brush_t;

//
// there is a node_t structure for every node and leaf in the bsp tree
//
#define PLANENUM_LEAF	-1
#define BOUNDS_EXPANSION	1.0

#define FNODE_DETAIL	(1<<0)		// is under a diskleaf
#define FNODE_LEAFPORTAL	(1<<1)		// not detail and children are detail; only visleafs have contents, portals, mins, maxs
#define FNODE_DETAILCONTENTS	(1<<2)		// inside a detail brush
#define FNODE_EMPTY		(1<<3)		// not solid or sky

typedef struct node_s
{
	surface_t		*surfaces;
	brush_t		*detailbrushes;

	brush_t		*boundsbrush;
	vec3_t		loosemins;	// all leafs and nodes have this, while 'mins' and 'maxs'
	vec3_t		loosemaxs;	// are only valid for nondetail leafs and nodes.

	int		flags;
	vec3_t		mins, maxs;	// bounding volume of portals;

	// information for decision nodes	
	int		planenum;		// -1 = leaf node	
	int		firstface;	// decision node only
	int		numfaces;		// decision node only
	struct node_s	*children[2];	// only valid for decision nodes
	face_t		*faces;		// decision nodes only, list for both sides
	
	// information for leafs
	int		contents;		// leaf nodes (0 for decision nodes)
	face_t		**markfaces;	// leaf nodes only, point to node faces
	struct portal_s	*portals;
	int		visleafnum;	// -1 = solid
	int		valid;		// for flood filling
	int		occupied;		// light number in leaf for outside filling

	bool		opaque() const	{ return contents == CONTENTS_SOLID || contents == CONTENTS_SKY; }
} node_t;

typedef struct portal_s
{
	int		planenum;
	node_t		*onnode;		// NULL = outside box
	node_t		*nodes[2];	// [0] = front side of plane
	struct portal_s	*next[2];	
	winding_t		*winding;
} portal_t;

typedef struct
{
	vec3_t		mins, maxs;	// bounding box
	face_t		*validfaces[MAX_INTERNAL_MAP_PLANES];	// build surfaces is also used by GatherNodeFaces
	brush_t		*detailbrushes;
	surface_t		*surfaces;
	node_t		*headnode;
	int		numsurfaces;	// surface_t[count]
	int		numfaces;		// debug
} tree_t;

face_t *NewFaceFromFace (face_t *in);
void SplitFace( face_t *in, plane_t *split, face_t **front, face_t **back, bool keepsource = false );
void SplitFaceEpsilon( face_t *in, plane_t *split, face_t **front, face_t **back, vec_t epsilon, bool keepsource = false );

//=============================================================================

// solidbsp.c

void DivideFacet( face_t *in, plane_t *split, face_t **front, face_t **back );
void CalcSurfaceInfo( surface_t *surf );
void SubdivideFace( face_t *f, face_t **prevptr );
void SolidBSP( tree_t *tree, int modnum, int hullnum );
vec_t SplitPlaneMetric( const plane_t *p, const vec3_t mins, const vec3_t maxs );
void MakeNodePortal( node_t *node );
void SplitNodePortals( node_t *node );
void FreeLeafSurfs( node_t *leaf );

//=============================================================================

// merge.c

void MergePlaneFaces( surface_t *plane, int mergelevel );
face_t *FreeMergeListScraps (face_t *merged);
void MergeTreeFaces( tree_t *tree );

//=============================================================================

// surfaces.c

extern	int		firstmodeledge;

void SubdivideFaces (surface_t *surfhead);
int GetEdge (vec3_t p1, vec3_t p2, face_t *f);
void GatherTreeFaces( tree_t *tree );
void MakeFaceEdges( void );
void InitHash( void );

//=============================================================================

// portals.c

void AddPortalToNodes( portal_t *p, node_t *front, node_t *back );
void RemovePortalFromNode( portal_t *portal, node_t *l );
void MakeHeadnodePortals( node_t *node, const vec3_t mins, const vec3_t maxs );
void FreeTreePortals( tree_t *tree );
void WritePortalfile( tree_t *tree, bool leaked );
void FillInside( node_t *node );

//=============================================================================

// detail.c

brush_t *ReadBrushes( FILE *file );

//=============================================================================

// partition.c

surface_t *SelectPartition( surface_t *surfaces, node_t *node, bool midsplit, int splitdetaillevel, vec3_t validmins, vec3_t validmaxs );

// tree.c

tree_t *AllocTree( void );
void FreeTree( tree_t *t );
tree_t *MakeTreeFromHullFaces( FILE *polyfile, FILE *brushfile );
tree_t *TreeProcessModel( tree_t *tree, int modnum, int hullnum );
void MakeSurflistFromValidFaces( tree_t *tree );
bool CheckFaceForHint( const face_t *f );
bool CheckFaceForDiscardable( const face_t *f );

//=============================================================================

// tjunc.c

void tjunc( node_t *headnode, bool worldmodel );

//=============================================================================

// writebsp.c

void WriteNodePlanes (node_t *headnode);
void WriteClipNodes (node_t *headnode);
void WriteDrawNodes (node_t *headnode);

void EmitDrawNodes( tree_t *tree );
void EmitClipNodes( tree_t *tree, int modnum, int hullnum );
void EmitNodeFaces( node_t *headnode );
int EmitVertex( const vec3_t point );
void BeginBSPFile( void );
void FinishBSPFile( const char *source );
int FindFloatPlane( const vec3_t normal, vec_t dist );

//=============================================================================

// outside.c

bool FillOutside( tree_t *tree, int hullnum, bool leakfile );

//=============================================================================

extern bool	g_nofill;
extern bool	g_notjunc;
extern bool	g_noclip;
extern bool	g_forcevis;

extern int	g_maxnode_size;
extern int	g_merge_level;
extern vec_t	g_prtepsilon;

extern int	valid;
extern int	c_splitnodes;
extern int	c_unsplitted_faces;

extern char	g_portfilename[1024];
extern char	g_pointfilename[1024];
extern char	g_linefilename[1024];

extern plane_t	g_mapplanes[MAX_INTERNAL_MAP_PLANES];
extern int	g_nummapplanes;
extern node_t	g_outside_node;

void AddFaceToBounds( face_t *f, vec3_t mins, vec3_t maxs );

// misc functions

face_t *AllocFace( void );
void FreeFace( face_t *f );
void UnlinkFace( face_t **head, face_t *face );
int CountListFaces( face_t *list );

struct portal_s *AllocPortal( void );
void FreePortal( struct portal_s *p );

surface_t *AllocSurface( void );
void FreeSurface( surface_t *s );

side_t *AllocSide( void );
void FreeSide( side_t *s );
side_t *NewSideFromSide( const side_t *s );
brush_t *AllocBrush( void );
void FreeBrush( brush_t *b );
brush_t *NewBrushFromBrush( const brush_t *b );
void SplitBrush( brush_t *in, const plane_t *split, brush_t **front, brush_t **back );
brush_t *BrushFromBox( const vec3_t mins, const vec3_t maxs );
void CalcBrushBounds( const brush_t *b, vec3_t &mins, vec3_t &maxs );

node_t *AllocNode( void );
void FreeNode( node_t *n );
void FreeLeaf( node_t *n );

//=============================================================================
