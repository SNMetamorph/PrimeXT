/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// solidbsp.c

#include "bsp5.h"
#include <limits.h>

/*

  Each node or leaf will have a set of portals that completely enclose
  the volume of the node and pass into an adjacent node.

*/

int		c_leaffaces;
int		c_nodefaces;
int		c_splitnodes;
int		c_clipped_portals;

//============================================================================
static bool	g_report_progress = false;
static face_t	*markfaces[MAX_MAP_MARKSURFACES + 1];
static int	dispatch_tree_faces;
static int	total_tree_faces;

/*
==================
Split a bounding box by a plane; The front and back bounds returned
are such that they completely contain the portion of the input box
on that side of the plane. Therefore, if the split plane is
non-axial, then the returned bounds will overlap.
==================
*/
static void DivideBounds( const vec3_t mins, const vec3_t maxs, const plane_t *split, vec3_t fmins, vec3_t fmaxs, vec3_t bmins, vec3_t bmaxs )
{
	vec_t		dist1, dist2, mid;
	vec_t		split_mins, split_maxs;
	int		a, b, c, i, j;
	const vec_t	*bounds[2];
	vec3_t		corner;

	VectorCopy( mins, fmins );
	VectorCopy( mins, bmins );
	VectorCopy( maxs, fmaxs );
	VectorCopy( maxs, bmaxs );

	if( split->type < PLANE_NONAXIAL )
	{
		// axial split is easy
		fmins[split->type] = bmaxs[split->type] = split->dist;
		return;
	}

	// make proper sloping cuts...
	bounds[0] = mins;
	bounds[1] = maxs;

	for( a = 0; a < 3; a++ )
	{
		// check for parallel case... no intersection
		if( fabs( split->normal[a] ) < NORMAL_EPSILON )
			continue;

		b = (a + 1) % 3;
		c = (a + 2) % 3;

		split_mins = maxs[a];
		split_maxs = mins[a];

		for( i = 0; i < 2; i++ )
		{
			corner[b] = bounds[i][b];

			for( j = 0; j < 2; j++ )
			{
				corner[c] = bounds[j][c];
				corner[a] = bounds[0][a];

				dist1 = DotProduct( corner, split->normal ) - split->dist;

				corner[a] = bounds[1][a];
				dist2 = DotProduct( corner, split->normal ) - split->dist;

				mid = bounds[1][a] - bounds[0][a];
				mid *= ( dist1 / ( dist1 - dist2 ));
				mid += bounds[0][a];

				split_mins = bound( mins[a], split_mins, mid );
				split_maxs = bound( mid, split_maxs, maxs[a] );
			}
		}

		if( split->normal[a] > 0 )
		{
			fmins[a] = split_mins;
			bmaxs[a] = split_maxs;
		}
		else
		{
			bmins[a] = split_mins;
			fmaxs[a] = split_maxs;
		}
	}
}

vec_t SplitPlaneMetric( const plane_t *p, const vec3_t mins, const vec3_t maxs )
{
	vec_t	value = 0.0;
	vec_t	dist;

	if( p->type < PLANE_NONAXIAL )
	{
		dist = p->dist * p->normal[p->type];
 
		for( int i = 0; i < 3; i++ )
		{
			if( i == p->type )
			{
				value += (maxs[i] - dist) * (maxs[i] - dist);
				value += (dist - mins[i]) * (dist - mins[i]);
			}
			else value += 2 * (maxs[i] - mins[i]) * (maxs[i] - mins[i]);
		}
	}
	else
	{
		vec3_t	fmins, fmaxs, bmins, bmaxs;
 
		DivideBounds( mins, maxs, p, fmins, fmaxs, bmins, bmaxs );

		for( int i = 0; i < 3; i++ )
		{
			value += (fmaxs[i] - fmins[i]) * (fmaxs[i] - fmins[i]);
			value += (bmaxs[i] - bmins[i]) * (bmaxs[i] - bmins[i]);
		}
	}

	return value;
}

//============================================================================

