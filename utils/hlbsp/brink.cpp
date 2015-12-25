#include "bsp5.h"

#ifdef HLBSP_BRINKHACK

#include <list>
#include <map>
#include <vector>

// TODO: we should consider corners in addition to brinks.
// TODO: use bcircle_t structure only to find out all possible "movement"s, then send then down the bsp tree to determine which leafs may incorrectly block the movement.

// The image of a typical buggy brink (with type BrinkFloorBlocking):
//
//                brinkstoppoint
//        x------------x
//       /            / \.
//      /            /   \.
//     /   move >>>>xblocked
//    /            /       \.
//   /            /         x
//  x------------x         /
//  |      brinkstartpoint/
//  |            | \     /             Z
//  |            |  \   /              |  Y
//  |            |   \ /               | /
//  x------------x----x                |/
//                                     O-------X
//


typedef struct bpartition_s
{
	int planenum;
	bool planeside;
	int content;
	bbrinklevel_e type;

	bpartition_s *next;
}
bpartition_t;

typedef struct bclipnode_s
{
	bool isleaf;

	int planenum;
	const dplane_t *plane;
	bclipnode_s *children[2]; // children[0] is the front side of the plane (SIDE_FRONT = 0)

	int content;
	bpartition_t *partitions;

	struct btreeleaf_s *treeleaf;
}
bclipnode_t;

typedef struct bbrinknode_s
{ // we will only focus on the bsp shape which encircles a brink, so we extract the clipnodes that meets with the brink and store them here
	bool isleaf;

	int planenum;
	const dplane_t *plane;
	int children[2];

	int content;
	bclipnode_t *clipnode;
}
bbrinknode_t;

typedef struct
{
	vec3_t start, stop;
	vec3_t direction;

	int numnodes; // including both nodes and leafs
	std::vector< bbrinknode_t > *nodes;

	struct btreeedge_s *edge; // only for use in deciding brink type
}
bbrink_t;

bbrink_t *CopyBrink (bbrink_t *other)
{
	bbrink_t *b;
	hlassume (b = (bbrink_t *)malloc (sizeof (bbrink_t)), assume_NoMemory);
	VectorCopy (other->direction, b->direction);
	VectorCopy (other->start, b->start);
	VectorCopy (other->stop, b->stop);
	b->numnodes = other->numnodes;
	b->nodes = new std::vector< bbrinknode_t >(*other->nodes);
	return b;
}

void DeleteBrink (bbrink_t *b)
{
	delete b->nodes;
	free (b);
}

bbrink_t *CreateBrink (vec3_t start, vec3_t stop)
{
	bbrink_t *b;
	hlassume (b = (bbrink_t *)malloc (sizeof (bbrink_t)), assume_NoMemory);

	VectorCopy (start, b->start);
	VectorCopy (stop, b->stop);
	VectorSubtract (stop, start, b->direction);

	b->numnodes = 1;
	b->nodes = new std::vector< bbrinknode_t >;
	bbrinknode_t newnode;
	newnode.isleaf = true;
	newnode.clipnode = NULL;
	b->nodes->push_back (newnode);
	
	// CreateBrink must be followed by BrinkSplitClipnode
	return b;
}

void PrintBrink (const bbrink_t *b)
{
	Log ("direction %f %f %f start %f %f %f stop %f %f %f\n", b->direction[0], b->direction[1], b->direction[2], b->start[0], b->start[1], b->start[2], b->stop[0], b->stop[1], b->stop[2]);
	Log ("numnodes %d\n", b->numnodes);
	for (int i = 0; i < b->numnodes; i++)
	{
		bbrinknode_t *n = &(*b->nodes)[i];
		if (n->isleaf)
		{
			Log ("leaf[%d] content %d\n", i, n->content);
		}
		else
		{
			Log ("node[%d]-[%d:%d] plane %f %f %f %f\n", i, n->children[0], n->children[1], n->plane->normal[0], n->plane->normal[1], n->plane->normal[2], n->plane->dist);
		}
	}
}

void BrinkSplitClipnode (bbrink_t *b, const dplane_t *plane, int planenum, bclipnode_t *prev, bclipnode_t *n0, bclipnode_t *n1)
{
	int found;
	int numfound = 0;
	for (int i = 0; i < b->numnodes; i++)
	{
		bbrinknode_t *node = &(*b->nodes)[i];
		if (node->isleaf && node->clipnode == prev)
		{
			found = i;
			numfound++;
		}
	}
	if (numfound == 0)
	{
		PrintOnce ("BrinkSplitClipnode: internal error: couldn't find clipnode");
		hlassume (false, assume_first);
	}
	if (numfound > 1)
	{
		PrintOnce ("BrinkSplitClipnode: internal error: found more than one clipnode");
		hlassume (false, assume_first);
	}
	if (n0 == n1)
	{
		PrintOnce ("BrinkSplitClipnode: internal error: n0 == n1");
		hlassume (false, assume_first);
	}
	b->nodes->resize (b->numnodes + 2);
	bbrinknode_t *node = &(*b->nodes)[found];
	bbrinknode_t *front = &(*b->nodes)[b->numnodes];
	bbrinknode_t *back = &(*b->nodes)[b->numnodes + 1];
	
	node->clipnode = NULL;
	node->isleaf = false;
	node->plane = plane;
	node->planenum = planenum;
	node->children[0] = b->numnodes;
	node->children[1] = b->numnodes + 1;
	
	front->isleaf = true;
	front->content = n0->content;
	front->clipnode = n0;

	back->isleaf = true;
	back->content = n1->content;
	back->clipnode = n1;

	b->numnodes += 2;
}

void BrinkReplaceClipnode (bbrink_t *b, bclipnode_t *prev, bclipnode_t *n)
{
	int found;
	int numfound = 0;
	for (int i = 0; i < b->numnodes; i++)
	{
		bbrinknode_t *node = &(*b->nodes)[i];
		if (node->isleaf && node->clipnode == prev)
		{
			found = i;
			numfound++;
		}
	}
	if (numfound == 0)
	{
		PrintOnce ("BrinkSplitClipnode: internal error: couldn't find clipnode");
		hlassume (false, assume_first);
	}
	if (numfound > 1)
	{
		PrintOnce ("BrinkSplitClipnode: internal error: found more than one clipnode");
		hlassume (false, assume_first);
	}
	bbrinknode_t *node = &(*b->nodes)[found];
	node->clipnode = n;
	node->content = n->content;
}



// compute the structure of the whole bsp tree

struct btreepoint_s; // 0d object
struct btreeedge_s; // 1d object
struct btreeface_s; // 2d object
struct btreeleaf_s; // 3d object

typedef struct
{
	btreepoint_s *p;
	bool side;
}
btreepoint_r;

typedef struct
{
	btreeedge_s *e;
	bool side;
}
btreeedge_r;

typedef struct
{
	btreeface_s *f;
	bool side;
}
btreeface_r;

typedef struct
{
	btreeleaf_s *l;
	bool side;
}
btreeleaf_r;

typedef std::list< btreepoint_r > btreepoint_l;
typedef std::list< btreeedge_r > btreeedge_l;
typedef std::list< btreeface_r > btreeface_l;
typedef std::list< btreeleaf_r > btreeleaf_l;

typedef struct btreepoint_s
{
	vec3_t v;
	bool infinite;

	btreeedge_l *edges; // this is a reversed reference

	bool tmp_tested;
	vec_t tmp_dist;
	int tmp_side;
}
btreepoint_t;

typedef struct btreeedge_s
{
	btreepoint_r points[2]; // pointing from points[1] to points[0]
	bool infinite; // both points are infinite (i.e. this edge lies on the bounding box)

	btreeface_l *faces; // this is a reversed reference

	bbrink_t *brink; // not defined for infinite edges

	bool tmp_tested;
	int tmp_side;
#ifdef HLBSP_BRINKNOTUSEDBYLEAF_FIX
	bool tmp_onleaf[2];
#endif
}
btreeedge_t;

typedef struct btreeface_s
{
	btreeedge_l *edges; // empty faces are allowed (in order to preserve topological correctness)
	bool infinite; // when the face is infinite, all its edges must also be infinite

	btreeleaf_r leafs[2]; // pointing from leafs[0] to leafs[1] // this is a reversed reference

	const dplane_t *plane; // not defined for infinite face
	int planenum;
	bool planeside; // if ture, this face is pointing at -plane->normal

	bool tmp_tested;
	int tmp_side;
}
btreeface_t;