/*
=================
CalcSurfaceInfo

Calculates the bounding box
=================
*/
void CalcSurfaceInfo( surface_t *surf )
{
	if( !surf->faces )
		COM_FatalError( "CalcSurfaceInfo: surface without a face\n" );
		
	// calculate a bounding box
	ClearBounds( surf->mins, surf->maxs );

	surf->detaillevel = -1;

	for( face_t *f = surf->faces; f != NULL; f = f->next )
	{
		ASSERT( f->w != NULL );

		if( f->contents >= 0 )
			COM_FatalError( "bad contents %d\n", f->contents );

		WindingBounds( f->w, surf->mins, surf->maxs, true );

		if( surf->detaillevel == -1 || f->detaillevel < surf->detaillevel )
			surf->detaillevel = f->detaillevel;
	}
}

/*
==================
DivideSurface
==================
*/
void DivideSurface( surface_t *in, plane_t *split, surface_t **front, surface_t **back )
{
	face_t	*facet, *next;
	face_t	*frontlist, *backlist;
	face_t	*frontfrag, *backfrag;
	plane_t	*inplane;	
	surface_t	*news;

	inplane = &g_mapplanes[in->planenum];
	
	// parallel case is easy
	if( VectorCompare( inplane->normal, split->normal ))
	{
		// check for exactly on node
		if( inplane->dist > split->dist )
		{
			*front = in;
			*back = NULL;
		}
		else if( inplane->dist < split->dist )
		{
			*front = NULL;
			*back = in;
		}
		else
		{
			frontlist = NULL;
			backlist = NULL;

			for( facet = in->faces; facet; facet = next )
			{
				next = facet->next;

				if( facet->planenum & 1 )
				{
					facet->next = backlist;
					backlist = facet;
				}
				else
				{
					facet->next = frontlist;
					frontlist = facet;
				}
			}
			goto makesurfs;
		}
		return;
	}

	// do a real split.  may still end up entirely on one side
	// OPTIMIZE: use bounding box for fast test
	frontlist = backlist = NULL;
	
	for( facet = in->faces; facet != NULL; facet = next )
	{
		next = facet->next;

		SplitFaceEpsilon( facet, split, &frontfrag, &backfrag, BSPCHOP_EPSILON );

		if( frontfrag )
		{
			frontfrag->next = frontlist;
			frontlist = frontfrag;
		}

		if( backfrag )
		{
			backfrag->next = backlist;
			backlist = backfrag;
		}
	}
makesurfs:
	// if nothing actually got split, just move the in plane
	if( frontlist == NULL )
	{
		*front = NULL;
		*back = in;
		in->faces = backlist;
		return;
	}

	if( backlist == NULL )
	{
		*front = in;
		*back = NULL;
		in->faces = frontlist;
		return;
	}

	// stuff got split, so allocate one new surface and reuse in
	news = AllocSurface ();
	total_tree_faces++;
	*news = *in;
	news->faces = backlist;
	*back = news;
	
	in->faces = frontlist;
	*front = in;
	
	// recalc bboxes and flags
	CalcSurfaceInfo( news );
	CalcSurfaceInfo( in );	
}

/*
==================
DivideNodeBounds
==================
*/
static void DivideNodeBounds( node_t *node, plane_t *split )
{
	node_t	*front = node->children[0];
	node_t	*back = node->children[1];

	ASSERT( front && back );

	DivideBounds( node->mins, node->maxs, split, front->mins, front->maxs, back->mins, back->maxs );
}

/*
==================
SplitNodeSurfaces
==================
*/
static void SplitNodeSurfaces( surface_t *surfaces, const node_t *node )
{
	surface_t	*frontlist, *frontfrag;
	surface_t	*backlist, *backfrag;
	plane_t	*splitplane;
	surface_t	*p, *next;

	splitplane = &g_mapplanes[node->planenum];

	frontlist = NULL;
	backlist = NULL;

	for( p = surfaces; p != NULL; p = next )
	{
		next = p->next;
		DivideSurface( p, splitplane, &frontfrag, &backfrag );

		if( frontfrag )
		{
			if( !frontfrag->faces )
				COM_FatalError( "surface with no faces\n" );
			frontfrag->next = frontlist;
			frontlist = frontfrag;
		}

		if( backfrag )
		{
			if( !backfrag->faces )
				COM_FatalError( "surface with no faces\n" );
			backfrag->next = backlist;
			backlist = backfrag;
		}
	}

	node->children[0]->surfaces = frontlist;
	node->children[1]->surfaces = backlist;
}

/*
==================
SplitNodeBrushes
==================
*/
static void SplitNodeBrushes( brush_t *brushes, const node_t *node )
{
	brush_t	*frontlist, *frontfrag;
	brush_t	*backlist, *backfrag;
	plane_t	*splitplane;
	brush_t	*b, *next;

	splitplane = &g_mapplanes[node->planenum];

	frontlist = NULL;
	backlist = NULL;

	for( b = brushes; b; b = next )
	{
		next = b->next;
		SplitBrush( b, splitplane, &frontfrag, &backfrag );

		if( frontfrag )
		{
			frontfrag->next = frontlist;
			frontlist = frontfrag;
		}

		if( backfrag )
		{
			backfrag->next = backlist;
			backlist = backfrag;
		}
	}

	node->children[0]->detailbrushes = frontlist;
	node->children[1]->detailbrushes = backlist;
}

/*
==================
RankForContents
==================
*/
int RankForContents( int contents )
{
	switch( contents )
	{
	case CONTENTS_EMPTY:	return 0;
	case CONTENTS_VISBLOCKER:	return 1;
	case CONTENTS_TRANSLUCENT:	return 2;
	case CONTENTS_FOG:		return 3;
	case CONTENTS_WATER:	return 4;
	case CONTENTS_SLIME:	return 5;
	case CONTENTS_LAVA :	return 6;
	case CONTENTS_SKY  :	return 7;
	case CONTENTS_SOLID:	return 8;
	default: COM_FatalError( "RankForContents: bad contents %i\n", contents );
	}

	return -1;
}

/*
==================
ContentsForRank
==================
*/
int ContentsForRank( int rank )
{
	switch( rank )
	{
	case -1: return CONTENTS_EMPTY;	// no faces at all
	case 0: return CONTENTS_EMPTY;
	case 1: return CONTENTS_VISBLOCKER;
	case 2: return CONTENTS_TRANSLUCENT;
	case 3: return CONTENTS_FOG;
	case 4: return CONTENTS_WATER;
	case 5: return CONTENTS_SLIME;
	case 6: return CONTENTS_LAVA;
	case 7: return CONTENTS_SKY;
	case 8: return CONTENTS_SOLID;
	default: COM_FatalError( "ContentsForRank: bad rank %i\n", rank );
	}

	return -1;
}

/*
==================
MakeNodePortal

create the new portal by taking the full plane winding for the cutting plane
and clipping it by all of the planes from the other portals.

Each portal tracks the node that created it, so unused nodes
can be removed later.
==================
*/
void MakeNodePortal( node_t *node )
{
	portal_t	*new_portal, *p;
	plane_t	*plane;
	plane_t	*clipplane;
	int	side = 0;
	winding_t	*w;

	plane = &g_mapplanes[node->planenum];
	w = BaseWindingForPlane( plane->normal, plane->dist );

	new_portal = AllocPortal();
	new_portal->planenum = node->planenum;
	new_portal->onnode = node;

	for( p = node->portals; p != NULL; p = p->next[side] )	
	{
		if( p->nodes[0] == node )
		{
			clipplane = &g_mapplanes[p->planenum];
			side = 0;
		}
		else if( p->nodes[1] == node )
		{
			clipplane = &g_mapplanes[p->planenum ^ 1];
			side = 1;
		}
		else COM_FatalError( "MakeNodePortal: mislinked portal\n" );

		if( !ChopWindingInPlace( &w, clipplane->normal, clipplane->dist, g_prtepsilon ))
		{
			FreePortal( new_portal );
			c_clipped_portals++;
			return;
		}
	}

	new_portal->winding = w;	
	AddPortalToNodes( new_portal, node->children[0], node->children[1] );
}