typedef struct btreeleaf_s
{
	btreeface_l *faces;
	bool infinite; // note: the infinite leaf is not convex

	bclipnode_t *clipnode; // not defined for infinite leaf
}
btreeleaf_t;

btreepoint_t *AllocTreepoint (int &numobjects, bool infinite)
{
	numobjects++;
	btreepoint_t *tp = (btreepoint_t *)malloc (sizeof (btreepoint_t));
	hlassume (tp != NULL, assume_NoMemory);
	tp->edges = new btreeedge_l ();
	tp->infinite = infinite;
	return tp;
}

btreeedge_t *AllocTreeedge (int &numobjects, bool infinite)
{
	numobjects++;
	btreeedge_t *te = (btreeedge_t *)malloc (sizeof (btreeedge_t));
	hlassume (te != NULL, assume_NoMemory);
	te->points[0].p = NULL;
	te->points[0].side = false;
	te->points[1].p = NULL;
	te->points[1].side = true;
	te->faces = new btreeface_l ();
	te->infinite = infinite;
	// should be followed by SetEdgePoints
	return te;
}

void AttachPointToEdge (btreeedge_t *te, btreepoint_t *tp, bool side)
{
	if (te->points[side].p)
	{
		PrintOnce ("AttachPointToEdge: internal error: point occupied.");
		hlassume (false, assume_first);
	}
	if (te->infinite && !tp->infinite)
	{
		PrintOnce ("AttachPointToEdge: internal error: attaching a finite object to an infinite object.");
		hlassume (false, assume_first);
	}
	te->points[side].p = tp;

	btreeedge_r er;
	er.e = te;
	er.side = side;
	tp->edges->push_back (er);
}

void SetEdgePoints (btreeedge_t *te, btreepoint_t *tp0, btreepoint_t *tp1)
{
	AttachPointToEdge (te, tp0, false);
	AttachPointToEdge (te, tp1, true);
}

btreeface_t *AllocTreeface (int &numobjects, bool infinite)
{
	numobjects++;
	btreeface_t *tf = (btreeface_t *)malloc (sizeof (btreeface_t));
	hlassume (tf != NULL, assume_NoMemory);
	tf->edges = new btreeedge_l ();
	tf->leafs[0].l = NULL;
	tf->leafs[0].side = false;
	tf->leafs[1].l = NULL;
	tf->leafs[1].side = true;
	tf->infinite = infinite;
	return tf;
}

void AttachEdgeToFace (btreeface_t *tf, btreeedge_t *te, int side)
{
	if (tf->infinite && !te->infinite)
	{
		PrintOnce ("AttachEdgeToFace: internal error: attaching a finite object to an infinite object.");
		hlassume (false, assume_first);
	}
	btreeedge_r er;
	er.e = te;
	er.side = side;
	tf->edges->push_back (er);

	btreeface_r fr;
	fr.f = tf;
	fr.side = side;
	te->faces->push_back (fr);
}

void AttachFaceToLeaf (btreeleaf_t *tl, btreeface_t *tf, int side)
{
	if (tl->infinite && !tf->infinite)
	{
		PrintOnce ("AttachFaceToLeaf: internal error: attaching a finite object to an infinite object.");
		hlassume (false, assume_first);
	}
	btreeface_r fr;
	fr.f = tf;
	fr.side = side;
	tl->faces->push_back (fr);

	if (tf->leafs[side].l)
	{
		PrintOnce ("AttachFaceToLeaf: internal error: leaf occupied.");
		hlassume (false, assume_first);
	}
	tf->leafs[side].l = tl;
}

void SetFaceLeafs (btreeface_t *tf, btreeleaf_t *tl0, btreeleaf_t *tl1)
{
	AttachFaceToLeaf (tl0, tf, false);
	AttachFaceToLeaf (tl1, tf, true);
}

btreeleaf_t *AllocTreeleaf (int &numobjects, bool infinite)
{
	numobjects++;
	btreeleaf_t *tl = (btreeleaf_t *)malloc (sizeof (btreeleaf_t));
	hlassume (tl != NULL, assume_NoMemory);
	tl->faces = new btreeface_l ();
	tl->infinite = infinite;
	return tl;
}

btreeleaf_t *BuildOutside (int &numobjects)
{
	btreeleaf_t *leaf_outside;
	leaf_outside = AllocTreeleaf (numobjects, true);
	leaf_outside->clipnode = NULL;
	return leaf_outside;
}

btreeleaf_t *BuildBaseCell (int &numobjects, bclipnode_t *clipnode, vec_t range, btreeleaf_t *leaf_outside)
{
	btreepoint_t *tp[8];
	int i;
	for (i = 0; i < 8; i++)
	{
		tp[i] = AllocTreepoint (numobjects, true);
		if (i & 1)
			tp[i]->v[0] = range;
		else
			tp[i]->v[0] = -range;
		if (i & 2)
			tp[i]->v[1] = range;
		else
			tp[i]->v[1] = -range;
		if (i & 4)
			tp[i]->v[2] = range;
		else
			tp[i]->v[2] = -range;
	}
	btreeedge_t *te[12];
	for (i = 0; i < 12; i++)
	{
		te[i] = AllocTreeedge (numobjects, true);
	}
	SetEdgePoints (te[0], tp[1], tp[0]);
	SetEdgePoints (te[1], tp[3], tp[2]);
	SetEdgePoints (te[2], tp[5], tp[4]);
	SetEdgePoints (te[3], tp[7], tp[6]);
	SetEdgePoints (te[4], tp[2], tp[0]);
	SetEdgePoints (te[5], tp[3], tp[1]);
	SetEdgePoints (te[6], tp[6], tp[4]);
	SetEdgePoints (te[7], tp[7], tp[5]);
	SetEdgePoints (te[8], tp[4], tp[0]);
	SetEdgePoints (te[9], tp[5], tp[1]);
	SetEdgePoints (te[10], tp[6], tp[2]);
	SetEdgePoints (te[11], tp[7], tp[3]);
	btreeface_t *tf[6];
	for (i = 0; i < 6; i++)
	{
		tf[i] = AllocTreeface (numobjects, true);
	}
	AttachEdgeToFace (tf[0], te[4], true);
	AttachEdgeToFace (tf[0], te[6], false);
	AttachEdgeToFace (tf[0], te[8], false);
	AttachEdgeToFace (tf[0], te[10], true);
	AttachEdgeToFace (tf[1], te[5], false);
	AttachEdgeToFace (tf[1], te[7], true);
	AttachEdgeToFace (tf[1], te[9], true);
	AttachEdgeToFace (tf[1], te[11], false);
	AttachEdgeToFace (tf[2], te[0], false);
	AttachEdgeToFace (tf[2], te[2], true);
	AttachEdgeToFace (tf[2], te[8], true);
	AttachEdgeToFace (tf[2], te[9], false);
	AttachEdgeToFace (tf[3], te[1], true);
	AttachEdgeToFace (tf[3], te[3], false);
	AttachEdgeToFace (tf[3], te[10], false);
	AttachEdgeToFace (tf[3], te[11], true);
	AttachEdgeToFace (tf[4], te[0], true);
	AttachEdgeToFace (tf[4], te[1], false);
	AttachEdgeToFace (tf[4], te[4], false);
	AttachEdgeToFace (tf[4], te[5], true);
	AttachEdgeToFace (tf[5], te[2], false);
	AttachEdgeToFace (tf[5], te[3], true);
	AttachEdgeToFace (tf[5], te[6], true);
	AttachEdgeToFace (tf[5], te[7], false);
	btreeleaf_t *tl;
	tl = AllocTreeleaf (numobjects, false);
	for (i = 0; i < 6; i++)
	{
		SetFaceLeafs (tf[i], tl, leaf_outside);
	}
	tl->clipnode = clipnode;
	return tl;
}

btreepoint_t *GetPointFromEdge (btreeedge_t *te, bool side)
{
	if (!te->points[side].p)
	{
		PrintOnce ("GetPointFromEdge: internal error: point not set.");
		hlassume (false, assume_first);
	}
	return te->points[side].p;
}

void RemoveEdgeFromList (btreeedge_l *el, btreeedge_t *te, bool side)
{
	btreeedge_l::iterator ei;
	for (ei = el->begin (); ei != el->end (); ei++)
	{
		if (ei->e == te && ei->side == side)
		{
			el->erase (ei);
			return; // only remove one copy if there are many (in order to preserve topological correctness)
		}
	}
	PrintOnce ("RemoveEdgeFromList: internal error: edge not found.");
	hlassume (false, assume_first);
}