/*
==============
SplitNodePortals

Move or split the portals that bound node so that the node's
children have portals instead of node.
==============
*/
void SplitNodePortals( node_t *node )
{
	portal_t	*p, *next_portal, *new_portal;
	winding_t	*frontwinding, *backwinding;
	node_t	*f, *b, *other_node;
	int	side = 0;
	plane_t	*plane;

	plane = &g_mapplanes[node->planenum];
	f = node->children[0];
	b = node->children[1];

	for( p = node->portals; p != NULL; p = next_portal )	
	{
		if( p->nodes[0] == node )
			side = 0;
		else if( p->nodes[1] == node )
			side = 1;
		else COM_FatalError( "CutNodePortals_r: mislinked portal\n" );

		next_portal = p->next[side];
		other_node = p->nodes[!side];

		RemovePortalFromNode( p, p->nodes[0] );
		RemovePortalFromNode( p, p->nodes[1] );

		// cut the portal into two portals, one on each side of the cut plane
		DivideWindingEpsilon( p->winding, plane->normal, plane->dist, g_prtepsilon, &frontwinding, &backwinding );

		if( !frontwinding && !backwinding )
			continue;

		if( !frontwinding )
		{
			if( !side ) AddPortalToNodes( p, b, other_node );
			else AddPortalToNodes( p, other_node, b );
			continue;
		}

		if( !backwinding )
		{
			if( !side ) AddPortalToNodes( p, f, other_node );
			else AddPortalToNodes( p, other_node, f );
			continue;
		}
		
		// the winding is split
		new_portal = AllocPortal();
		*new_portal = *p;
		new_portal->winding = backwinding;
		FreeWinding( p->winding );
		p->winding = frontwinding;

		if( side == 0 )
		{
			AddPortalToNodes( p, f, other_node );
			AddPortalToNodes( new_portal, b, other_node );
		}
		else
		{
			AddPortalToNodes( p, other_node, f );
			AddPortalToNodes( new_portal, other_node, b );
		}
	}

	node->portals = NULL;
}

/*
==================
CalcNodeBounds

Determines the boundaries of a node by
minmaxing all the portal points, whcih
completely enclose the node.

 Returns true if the node should be midsplit.(very large)
==================
*/
bool CalcNodeBounds( node_t *node, vec3_t validmins, vec3_t validmaxs )
{
	portal_t	*p, *next_portal;
	int	i, side;

	if( FBitSet( node->flags, FNODE_DETAIL ))
		return false;

	ClearBounds( node->mins, node->maxs );

	for( p = node->portals; p != NULL; p = next_portal )	
	{
		if( p->nodes[0] == node )
			side = 0;
		else if( p->nodes[1] == node )
			side = 1;
		else COM_FatalError( "CalcNodeBounds: mislinked portal\n" );

		next_portal = p->next[side];
		WindingBounds( p->winding, node->mins, node->maxs, true );
	}

	if( FBitSet( node->flags, FNODE_LEAFPORTAL ))
		return false;

	for( i = 0; i < 3; i++ )
	{
		validmins[i] = Q_max( node->mins[i], -( 32768.0 + g_maxnode_size ));
		validmaxs[i] = Q_min( node->maxs[i],  ( 32768.0 + g_maxnode_size ));
	}

	for( i = 0; i < 3; i++ )
	{
		if( validmaxs[i] - validmins[i] <= ON_EPSILON )
			return false;
	}

	for( i = 0; i < 3; i++ )
	{
		if( validmaxs[i] - validmins[i] > g_maxnode_size + ON_EPSILON )
			return true;
	}

	return false;
}

/*
==================
FreeLeafSurfs
==================
*/
void FreeLeafSurfs( node_t *leaf )
{
	surface_t	*surf, *snext;
	face_t	*f, *fnext;

	for( surf = leaf->surfaces; surf != NULL; surf = snext )
	{
		snext = surf->next;

		for( f = surf->faces; f != NULL; f = fnext )
		{
			fnext = f->next;
			FreeFace( f );
		}

		FreeSurface( surf );
	}

	leaf->surfaces = NULL;
}

/*
==================
FreeLeafBrushes
==================
*/
static void FreeLeafBrushes( node_t *leaf )
{
	brush_t	*b, *next;

	for( b = leaf->detailbrushes; b != NULL; b = next )
	{
		next = b->next;
		FreeBrush( b );
	}

	leaf->detailbrushes = NULL;
}

/*
==================
LinkNodeFaces

Do a final merge attempt, then subdivide the faces to surface cache size if needed.
These are final faces that will be drawable in the game.
Copies of these faces are further chopped up into the leafs, but they will reference these originals.
==================
*/
void LinkNodeFaces( node_t *node, surface_t *surf, bool subdivide )
{
	face_t	*f, *newf, **prevptr;

	// merge as much as possible
	MergePlaneFaces( surf, subdivide ? g_merge_level : 1 );

	// subdivide
	prevptr = &surf->faces;

	while( subdivide )
	{
		f = *prevptr;
		if( !f ) break;

		SubdivideFace( f, prevptr );

		f = *prevptr;
		prevptr = &f->next;
	}

	node->surfaces = NULL;
	node->faces = NULL;

	// copy the faces to the node, and consider them the originals
	for( f = surf->faces; f != NULL; f = f->next )
	{
		dispatch_tree_faces++;

		if( f->facestyle == face_discardable )
			continue;

		// FIXME: we shouldn't check for CONTENTS_SKY here!!!
		if( f->contents != CONTENTS_SOLID && f->contents != CONTENTS_SKY )
		{
			newf = NewFaceFromFace( f );
			newf->w = CopyWinding( f->w );
			f->original = newf;
			newf->next = node->faces;
			node->faces = newf;
			c_nodefaces++;
		}
	}

	if( g_report_progress )
	{
		UpdatePacifier( (float)dispatch_tree_faces / total_tree_faces );
	}
}

/*
==================
SetLeafContents

Determines the contents of the leaf and creates the final list of
original faces that have some fragment inside this leaf
==================
*/
void SetLeafContents( surface_t *planelist, node_t *leafnode )
{
	int	rank, r;
	surface_t	*surf;

	rank = -1;

	for( surf = planelist; surf != NULL; surf = surf->next )
	{
		if( !surf->onnode )
			continue;

		for( face_t *f = surf->faces; f != NULL; f = f->next )
		{
			if( f->detaillevel ) continue;
			r = RankForContents( f->contents );
			rank = Q_max( rank, r );
		}
	}

	leafnode->contents = ContentsForRank( rank );
}

static void MakeLeaf( node_t *leafnode )
{
	int	nummarkfaces;
	surface_t	*surf;
	face_t	*f;	

	leafnode->planenum = PLANENUM_LEAF;

	if( leafnode->detailbrushes )
		SetBits( leafnode->flags, FNODE_DETAILCONTENTS );
	FreeLeafBrushes( leafnode );
	leafnode->detailbrushes = NULL;

	if( leafnode->boundsbrush )
		FreeBrush( leafnode->boundsbrush );
	leafnode->boundsbrush = NULL;

	if( !( FBitSet( leafnode->flags, FNODE_LEAFPORTAL ) && leafnode->contents == CONTENTS_SOLID ))
	{
		nummarkfaces = 0;
		for (surf = leafnode->surfaces; surf; surf = surf->next )
		{
			if( !surf->onnode )
				continue;

			for( f = surf->faces; f != NULL; f = f->next )
			{
				if( f->original == NULL )
				{
					// because it is not on node or its content is solid
					continue;
				}

				if( nummarkfaces == MAX_MAP_MARKSURFACES )
					COM_FatalError( "MAX_MAP_MARKSURFACES limit exceeded\n" );
				markfaces[nummarkfaces++] = f->original;
			}
		}

		markfaces[nummarkfaces] = NULL; // end marker
		nummarkfaces++;

		leafnode->markfaces = (face_t **)Mem_Alloc( nummarkfaces * sizeof( *leafnode->markfaces ));
		memcpy( leafnode->markfaces, markfaces, nummarkfaces * sizeof( *leafnode->markfaces ));
	}

	FreeLeafSurfs( leafnode );
	leafnode->surfaces = NULL;
}

int CalcSplitDetaillevel( const node_t *node )
{
	int	bestdetaillevel = -1;
	surface_t	*s;

	for( s = node->surfaces; s != NULL; s = s->next )
	{
		if( s->onnode )
			continue;

		for( face_t *f = s->faces; f != NULL; f = f->next )
		{
			if( f->facestyle == face_discardable )
				continue;

			if( bestdetaillevel == -1 || f->detaillevel < bestdetaillevel )
				bestdetaillevel = f->detaillevel;
		}
	}

	return bestdetaillevel;
}