void RemovePointFromEdge (btreeedge_t *te, btreepoint_t *tp, bool side) // warning: the point will not be freed
{
	if (te->points[side].p != tp)
	{
		PrintOnce ("RemovePointFromEdge: internal error: point not found.");
		hlassume (false, assume_first);
	}
	te->points[side].p = NULL;

	RemoveEdgeFromList (tp->edges, te, side);
}

void DeletePoint (int &numobjects, btreepoint_t *tp)
{
	if (!tp->edges->empty ())
	{
		PrintOnce ("DeletePoint: internal error: point used by edge.");
		hlassume (false, assume_first);
	}
	delete tp->edges;
	free (tp);
	numobjects--;
}

void RemoveFaceFromList (btreeface_l *fl, btreeface_t *tf, bool side)
{
	btreeface_l::iterator fi;
	for (fi = fl->begin (); fi != fl->end (); fi++)
	{
		if (fi->f == tf && fi->side == side)
		{
			fl->erase (fi);
			return;
		}
	}
	PrintOnce ("RemoveFaceFromList: internal error: face not found.");
	hlassume (false, assume_first);
}

void RemoveEdgeFromFace (btreeface_t *tf, btreeedge_t *te, bool side)
{
	RemoveEdgeFromList (tf->edges, te, side);
	RemoveFaceFromList (te->faces, tf, side);
}

void DeleteEdge (int &numobjects, btreeedge_t *te) // warning: points in this edge could be freed if not reference by any other edges
{
	if (!te->faces->empty ())
	{
		PrintOnce ("DeleteEdge: internal error: edge used by face.");
		hlassume (false, assume_first);
	}
	if (!te->infinite)
	{
		DeleteBrink (te->brink);
	}
	for (int side = 0; side < 2; side++)
	{
		btreepoint_t *tp;
		tp = GetPointFromEdge (te, side);
		RemovePointFromEdge (te, tp, side);
		if (tp->edges->empty ())
		{
			DeletePoint (numobjects, tp);
		}
	}
	delete te->faces;
	free (te);
	numobjects--;
}

btreeleaf_t *GetLeafFromFace (btreeface_t *tf, bool side)
{
	if (!tf->leafs[side].l)
	{
		PrintOnce ("GetLeafFromFace: internal error: Leaf not set.");
		hlassume (false, assume_first);
	}
	return tf->leafs[side].l;
}

void RemoveFaceFromLeaf (btreeleaf_t *tl, btreeface_t *tf, bool side)
{
	if (tf->leafs[side].l != tl)
	{
		PrintOnce ("RemoveFaceFromLeaf: internal error: leaf not found.");
		hlassume (false, assume_first);
	}
	tf->leafs[side].l = NULL;

	RemoveFaceFromList (tl->faces, tf, side);
}

void DeleteFace (int &numobjects, btreeface_t *tf) // warning: edges in this face could be freed if not reference by any other faces
{
	btreeedge_l::iterator ei;
	while ((ei = tf->edges->begin ()) != tf->edges->end ())
	{
		btreeedge_t *te = ei->e;
		RemoveFaceFromList (te->faces, tf, ei->side);
		tf->edges->erase (ei);
		if (te->faces->empty ())
		{
			DeleteEdge (numobjects, te);
		}
	}
	for (int side = 0; side < 2; side++)
	{
		if (tf->leafs[side].l)
		{
			PrintOnce ("DeleteFace: internal error: face used by leaf.");
			hlassume (false, assume_first);
		}
	}
	delete tf->edges;
	free (tf);
	numobjects--;
}

void DeleteLeaf (int &numobjects, btreeleaf_t *tl)
{
	btreeface_l::iterator fi;
	while ((fi = tl->faces->begin ()) != tl->faces->end ())
	{
		btreeface_t *tf = fi->f;
		RemoveFaceFromLeaf (tl, tf, fi->side);
		if (!tf->leafs[false].l && !tf->leafs[true].l)
		{
			DeleteFace (numobjects, tf);
		}
	}
	delete tl->faces;
	free (tl);
	numobjects--;
}