void FixDetaillevelForDiscardable( node_t *node, int detaillevel )
{
	surface_t	*s, **psnext;
	face_t	*f, **pfnext;

	// when we move on to the next detaillevel, some discardable faces of previous detail level remain not on node
	// (because they are discardable). remove them now
	for( psnext = &node->surfaces; s = *psnext, s != NULL; )
	{
		if( s->onnode )
		{
			psnext = &s->next;
			continue;
		}

		ASSERT( s->faces != NULL );

		for( pfnext = &s->faces; f = *pfnext, f != NULL; )
		{
			if( detaillevel == -1 || f->detaillevel < detaillevel )
			{
				*pfnext = f->next;
				FreeFace( f );
			}
			else
			{
				pfnext = &f->next;
			}
		}

		if( !s->faces )
		{
			*psnext = s->next;
			FreeSurface( s );
		}
		else
		{
			psnext = &s->next;
			CalcSurfaceInfo( s );
			ASSERT( !( detaillevel == -1 || s->detaillevel < detaillevel ));
		}
	}
}

/*
==================
BuildBspTree_r
==================
*/
void BuildBspTree_r( node_t *node, bool subdivide )
{
	vec3_t	validmins, validmaxs;
	surface_t	*allsurfs;
	bool	midsplit;
	surface_t	*split;

	midsplit = CalcNodeBounds( node, validmins, validmaxs );

	if( node->boundsbrush )
	{
		CalcBrushBounds( node->boundsbrush, node->loosemins, node->loosemaxs );
	}
	else
	{
		VectorFill( node->loosemins,  BOGUS_RANGE );
		VectorFill( node->loosemaxs, -BOGUS_RANGE );
	}

	int splitdetaillevel = CalcSplitDetaillevel( node );
	FixDetaillevelForDiscardable( node, splitdetaillevel );

	// select the partition plane
	split = SelectPartition( node->surfaces, node, midsplit, splitdetaillevel, validmins, validmaxs );

	if( !FBitSet( node->flags, FNODE_DETAIL ) && ( !split || split->detaillevel > 0 ))
	{
		SetBits( node->flags, FNODE_LEAFPORTAL );
		SetLeafContents( node->surfaces, node );

		if( node->contents == CONTENTS_SOLID )
			split = NULL;
	}
	else
	{
		ClearBits( node->flags, FNODE_LEAFPORTAL );
	}

	if( !split )
	{	
		// this is a leaf node
		MakeLeaf( node );
		return;
	}

	split->onnode = node; // can't use again
	node->planenum = split->planenum;
	allsurfs = node->surfaces;

	// these are final polygons
	LinkNodeFaces( node, split, subdivide );
	node->children[0] = AllocNode ();
	node->children[1] = AllocNode ();
	c_splitnodes++;

	if( split->detaillevel > 0 )
		SetBits( node->children[0]->flags, FNODE_DETAIL );

	if( split->detaillevel > 0 )
		SetBits( node->children[1]->flags, FNODE_DETAIL );

	// split all the polysurfaces into front and back lists
	SplitNodeSurfaces( allsurfs, node );
	SplitNodeBrushes( node->detailbrushes, node );

	if( node->boundsbrush )
	{
		for( int k = 0; k < 2; k++ )
		{
			brush_t	*copy, *front, *back;
			plane_t	p;

			if( k == 0 )
			{
				// front child
				VectorCopy( g_mapplanes[split->planenum].normal, p.normal );
				p.dist = g_mapplanes[split->planenum].dist - BOUNDS_EXPANSION;
			}
			else
			{
				// back child
				VectorNegate( g_mapplanes[split->planenum].normal, p.normal );
				p.dist = -g_mapplanes[split->planenum].dist - BOUNDS_EXPANSION;
			}

			copy = NewBrushFromBrush( node->boundsbrush );
			SplitBrush( copy, &p, &front, &back );

			if( back ) FreeBrush( back );
			if( !front ) MsgDev( D_WARN, "BuildBspTree_r: bounds was clipped away\n" );

			node->children[k]->boundsbrush = front;
		}
		FreeBrush( node->boundsbrush );
	}

	node->boundsbrush = NULL;

	if( !split->detaillevel )
	{
		// create the portal that seperates the two children
		MakeNodePortal( node );
	
		// carve the portals on the boundaries of the node
		SplitNodePortals( node );
	}

	// recursively do the children
	BuildBspTree_r( node->children[0], subdivide );
	BuildBspTree_r( node->children[1], subdivide );
}

/*
==================
SolidBSP

Takes a chain of surfaces plus a split type, and
returns a bsp tree with faces off the nodes.

The original surface chain will be completely freed.
==================
*/
void SolidBSP( tree_t *tree, int modnum, int hullnum )
{
	vec3_t	brushmins, brushmaxs, size;
	bool	report = (modnum == 0);
	double	start, end;
	int	flags = 0;
	vec_t	maxnode;

	MsgDev( D_REPORT, "----- SolidBSP ----- (hull %i, model %i)\n", hullnum, modnum );

	// calc the maxnode size based on world size
	if( g_maxnode_size == DEFAULT_MAXNODE_SIZE )
	{
		VectorSubtract( tree->maxs, tree->mins, size );
		maxnode = VectorMax( size ) / 8.0; // 8192 / 8 = 1024
		maxnode = Q_roundup( maxnode, 1024.0 );
		MsgDev( D_REPORT, "max node size %g\n", maxnode );
		g_maxnode_size = maxnode;
	}

	tree->headnode = AllocNode ();
	tree->headnode->detailbrushes = tree->detailbrushes;
	tree->headnode->surfaces = tree->surfaces;

	if( !tree->surfaces || ( hullnum != 0 && g_noclip ))
	{
		// nothing at all to build
		if( hullnum != 0 )
		{
			tree->headnode->planenum = PLANENUM_LEAF;
			tree->headnode->contents = CONTENTS_EMPTY;
			SetBits( tree->headnode->flags, FNODE_LEAFPORTAL );
		}
		else
		{
			tree->headnode->children[0] = AllocNode ();
			tree->headnode->children[0]->planenum = PLANENUM_LEAF;
			tree->headnode->children[0]->contents = CONTENTS_EMPTY;
			tree->headnode->children[0]->markfaces = (face_t **)Mem_Alloc( sizeof( face_t * ));
			SetBits( tree->headnode->children[0]->flags, FNODE_LEAFPORTAL );

			tree->headnode->children[1] = AllocNode ();
			tree->headnode->children[1]->planenum = PLANENUM_LEAF;
			tree->headnode->children[1]->contents = CONTENTS_EMPTY;
			tree->headnode->children[1]->markfaces = (face_t **)Mem_Alloc( sizeof( face_t * ));
			SetBits( tree->headnode->children[1]->flags, FNODE_LEAFPORTAL );
		}
		return;
	}

	// calculate a bounding box for the entire model
	for( int i = 0; i < 3; i++ )
	{
		tree->headnode->mins[i] = tree->mins[i];
		tree->headnode->maxs[i] = tree->maxs[i];
		brushmins[i] = tree->mins[i] - SIDESPACE;
		brushmaxs[i] = tree->maxs[i] + SIDESPACE;
	}

	tree->headnode->boundsbrush = BrushFromBox( brushmins, brushmaxs );

	c_unsplitted_faces = 0;
	c_clipped_portals = 0;	
	c_splitnodes = 0;
	c_nodefaces = 0;
	c_leaffaces = 0;

	// generate six portals that enclose the entire world
	MakeHeadnodePortals( tree->headnode, tree->mins, tree->maxs );
	g_report_progress = report;

	if( g_report_progress )
	{
		// because we have mirror for each face
		total_tree_faces = tree->numsurfaces;
		dispatch_tree_faces = 0;
		start = I_FloatTime();
		StartPacifier();
	}

	//
	// recursively partition everything
	//
	BuildBspTree_r( tree->headnode, ( hullnum == 0 ));

	if( report )
	{
		end = I_FloatTime ();
		EndPacifier( end - start );
		if( c_clipped_portals ) MsgDev( D_WARN, "%i portals was clipped away\n", c_clipped_portals );
		if( c_unsplitted_faces ) MsgDev( D_WARN, "%i faces can't be a split\n", c_unsplitted_faces );
	}

	MsgDev( D_REPORT, "%5i split nodes\n", c_splitnodes );
	MsgDev( D_REPORT, "%5i node faces\n", c_nodefaces );
	MsgDev( D_REPORT, "%5i leaf faces\n", c_leaffaces );
}