void SplitTreeLeaf (int &numobjects, btreeleaf_t *tl, const dplane_t *plane, int planenum, vec_t epsilon, btreeleaf_t *&front, btreeleaf_t *&back, bclipnode_t *c0, bclipnode_t *c1)
{
	btreeface_l::iterator fi;
	btreeedge_l::iterator ei;
	bool restart = false;

	// clear all the flags
	for (fi = tl->faces->begin (); fi != tl->faces->end (); fi++)
	{
		btreeface_t *tf = fi->f;
		tf->tmp_tested = false;
		for (ei = tf->edges->begin (); ei != tf->edges->end (); ei++)
		{
			btreeedge_t *te = ei->e;
			te->tmp_tested = false;
			for (int side = 0; side < 2; side++)
			{
				btreepoint_t *tp = GetPointFromEdge (te, side);
				tp->tmp_tested = false;
			}
		}
	}

	// split each point
	for (fi = tl->faces->begin (); fi != tl->faces->end (); fi++)
	{
		btreeface_t *tf = fi->f;
		for (ei = tf->edges->begin (); ei != tf->edges->end (); ei++)
		{
			btreeedge_t *te = ei->e;
			for (int side = 0; side < 2; side++)
			{
				btreepoint_t *tp = GetPointFromEdge (te, side);
				if (tp->tmp_tested)
				{
					continue;
				}
				tp->tmp_tested = true;
				vec_t dist = DotProduct (tp->v, plane->normal) - plane->dist;
				tp->tmp_dist = dist;
				if (dist > epsilon)
				{
					tp->tmp_side = SIDE_FRONT;
				}
				else if (dist < -epsilon)
				{
					tp->tmp_side = SIDE_BACK;
				}
				else
				{
					tp->tmp_side = SIDE_ON;
				}
#ifdef HLBSP_BRINKHACK_BUGFIX
#if 0
				// let's mess up something and see whether the code is fragile or robust
				static int randcounter = 0;
				if (randcounter++ % 8 == 0) tp->tmp_side = randcounter % 3;
#endif
#endif
			}
		}
	}

	// split each edge
	for (fi = tl->faces->begin (); fi != tl->faces->end (); fi++)
	{
		btreeface_t *tf = fi->f;
		for (ei = tf->edges->begin (); ei != tf->edges->end (); restart? restart = false, ei = tf->edges->begin (): ei++)
		{
			btreeedge_t *te = ei->e;
			if (te->tmp_tested) // splitted
			{
				continue;
			}
			te->tmp_tested = true;
			te->tmp_side = SIDE_ON;
			for (int side = 0; side < 2; side++)
			{
				btreepoint_t *tp = GetPointFromEdge (te, side);
				if (te->tmp_side == SIDE_ON)
				{
					te->tmp_side = tp->tmp_side;
				}
				else if (tp->tmp_side != SIDE_ON && tp->tmp_side != te->tmp_side)
				{
					te->tmp_side = SIDE_CROSS;
				}
			}
#ifdef HLBSP_BRINKNOTUSEDBYLEAF_FIX
			// The plane does not necessarily split the leaf into two, because of epsilon problem etc., and this will cause "Error: CollectBrinks_r: not leaf" on some maps.
			// In addition, by the time of this step (split edges), the plane has not splitted the leaf yet, so splitting the brink leafs now will break the integrety of the entire geometry. (We want the four steps to be as independent on each other as possible, that is, the entire geometry remains valid after each step.)
#else
			if (!te->infinite)
			{
				if (te->tmp_side == SIDE_ON)
				{
					// since the plane splits the leaf into two, it must also split the brink into two
					BrinkSplitClipnode (te->brink, plane, planenum, tl->clipnode, c0, c1);
				}
				else if (te->tmp_side == SIDE_FRONT)
				{
					BrinkReplaceClipnode (te->brink, tl->clipnode, c0);
				}
				else if (te->tmp_side == SIDE_BACK)
				{
					BrinkReplaceClipnode (te->brink, tl->clipnode, c1);
				}
			}
#endif
			if (te->tmp_side == SIDE_CROSS)
			{
				btreepoint_t *tp0 = GetPointFromEdge (te, false);
				btreepoint_t *tp1 = GetPointFromEdge (te, true);
				btreepoint_t *tpmid = AllocTreepoint (numobjects, te->infinite);
				tpmid->tmp_tested = true;
				tpmid->tmp_dist = 0;
				tpmid->tmp_side = SIDE_ON;
				vec_t frac = tp0->tmp_dist / (tp0->tmp_dist - tp1->tmp_dist);
				for (int k = 0; k < 3; k++)
				{
					tpmid->v[k] = tp0->v[k] + frac * (tp1->v[k] - tp0->v[k]);
				}
				btreeedge_t *te0 = AllocTreeedge (numobjects, te->infinite);
				SetEdgePoints (te0, tp0, tpmid);
				te0->tmp_tested = true;
				te0->tmp_side = tp0->tmp_side;
				if (!te0->infinite)
				{
					te0->brink = CopyBrink (te->brink);
					VectorCopy (tpmid->v, te0->brink->start);
					VectorCopy (tp0->v, te0->brink->stop);
#ifndef HLBSP_BRINKNOTUSEDBYLEAF_FIX
					BrinkReplaceClipnode (te0->brink, tl->clipnode, (tp0->tmp_side == SIDE_BACK? c1: c0));
#endif
				}
				btreeedge_t *te1 = AllocTreeedge (numobjects, te->infinite);
				SetEdgePoints (te1, tpmid, tp1);
				te1->tmp_tested = true;
				te1->tmp_side = tp1->tmp_side;
				if (!te1->infinite)
				{
					te1->brink = CopyBrink (te->brink);
					VectorCopy (tp1->v, te1->brink->start);
					VectorCopy (tpmid->v, te1->brink->stop);
#ifndef HLBSP_BRINKNOTUSEDBYLEAF_FIX
					BrinkReplaceClipnode (te1->brink, tl->clipnode, (tp1->tmp_side == SIDE_BACK? c1: c0));
#endif
				}
				btreeface_l::iterator fj;
				while ((fj = te->faces->begin ()) != te->faces->end ())
				{
					AttachEdgeToFace (fj->f, te0, fj->side);
					AttachEdgeToFace (fj->f, te1, fj->side);
					RemoveEdgeFromFace (fj->f, te, fj->side);
				}
				DeleteEdge (numobjects, te);
				restart = true;
			}
		}
	}

	// split each face
	for (fi = tl->faces->begin (); fi != tl->faces->end (); restart? restart = false, fi = tl->faces->begin (): fi++)
	{
		btreeface_t *tf = fi->f;
		if (tf->tmp_tested)
		{
			continue;
		}
		tf->tmp_tested = true;
		tf->tmp_side = SIDE_ON;
		for (ei = tf->edges->begin (); ei != tf->edges->end (); ei++)
		{
			if (tf->tmp_side == SIDE_ON)
			{
				tf->tmp_side = ei->e->tmp_side;
			}
			else if (ei->e->tmp_side != SIDE_ON && ei->e->tmp_side != tf->tmp_side)
			{
				tf->tmp_side = SIDE_CROSS;
			}
		}
		if (tf->tmp_side == SIDE_CROSS)
		{
			btreeface_t *frontface, *backface;
			frontface = AllocTreeface (numobjects, tf->infinite);
			if (!tf->infinite)
			{
				frontface->plane = tf->plane;
				frontface->planenum = tf->planenum;
				frontface->planeside = tf->planeside;
			}
			SetFaceLeafs (frontface, GetLeafFromFace (tf, false), GetLeafFromFace (tf, true));
			frontface->tmp_tested = true;
			frontface->tmp_side = SIDE_FRONT;
			backface = AllocTreeface (numobjects, tf->infinite);
			if (!tf->infinite)
			{
				backface->plane = tf->plane;
				backface->planenum = tf->planenum;
				backface->planeside = tf->planeside;
			}
			SetFaceLeafs (backface, GetLeafFromFace (tf, false), GetLeafFromFace (tf, true));
			backface->tmp_tested = true;
			backface->tmp_side = SIDE_BACK;

			std::map< btreepoint_t *, int > vertexes;
			std::map< btreepoint_t *, int >::iterator vertex, vertex2;
			for (ei = tf->edges->begin (); ei != tf->edges->end (); ei++)
			{
				if (ei->e->tmp_side != SIDE_BACK)
				{
					AttachEdgeToFace (frontface, ei->e, ei->side);
				}
				else
				{
					AttachEdgeToFace (backface, ei->e, ei->side);

					btreeedge_t *e = ei->e;
					for (int side = 0; side < 2; side++)
					{
						btreepoint_t *p = GetPointFromEdge (e, side);
						vertexes[p] += ((bool)side == ei->side? 1: -1); // the default value is 0 if vertexes[p] does not exist
						vertex = vertexes.find (p);
						if (vertex->second == 0)
						{
							vertexes.erase (vertex);
						}
					}
				}
			}
			if (vertexes.size () != 2)
			{
				Developer (DEVELOPER_LEVEL_WARNING, "SplitTreeLeaf: got invalid edge from split\n");
			}

			while (1)
			{
				for (vertex = vertexes.begin (); vertex != vertexes.end (); vertex++)
				{
					if (vertex->second > 0)
					{
						break;
					}
				}
				for (vertex2 = vertexes.begin (); vertex2 != vertexes.end (); vertex2++)
				{
					if (vertex2->second < 0)
					{
						break;
					}
				}
				if (vertex == vertexes.end () && vertex2 == vertexes.end ())
				{
					break;
				}
				if (vertex == vertexes.end () || vertex2 == vertexes.end ())
				{
					PrintOnce ("SplitTreeLeaf: internal error: couldn't link edges");
					hlassume (false, assume_first);
				}
				if (vertex->second != 1 || vertex2->second != -1)
				{
					Developer (DEVELOPER_LEVEL_WARNING, "SplitTreeLeaf: got deformed edge from split\n");
				}
				if (vertex->first->tmp_side != SIDE_ON || vertex2->first->tmp_side != SIDE_ON)
				{
					PrintOnce ("SplitTreeLeaf: internal error: tmp_side != SIDE_ON");
					hlassume (false, assume_first);
				}

				btreeedge_t *te;
				te = AllocTreeedge (numobjects, tf->infinite);
				SetEdgePoints (te, vertex->first, vertex2->first);
				if (!te->infinite)
				{
					te->brink = CreateBrink (vertex2->first->v, vertex->first->v);
					if (GetLeafFromFace (tf, tf->planeside)->infinite || GetLeafFromFace (tf, !tf->planeside)->infinite)
					{
						PrintOnce ("SplitTreeLeaf: internal error: an infinite object contains a finite object");
						hlassume (false, assume_first);
					}
					BrinkSplitClipnode (te->brink, tf->plane, tf->planenum, NULL, GetLeafFromFace (tf, tf->planeside)->clipnode, GetLeafFromFace (tf, !tf->planeside)->clipnode);
#ifndef HLBSP_BRINKNOTUSEDBYLEAF_FIX
					BrinkSplitClipnode (te->brink, plane, planenum, tl->clipnode, c0, c1);
#endif
				}
				te->tmp_tested = true;
				te->tmp_side = SIDE_ON;
				AttachEdgeToFace (frontface, te, false);
				AttachEdgeToFace (backface, te, true);

				vertex->second--;
				vertex2->second++;
			}

			for (int side = 0; side < 2; side++)
			{
				RemoveFaceFromLeaf (GetLeafFromFace (tf, side), tf, side);
			}
			DeleteFace (numobjects, tf);
			restart = true;
		}
	}

	// split the leaf
	{
		if (tl->infinite)
		{
			PrintOnce ("SplitTreeLeaf: internal error: splitting the infinite leaf");
			hlassume (false, assume_first);
		}
		front = AllocTreeleaf (numobjects, tl->infinite);
		back = AllocTreeleaf (numobjects, tl->infinite);
		front->clipnode = c0;
		back->clipnode = c1;

		int tmp_side = SIDE_ON;
		for (fi = tl->faces->begin (); fi != tl->faces->end (); fi++)
		{
			if (tmp_side == SIDE_ON)
			{
				tmp_side = fi->f->tmp_side;
			}
			else if (fi->f->tmp_side != SIDE_ON && fi->f->tmp_side != tmp_side)
			{
				tmp_side = SIDE_CROSS;
			}
		}
		
		std::map< btreeedge_t *, int > edges;
		std::map< btreeedge_t *, int >::iterator edge;

		while ((fi = tl->faces->begin ()) != tl->faces->end ())
		{
			btreeface_t *tf = fi->f;
			int side = fi->side;
			RemoveFaceFromLeaf (tl, tf, side); // because we can only store 2 leafs for a face
			
			// fi is unusable now
			if (tf->tmp_side == SIDE_FRONT || tf->tmp_side == SIDE_ON && tmp_side != SIDE_BACK)
			{
				AttachFaceToLeaf (front, tf, side);
			}
			else if (tf->tmp_side == SIDE_BACK || tf->tmp_side == SIDE_ON && tmp_side == SIDE_BACK)
			{
				AttachFaceToLeaf (back, tf, side);

				if (tmp_side == SIDE_CROSS)
				{
					for (ei = tf->edges->begin (); ei != tf->edges->end (); ei++)
					{
						edges[ei->e] += (ei->side == (bool)side? 1: -1);
						edge = edges.find (ei->e);
						if (edge->second == 0)
						{
							edges.erase (edge);
						}
					}
				}
			}
		}

		if (tmp_side == SIDE_CROSS)
		{
			btreeface_t *tf;
			tf = AllocTreeface (numobjects, tl->infinite);
			if (!tf->infinite)
			{
				tf->plane = plane;
				tf->planenum = planenum;
				tf->planeside = false;
			}
			tf->tmp_tested = true;
			tf->tmp_side = SIDE_ON;
			SetFaceLeafs (tf, front, back);
			for (edge = edges.begin (); edge != edges.end (); edge++)
			{
				if (edge->first->tmp_side != SIDE_ON)
				{
					PrintOnce ("SplitTreeLeaf: internal error");
					hlassume (false, assume_first);
				}
				while (edge->second > 0)
				{
					AttachEdgeToFace (tf, edge->first, false);
					edge->second--;
				}
				while (edge->second < 0)
				{
					AttachEdgeToFace (tf, edge->first, true);
					edge->second++;
				}
			}
		}

#ifndef HLBSP_BRINKHACK_BUGFIX
		DeleteLeaf (numobjects, tl);
#endif
#ifdef HLBSP_BRINKNOTUSEDBYLEAF_FIX
		btreeleaf_t *(frontback[2]) = {front, back};
		int side;

		for (side = 0; side < 2; side++)
		{
			for (fi = frontback[side]->faces->begin (); fi != frontback[side]->faces->end (); fi++)
			{
				for (ei = fi->f->edges->begin (); ei != fi->f->edges->end (); ei++)
				{
					ei->e->tmp_onleaf[0] = ei->e->tmp_onleaf[1] = false;
					ei->e->tmp_tested = false;
				}
			}
		}
		for (side = 0; side < 2; side++)
		{
			for (fi = frontback[side]->faces->begin (); fi != frontback[side]->faces->end (); fi++)
			{
				for (ei = fi->f->edges->begin (); ei != fi->f->edges->end (); ei++)
				{
					ei->e->tmp_onleaf[side] = true;
				}
			}
		}
		for (side = 0; side < 2; side++)
		{
			for (fi = frontback[side]->faces->begin (); fi != frontback[side]->faces->end (); fi++)
			{
				for (ei = fi->f->edges->begin (); ei != fi->f->edges->end (); ei++)
				{
					if (ei->e->tmp_tested)
					{
						continue;
					}
					ei->e->tmp_tested = true;
					if (!ei->e->infinite)
					{
						if (ei->e->tmp_onleaf[0] && ei->e->tmp_onleaf[1])
						{
							if (ei->e->tmp_side != SIDE_ON)
							{
								PrintOnce ("SplitTreeLeaf: internal error");
								hlassume (false, assume_first);
							}
							BrinkSplitClipnode (ei->e->brink, plane, planenum, tl->clipnode, c0, c1);
						}
						else if (ei->e->tmp_onleaf[0])
						{
							if (ei->e->tmp_side == SIDE_BACK)
							{
								PrintOnce ("SplitTreeLeaf: internal error");
								hlassume (false, assume_first);
							}
							BrinkReplaceClipnode (ei->e->brink, tl->clipnode, c0);
						}
						else if (ei->e->tmp_onleaf[1])
						{
							if (ei->e->tmp_side == SIDE_FRONT)
							{
								PrintOnce ("SplitTreeLeaf: internal error");
								hlassume (false, assume_first);
							}
							BrinkReplaceClipnode (ei->e->brink, tl->clipnode, c1);
						}
					}
				}
			}
		}
#endif
#ifdef HLBSP_BRINKHACK_BUGFIX
		DeleteLeaf (numobjects, tl);
#endif
	}
}

void BuildTreeCells_r (int &numobjects, bclipnode_t *c)
{
	if (c->isleaf)
	{
		return;
	}
	btreeleaf_t *tl, *front, *back;
	tl = c->treeleaf;
	SplitTreeLeaf (numobjects, tl, c->plane, c->planenum, ON_EPSILON, front, back, c->children[0], c->children[1]);
	c->treeleaf = NULL;
	c->children[0]->treeleaf = front;
	c->children[1]->treeleaf = back;
	BuildTreeCells_r (numobjects, c->children[0]);
	BuildTreeCells_r (numobjects, c->children[1]);
}



typedef struct bbrinkinfo_s
{
	int numclipnodes;
	bclipnode_t *clipnodes;
	int numobjects;
	btreeleaf_t *leaf_outside;
	int numbrinks;
	bbrink_t **brinks;
}
bbrinkinfo_t;

#define MAXCLIPNODES (MAX_MAP_CLIPNODES*8)

bclipnode_t *ExpandClipnodes_r (bclipnode_t *bclipnodes, int &numbclipnodes, const dclipnode_t *clipnodes, int headnode)
{
	if (numbclipnodes >= MAXCLIPNODES)
	{
		Error ("ExpandClipnodes_r: exceeded MAXCLIPNODES");
	}
	bclipnode_t *c = &bclipnodes[numbclipnodes];
	numbclipnodes++;
	if (headnode < 0)
	{
		c->isleaf = true;
		c->content = headnode;
		c->partitions = NULL;
	}
	else
	{
		c->isleaf = false;
		c->planenum = clipnodes[headnode].planenum;
		c->plane = &g_dplanes[c->planenum];
		for (int k = 0; k < 2; k++)
		{
			c->children[k] = ExpandClipnodes_r (bclipnodes, numbclipnodes, clipnodes, clipnodes[headnode].children[k]);
		}
	}
	return c;
}

void ExpandClipnodes (bbrinkinfo_t *info, const dclipnode_t *clipnodes, int headnode)
{
	bclipnode_t *bclipnodes = (bclipnode_t *)malloc (MAXCLIPNODES * sizeof (bclipnode_t)); // 262144 * 30byte = 7.5MB
	hlassume (bclipnodes != NULL, assume_NoMemory);
	info->numclipnodes = 0;
	ExpandClipnodes_r (bclipnodes, info->numclipnodes, clipnodes, headnode);
	info->clipnodes = (bclipnode_t *)malloc (info->numclipnodes * sizeof (bclipnode_t));
	hlassume (info->clipnodes != NULL, assume_NoMemory);
	memcpy (info->clipnodes, bclipnodes, info->numclipnodes * sizeof (bclipnode_t));
	for (int i = 0; i < info->numclipnodes; i++)
	{
		for (int k = 0; k < 2; k++)
		{
			info->clipnodes[i].children[k] = info->clipnodes + (bclipnodes[i].children[k] - bclipnodes);
		}
	}
	free (bclipnodes);
}

void BuildTreeCells (bbrinkinfo_t *info)
{
	info->numobjects = 0;
	info->leaf_outside = BuildOutside (info->numobjects);
	info->clipnodes[0].treeleaf = BuildBaseCell (info->numobjects, &info->clipnodes[0], BOGUS_RANGE, info->leaf_outside);
	BuildTreeCells_r (info->numobjects, &info->clipnodes[0]);
}

void DeleteTreeCells_r (int &numobjects, bclipnode_t *node)
{
	if (node->treeleaf)
	{
		DeleteLeaf (numobjects, node->treeleaf);
		node->treeleaf = NULL;
	}
	if (!node->isleaf)
	{
		DeleteTreeCells_r (numobjects, node->children[0]);
		DeleteTreeCells_r (numobjects, node->children[1]);
	}
}

void DeleteTreeCells (bbrinkinfo_t *info)
{
	DeleteLeaf (info->numobjects, info->leaf_outside);
	info->leaf_outside = NULL;
	DeleteTreeCells_r (info->numobjects, &info->clipnodes[0]);
	if (info->numobjects != 0)
	{
		PrintOnce ("DeleteTreeCells: internal error: numobjects != 0");
		hlassume (false, assume_first);
	}
}

void ClearMarks_r (bclipnode_t *node)
{
	if (node->isleaf)
	{
		btreeface_l::iterator fi;
		btreeedge_l::iterator ei;
		for (fi = node->treeleaf->faces->begin (); fi != node->treeleaf->faces->end (); fi++)
		{
			for (ei = fi->f->edges->begin (); ei != fi->f->edges->end (); ei++)
			{
				ei->e->tmp_tested = false;
			}
		}
	}
	else
	{
		ClearMarks_r (node->children[0]);
		ClearMarks_r (node->children[1]);
	}
}

void CollectBrinks_r (bclipnode_t *node, int &numbrinks, bbrink_t **brinks)
{
	if (node->isleaf)
	{
		btreeface_l::iterator fi;
		btreeedge_l::iterator ei;
		for (fi = node->treeleaf->faces->begin (); fi != node->treeleaf->faces->end (); fi++)
		{
			for (ei = fi->f->edges->begin (); ei != fi->f->edges->end (); ei++)
			{
				if (ei->e->tmp_tested)
				{
					continue;
				}
				ei->e->tmp_tested = true;
				if (!ei->e->infinite)
				{
					if (brinks != NULL)
					{
						brinks[numbrinks] = ei->e->brink;
						brinks[numbrinks]->edge = ei->e;
						for (int i = 0; i < brinks[numbrinks]->numnodes; i++)
						{
							bbrinknode_t *node = &(*brinks[numbrinks]->nodes)[i];
							if (node->isleaf && !node->clipnode->isleaf)
							{
								PrintOnce ("CollectBrinks_r: internal error: not leaf");
								hlassume (false, assume_first);
							}
						}
					}
					numbrinks++;
				}
			}
		}
	}
	else
	{
		CollectBrinks_r (node->children[0], numbrinks, brinks);
		CollectBrinks_r (node->children[1], numbrinks, brinks);
	}
}

void CollectBrinks (bbrinkinfo_t *info)
{
	info->numbrinks = 0;
	ClearMarks_r (&info->clipnodes[0]);
	CollectBrinks_r (&info->clipnodes[0], info->numbrinks, NULL);
	hlassume (info->brinks = (bbrink_t **)malloc (info->numbrinks * sizeof (bbrink_t *)), assume_NoMemory);
	info->numbrinks = 0;
	ClearMarks_r (&info->clipnodes[0]);
	CollectBrinks_r (&info->clipnodes[0], info->numbrinks, info->brinks);
}

void FreeBrinks (bbrinkinfo_t *info)
{
	free (info->brinks);
}

struct bwedge_s;
struct bsurface_s;

typedef struct bwedge_s
{
	int content;
	int nodenum;
	bsurface_s *prev;
	bsurface_s *next;
}
bwedge_t;

typedef struct bsurface_s
{
	vec3_t normal; // pointing clockwise
	int nodenum;
	bool nodeside;
	bwedge_s *prev;
	bwedge_s *next;
}
bsurface_t;

#define MAXBRINKWEDGES 64

typedef struct
{
	vec3_t axis;
	vec3_t basenormal;
	int numwedges[2]; // the front and back side of nodes[0]
	bwedge_t wedges[2][MAXBRINKWEDGES]; // in counterclosewise order
	bsurface_t surfaces[2][MAXBRINKWEDGES]; // the surface between two adjacent wedges
}
bcircle_t;

bool CalculateCircle (bbrink_t *b, bcircle_t *c)
{
	VectorCopy (b->direction, c->axis);
	if (!VectorNormalize (c->axis))
	{
		return false;
	}
	VectorCopy ((*b->nodes)[0].plane->normal, c->basenormal);

	int side, i;
	for (side = 0; side < 2; side++)
	{
		vec3_t facing;
		CrossProduct (c->basenormal, c->axis, facing);
		VectorScale (facing, side? -1: 1, facing);
		if (VectorNormalize (facing) < 1 - 0.01)
		{
			return false;
		}

		// sort the wedges
		c->numwedges[side] = 1;
		c->wedges[side][0].nodenum = (*b->nodes)[0].children[side];
		c->surfaces[side][0].nodenum = 0;
		c->surfaces[side][0].nodeside = !side;
		while (1)
		{
			for (i = 0; i < c->numwedges[side]; i++)
			{
				int nodenum = c->wedges[side][i].nodenum;
				bbrinknode_t *node = &(*b->nodes)[nodenum];
				if (!node->isleaf)
				{
					memmove (&c->wedges[side][i + 1], &c->wedges[side][i], (c->numwedges[side] - i) * sizeof (bwedge_t));
					memmove (&c->surfaces[side][i + 2], &c->surfaces[side][i + 1], (c->numwedges[side] - 1 - i) * sizeof (bsurface_t));
					c->numwedges[side]++;
					bool flipnode = (DotProduct (node->plane->normal, facing) < 0);
					c->wedges[side][i].nodenum = node->children[flipnode];
					c->wedges[side][i + 1].nodenum = node->children[!flipnode];
					c->surfaces[side][i + 1].nodenum = nodenum;
					c->surfaces[side][i + 1].nodeside = flipnode;
					break;
				}
			}
			if (i == c->numwedges[side])
			{
				break;
			}
		}
	}
	if ((c->numwedges[0] + c->numwedges[1]) * 2 - 1 != b->numnodes)
	{
		PrintOnce ("CalculateCircle: internal error 1");
		hlassume (false, assume_first);
	}

	// fill in other information
	for (side = 0; side < 2; side++)
	{
		for (i = 0; i < c->numwedges[side]; i++)
		{
			bwedge_t *w = &c->wedges[side][i];
			bbrinknode_t *node = &(*b->nodes)[w->nodenum];
			if (!node->clipnode->isleaf)
			{
				PrintOnce ("CalculateCircle: internal error: not leaf");
				hlassume (false, assume_first);
			}
			w->content = node->content;
			w->prev = &c->surfaces[side][i];
			w->next = (i == c->numwedges[side] - 1)? &c->surfaces[!side][0]: &c->surfaces[side][i + 1];
			w->prev->next = w;
			w->next->prev = w;
		}
		for (i = 0; i < c->numwedges[side]; i++)
		{
			bsurface_t *s = &c->surfaces[side][i];
			bbrinknode_t *node = &(*b->nodes)[s->nodenum];
			VectorScale (node->plane->normal, s->nodeside? -1: 1, s->normal);
		}
	}

	// check the normals
	for (side = 0; side < 2; side++)
	{
		for (i = 0; i < c->numwedges[side]; i++)
		{
			bwedge_t *w = &c->wedges[side][i];
			if (i == 0 && i == c->numwedges[side] - 1) // 180 degrees
			{
				continue;
			}
			vec3_t v;
			CrossProduct (w->prev->normal, w->next->normal, v);
			if (!VectorNormalize (v) ||
				DotProduct (v, c->axis) < 1 - 0.01)
			{
				return false;
			}
		}
	}
	return true;
}

void PrintCircle (const bcircle_t *c)
{
	Log ("axis %f %f %f\n", c->axis[0], c->axis[1], c->axis[2]);
	Log ("basenormal %f %f %f\n", c->basenormal[0], c->basenormal[1], c->basenormal[2]);
	Log ("numwedges %d %d\n", c->numwedges[0], c->numwedges[1]);
	for (int side = 0; side < 2; side++)
	{
		for (int i = 0; i < c->numwedges[side]; i++)
		{
			const bwedge_t *w = &c->wedges[side][i];
			const bsurface_t *s = &c->surfaces[side][i];
			Log ("surface[%d][%d] nodenum %d nodeside %d normal %f %f %f\n", side, i, s->nodenum, s->nodeside, s->normal[0], s->normal[1], s->normal[2]);
			Log ("wedge[%d][%d] nodenum %d content %d\n", side, i, w->nodenum, w->content);
		}
	}
}

bool AddPartition (bclipnode_t *clipnode, int planenum, bool planeside, int content, bbrinklevel_e brinktype)
{
	// make sure we won't do any harm
	btreeface_l::iterator fi;
	btreeedge_l::iterator ei;
	int side;
	if (!clipnode->isleaf)
	{
		return false;
	}
	bool onback = false;
	for (fi = clipnode->treeleaf->faces->begin (); fi != clipnode->treeleaf->faces->end (); fi++)
	{
		for (ei = fi->f->edges->begin (); ei != fi->f->edges->end (); ei++)
		{
			for (side = 0; side < 2; side++)
			{
				btreepoint_t *tp = GetPointFromEdge (ei->e, side);
				const dplane_t *plane = &g_dplanes[planenum];
				vec_t dist = DotProduct (tp->v, plane->normal) - plane->dist;
				if (planeside? dist < -ON_EPSILON: dist > ON_EPSILON)
				{
					return false;
				}
				if (planeside? dist > ON_EPSILON: dist < -ON_EPSILON)
				{
					onback = true;
				}
			}
		}
	}
	if (!onback)
	{
		return false; // the whole leaf is on the plane, or the leaf doesn't consist of any vertex
	}
	bpartition_t *p = (bpartition_t *)malloc (sizeof (bpartition_t));
	hlassume (p != NULL, assume_NoMemory);
	p->next = clipnode->partitions;
	p->planenum = planenum;
	p->planeside = planeside;
	p->content = content;
	p->type = brinktype;
	clipnode->partitions = p;
	return true;
}

void AnalyzeBrinks (bbrinkinfo_t *info)
{
	int countgood = 0;
	int countinvalid = 0;
	int countskipped = 0;
	int countfixed = 0;
	int i, j, side;
	for (i = 0; i < info->numbrinks; i++)
	{
		bbrink_t *b = info->brinks[i];
		if (b->numnodes <= 5) // quickly reject the most trivial brinks
		{
#ifdef HLBSP_BRINKNOTUSEDBYLEAF_FIX
			if (b->numnodes != 3 && b->numnodes != 5)
			{
				PrintOnce ("AnalyzeBrinks: internal error 1");
				hlassume (false, assume_first);
			}
			// because a brink won't necessarily be split twice after its creation
			if (b->numnodes == 3)
			{
				if (g_developer >= DEVELOPER_LEVEL_FLUFF)
				{
					Developer (DEVELOPER_LEVEL_FLUFF, "Brink wasn't split by the second plane:\n");
					PrintBrink (b);
				}
				countinvalid++;
			}
			else
			{
				countgood++;
			}
			continue;
#else
			hlassume (b->numnodes == 5, assume_first);
			countgood++;
			continue;
#endif
		}
		
		if (b->numnodes > 2 * MAXBRINKWEDGES - 1)
		{
			if (g_developer >= DEVELOPER_LEVEL_MEGASPAM)
			{
				Developer (DEVELOPER_LEVEL_MEGASPAM, "Skipping complicated brink:\n");
				PrintBrink (b);
			}
			countskipped++;
			continue;
		}
		bcircle_t c;
		// build the circle to find out the planes a player may move along
		if (!CalculateCircle (b, &c))
		{
			if (g_developer >= DEVELOPER_LEVEL_FLUFF)
			{
				Developer (DEVELOPER_LEVEL_FLUFF, "CalculateCircle failed for brink:\n");
				PrintBrink (b);
			}
			countinvalid++;
			continue;
		}
		
		int transitionfound[2];
		bsurface_t *transitionpos[2];
		bool transitionside[2];
		for (side = 0; side < 2; side++)
		{
			transitionfound[side] = 0;
			for (j = 1; j < c.numwedges[side]; j++) // we will later consider the surfaces on the first split
			{
				bsurface_t *s = &c.surfaces[side][j];
				if ((s->prev->content == CONTENTS_SOLID) != (s->next->content == CONTENTS_SOLID))
				{
					transitionfound[side]++;
					transitionpos[side] = s;
					transitionside[side] = (s->prev->content == CONTENTS_SOLID);
				}
			}
		}

		if (transitionfound[0] == 0 || transitionfound[1] == 0)
		{
			// at least one side of the first split is completely SOLID or EMPTY. no bugs in this case
			countgood++;
			continue;
		}

		if (transitionfound[0] > 1 || transitionfound[1] > 1 ||
			(c.surfaces[0][0].prev->content == CONTENTS_SOLID) != (c.surfaces[0][0].next->content == CONTENTS_SOLID) ||
			(c.surfaces[1][0].prev->content == CONTENTS_SOLID) != (c.surfaces[1][0].next->content == CONTENTS_SOLID))
		{
			// there must at least 3 transition surfaces now, which is too complicated. just leave it unfixed
			if (g_developer >= DEVELOPER_LEVEL_MEGASPAM)
			{
				Developer (DEVELOPER_LEVEL_MEGASPAM, "Skipping complicated brink:\n");
				PrintBrink (b);
				PrintCircle (&c);
			}
			countskipped++;
			continue;
		}
		
		if (transitionside[1] != !transitionside[0])
		{
			PrintOnce ("AnalyzeBrinks: internal error 2");
			hlassume (false, assume_first);
		}
		bool bfix = false;
		bool berror = false;
		vec3_t vup = {0, 0, 1};
		bool isfloor;
		bool onfloor;
		bool blocking;
		{
			isfloor = false;
			for (int side2 = 0; side2 < 2; side2++)
			{
				vec3_t normal;
				VectorScale (transitionpos[side2]->normal, transitionside[side2]? -1: 1, normal); // pointing from SOLID to EMPTY
				if (DotProduct (normal, vup) > BRINK_FLOOR_THRESHOLD)
				{
					isfloor = true;
				}
			}
		}
		{
			onfloor = false;
			for (int side2 = 0; side2 < 2; side2++)
			{
				btreepoint_t *tp = GetPointFromEdge (b->edge, side2);
#ifdef HLBSP_BRINKHACK_BUGFIX
				if (tp->infinite)
				{
					continue;
				}
#endif
				for (btreeedge_l::iterator ei = tp->edges->begin (); ei != tp->edges->end (); ei++)
				{
					for (btreeface_l::iterator fi = ei->e->faces->begin (); fi != ei->e->faces->end (); fi++)
					{
#ifdef HLBSP_BRINKHACK_BUGFIX
						if (fi->f->infinite || GetLeafFromFace (fi->f, false)->infinite || GetLeafFromFace (fi->f, true)->infinite)
						{
							PrintOnce ("AnalyzeBrinks: internal error: an infinite object contains a finite object");
							hlassume (false, assume_first);
						}
#endif
						for (int side3 = 0; side3 < 2; side3++)
						{
							vec3_t normal;
							VectorScale (fi->f->plane->normal, (fi->f->planeside != (bool)side3)? -1: 1, normal);
							if (DotProduct (normal, vup) > BRINK_FLOOR_THRESHOLD
								&& GetLeafFromFace (fi->f, side3)->clipnode->content == CONTENTS_SOLID
								&& GetLeafFromFace (fi->f, !side3)->clipnode->content != CONTENTS_SOLID)
							{
								onfloor = true;
							}
						}
					}
				}
			}
		}
		// this code does not fix all the bugs, it only aims to fix most of the bugs
		for (side = 0; side < 2; side++)
		{
			bsurface_t *smovement = transitionpos[side];
			bsurface_t *s;
			for (s = transitionside[!side]? &c.surfaces[!side][0]: &c.surfaces[side][0]; ; s = transitionside[!side]? s->next->next: s->prev->prev)
			{
				bwedge_t *w = transitionside[!side]? s->next: s->prev;
				bsurface_t *snext = transitionside[!side]? w->next: w->prev;
				vec3_t tmp;
				vec_t dot;
				CrossProduct (smovement->normal, snext->normal, tmp);
				dot = DotProduct (tmp, c.axis);
				if (transitionside[!side]? dot < 0.01: dot > -0.01)
				{
					break;
				}
				if (w->content != CONTENTS_SOLID)
				{
					break;
				}
				if (snext == (transitionside[!side]? &c.surfaces[side][0]: &c.surfaces[!side][0]))
				{
					Developer (DEVELOPER_LEVEL_ERROR, "AnalyzeBrinks: surface past 0\n");
					break;
				}
				bfix = true;
				{
					if (DotProduct (smovement->normal, s->normal) > 0.01)
					{
						blocking = false;
					}
					else
					{
						blocking = true;
					}
				}
				bclipnode_t *clipnode = (*b->nodes)[w->nodenum].clipnode;
				int planenum = (*b->nodes)[smovement->nodenum].planenum;
				bool planeside = transitionside[!side]? smovement->nodeside: !smovement->nodeside;
				bbrinklevel_e brinktype;
				brinktype = isfloor? (blocking? BrinkFloorBlocking: BrinkFloor): onfloor? (blocking? BrinkWallBlocking: BrinkWall): BrinkAny;
				if (!AddPartition (clipnode, planenum, planeside, CONTENTS_EMPTY, brinktype))
				{
					berror = true;
				}
			}
		}
		if (berror)
		{
			if (g_developer >= DEVELOPER_LEVEL_FLUFF)
			{
				Developer (DEVELOPER_LEVEL_FLUFF, "AddPartition failed for brink:\n");
				PrintBrink (b);
			}
			countinvalid++;
		}
		else if (!bfix)
		{
			countgood++;
		}
		else
		{
			countfixed++;
		}
	}
	Developer (DEVELOPER_LEVEL_MESSAGE, "brinks: good = %d skipped = %d fixed = %d invalid = %d\n", countgood, countskipped, countfixed, countinvalid);
}

void DeleteClipnodes (bbrinkinfo_t *info)
{
	for (int i = 0; i < info->numclipnodes; i++)
	{
#ifdef HLBSP_BRINKHACK_BUGFIX
		if (!info->clipnodes[i].isleaf)
		{
			continue;
		}
#endif
		bpartition_t *p;
		while ((p = info->clipnodes[i].partitions) != NULL)
		{
			info->clipnodes[i].partitions = p->next;
			free (p);
		}
	}
	free (info->clipnodes);
}

void SortPartitions (bbrinkinfo_t *info) // to merge same partition planes and compress clipnodes better if using HLBSP_MERGECLIPNODE
{
	int countfloorblocking = 0;
	int countfloor = 0;
	int countwallblocking = 0;
	int countwall = 0;
	int countany = 0;
	for (int i = 0; i < info->numclipnodes; i++)
	{
		bclipnode_t *clipnode = &info->clipnodes[i];
		if (!clipnode->isleaf)
		{
			continue;
		}
		bpartition_t *current, **pp, *partitions;
		partitions = clipnode->partitions;
		clipnode->partitions = NULL;
		while ((current = partitions) != NULL)
		{
			partitions = current->next;
			if (current->content != CONTENTS_EMPTY)
			{
				PrintOnce ("SortPartitions: content of partition was not empty.");
				hlassume (false, assume_first);
			}
			for (pp = &clipnode->partitions; *pp; pp = &(*pp)->next)
			{
				if ((*pp)->planenum > current->planenum ||
					(*pp)->planenum == current->planenum && (*pp)->planeside >= current->planeside) // normally the planeside should be identical
				{
					break;
				}
			}
			if (*pp && (*pp)->planenum == current->planenum && (*pp)->planeside == current->planeside)
			{
				(*pp)->type = qmin ((*pp)->type, current->type); // pick the lowest (most important) level from the existing partition and the current partition
				free (current);
				continue;
			}
			switch (current->type)
			{
			case BrinkFloorBlocking:
				countfloorblocking++;
				break;
			case BrinkFloor:
				countfloor++;
				break;
			case BrinkWallBlocking:
				countwallblocking++;
				break;
			case BrinkWall:
				countwall++;
				break;
			case BrinkAny:
				countany++;
				break;
			default:
				PrintOnce ("SortPartitions: internal error");
				hlassume (false, assume_first);
				break;
			}
			current->next = *pp;
			*pp = current;
		}
	}
	Developer (DEVELOPER_LEVEL_MESSAGE, "partitions: floorblocking = %d floor = %d wallblocking = %d wall = %d any = %d\n", countfloorblocking, countfloor, countwallblocking, countwall, countany);
}

void *CreateBrinkinfo (const dclipnode_t *clipnodes, int headnode)
{
	bbrinkinfo_t *info;
	try
	{
		hlassume (info = (bbrinkinfo_t *)malloc (sizeof (bbrinkinfo_t)), assume_NoMemory);
		ExpandClipnodes (info, clipnodes, headnode);
		BuildTreeCells (info);
		CollectBrinks (info);
		AnalyzeBrinks (info);
		FreeBrinks (info);
		DeleteTreeCells (info);
		SortPartitions (info);
	}
	catch (std::bad_alloc)
	{
		hlassume (false, assume_NoMemory);
	}
	return info;
}

#ifdef HLBSP_MERGECLIPNODE
extern int count_mergedclipnodes;
typedef std::map< std::pair< int, std::pair< int, int > >, int > clipnodemap_t;
inline clipnodemap_t::key_type MakeKey (const dclipnode_t &c)
{
	return std::make_pair (c.planenum, std::make_pair (c.children[0], c.children[1]));
}
#endif

bool FixBrinks_r_r (const bclipnode_t *clipnode, const bpartition_t *p, bbrinklevel_e level, int &headnode_out, dclipnode_t *begin, dclipnode_t *end, dclipnode_t *&current
#ifdef HLBSP_MERGECLIPNODE
					, clipnodemap_t *outputmap
#endif
					)
{
	while (p && p->type > level)
	{
		p = p->next;
	}
	if (p == NULL)
	{
		headnode_out = clipnode->content;
		return true;
	}
	dclipnode_t *cn;
#ifdef HLBSP_MERGECLIPNODE
	dclipnode_t tmpclipnode;
	cn = &tmpclipnode;
	dclipnode_t *c = current;
	current++;
#else
	if (current >= end)
	{
		return false;
	}
	cn = current;
	current++;
#endif
	cn->planenum = p->planenum;
	cn->children[p->planeside] = p->content;
	int r;
	if (!FixBrinks_r_r (clipnode, p->next, level, r, begin, end, current
#ifdef HLBSP_MERGECLIPNODE
		, outputmap
#endif
		))
	{
		return false;
	}
	cn->children[!p->planeside] = r;
#ifdef HLBSP_MERGECLIPNODE
	clipnodemap_t::iterator output;
	output = outputmap->find (MakeKey (*cn));
	if (g_noclipnodemerge || output == outputmap->end ())
	{
		if (c >= end)
		{
			return false;
		}
		*c = *cn;
		(*outputmap)[MakeKey (*cn)] = c - begin;
		headnode_out = c - begin;
	}
	else
	{
		count_mergedclipnodes++;
		if (current != c + 1)
		{
			Error ("Merge clipnodes: internal error");
		}
		current = c;
		headnode_out = output->second; // use the existing clipnode
	}
#else
	headnode_out = cn - begin;
#endif
	return true;
}

bool FixBrinks_r (const bclipnode_t *clipnode, bbrinklevel_e level, int &headnode_out, dclipnode_t *begin, dclipnode_t *end, dclipnode_t *&current
#ifdef HLBSP_MERGECLIPNODE
				, clipnodemap_t *outputmap
#endif
				)
{
	if (clipnode->isleaf)
	{
		return FixBrinks_r_r (clipnode, clipnode->partitions, level, headnode_out, begin, end, current
#ifdef HLBSP_MERGECLIPNODE
							, outputmap
#endif
							);
	}
	else
	{
		dclipnode_t *cn;
#ifdef HLBSP_MERGECLIPNODE
		dclipnode_t tmpclipnode;
		cn = &tmpclipnode;
		dclipnode_t *c = current;
		current++;
#else
		if (current >= end)
		{
			return false;
		}
		cn = current;
		current++;
#endif
		cn->planenum = clipnode->planenum;
		for (int k = 0; k < 2; k++)
		{
			int r;
			if (!FixBrinks_r (clipnode->children[k], level, r, begin, end, current
#ifdef HLBSP_MERGECLIPNODE
				, outputmap
#endif
				))
			{
				return false;
			}
			cn->children[k] = r;
		}
#ifdef HLBSP_MERGECLIPNODE
		clipnodemap_t::iterator output;
		output = outputmap->find (MakeKey (*cn));
		if (g_noclipnodemerge || output == outputmap->end ())
		{
			if (c >= end)
			{
				return false;
			}
			*c = *cn;
			(*outputmap)[MakeKey (*cn)] = c - begin;
			headnode_out = c - begin;
		}
		else
		{
			count_mergedclipnodes++;
			if (current != c + 1)
			{
				Error ("Merge clipnodes: internal error");
			}
			current = c;
			headnode_out = output->second; // use existing clipnode
		}
#else
		headnode_out = cn - begin;
#endif
		return true;
	}
}

bool FixBrinks (const void *brinkinfo, bbrinklevel_e level, int &headnode_out, dclipnode_t *clipnodes_out, int maxsize, int size, int &size_out)
{
	const bbrinkinfo_t *info = (const bbrinkinfo_t *)brinkinfo;
	dclipnode_t *begin = clipnodes_out;
	dclipnode_t *end = &clipnodes_out[maxsize];
	dclipnode_t *current = &clipnodes_out[size];
#ifdef HLBSP_MERGECLIPNODE
	clipnodemap_t outputmap;
#endif
	int r;
	if (!FixBrinks_r (&info->clipnodes[0], level, r, begin, end, current
#ifdef HLBSP_MERGECLIPNODE
		, &outputmap
#endif
		))
	{
		return false;
	}
	headnode_out = r;
	size_out = current - begin;
	return true;
}

void DeleteBrinkinfo (void *brinkinfo)
{
	bbrinkinfo_t *info = (bbrinkinfo_t *)brinkinfo;
	DeleteClipnodes (info);
	free (info);
}

#endif

