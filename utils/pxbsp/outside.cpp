/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "bsp5.h"

int	outleafs;
int	valid;
int	c_falsenodes;
int	c_free_faces;
int	c_keep_faces;
portal_t	*prevleaknode;
FILE	*pointfile = NULL;
FILE	*linefile = NULL;
int	hit_occupied;
int	backdraw;

/*
===========
PointInLeaf
===========
*/
node_t *PointInLeaf( node_t *node, const vec3_t point )
{
	while( !FBitSet( node->flags, FNODE_LEAFPORTAL ))
		node = node->children[PlaneDiff( point, &g_mapplanes[node->planenum] ) <= 0];
	return node;
}

/*
===========
PlaceOccupant
===========
*/
bool PlaceOccupant( int num, vec3_t point, node_t *headnode )
{
	node_t	*n;
	
	n = PointInLeaf( headnode, point );
	if( n->contents == CONTENTS_SOLID )
		return false;
	n->occupied = num;

	return true;
}

/*
=============
Portal_Passable

Returns true if the portal has non-opaque leafs on both sides
 
from q3map
=============
*/
static bool Portal_Passable( const portal_t *p )
{
	if( p->nodes[0] == &g_outside_node || p->nodes[1] == &g_outside_node )
		return false;

#if 0	// this break monster navigation
	if( p->nodes[0]->opaque() || p->nodes[1]->opaque( ))
		return false;
#endif
	return true;
}

/*
==============
MarkLeakTrail
==============
*/
void MarkLeakTrail( portal_t *n2 )
{
	vec3_t	p1, p2, dir;
	vec_t	len;
	portal_t	*n1;

	n1 = prevleaknode;
	prevleaknode = n2;
	if( !n1 ) return;

	WindingCenter( n1->winding, p1 );
	WindingCenter( n2->winding, p2 );

	// NOTE: create only if there was a leak.
	if( !pointfile ) pointfile = fopen( g_pointfilename, "w" );
	if( !pointfile ) COM_FatalError( "couldn't open %s\n", g_pointfilename );
	if( !linefile ) linefile = fopen( g_linefilename, "w" );
	if( !linefile ) COM_FatalError( "couldn't open %s\n", g_linefilename );

	fprintf( linefile, "%f %f %f - %f %f %f\n", p1[0], p1[1], p1[2], p2[0], p2[1], p2[2] );
	fprintf( pointfile, "%f %f %f\n", p1[0], p1[1], p1[2] );
	
	VectorSubtract( p2, p1, dir );
	len = VectorLength( dir );
	VectorNormalize( dir );

	while( len > 2.0 )
	{
		fprintf( pointfile,"%f %f %f\n", p1[0], p1[1], p1[2] );
		VectorMA( p1, 2.0, dir, p1 );
		len -= 2.0;
	}

}

/*
==============
FreeDetailNode_r
==============
*/
static void FreeDetailNode_r( node_t *node )
{
	face_t	*f, *next;

	if( node->planenum == PLANENUM_LEAF )
	{
		if(!( FBitSet( node->flags, FNODE_LEAFPORTAL ) && node->contents == CONTENTS_SOLID ))
		{
			Mem_Free( node->markfaces );
			node->markfaces = NULL;
		}
		return;
	}

	for( int i = 0; i < 2; i++ )
	{
		FreeDetailNode_r( node->children[i] );
		FreeNode( node->children[i] );
		node->children[i] = NULL;
	}

	for( f = node->faces; f != NULL; f = next )
	{
		next = f->next;
		FreeFace( f );
	}

	node->faces = NULL;
}

/*
==============
FillLeaf
==============
*/
static void FillLeaf( node_t *l )
{
	if( !FBitSet( l->flags, FNODE_LEAFPORTAL ))
	{
		MsgDev( D_WARN, "FillLeaf: not leaf\n" );
		return;
	}

	if( l->contents == CONTENTS_SOLID )
	{
		MsgDev( D_WARN, "FillLeaf: fill solid\n" );
		return;
	}

	FreeDetailNode_r( l );

	l->contents = CONTENTS_SOLID;
	l->planenum = PLANENUM_LEAF;
}

/*
==================
RecursiveFillOutside

If fill is false, just check, don't fill
Returns true if an occupied leaf is reached
==================
*/
bool RecursiveFillOutside( node_t *l, bool fill, bool leakfile )
{
	portal_t	*p;
	int	s;

	if( l->opaque() )
		return false;
		
	if( l->valid == valid )
		return false;
	
	if( l->occupied )
	{
		hit_occupied = l->occupied;
		backdraw = 1000;
		return true;
	}
	
	l->valid = valid;

	// fill it and it's neighbors
	if( fill ) FillLeaf( l );

	outleafs++;

	for( p = l->portals; p != NULL; )
	{
		s = (p->nodes[0] == l);

		if( Portal_Passable( p ) && RecursiveFillOutside( p->nodes[s], fill, leakfile ))
		{	
			// leaked, so stop filling
			if( backdraw-- > 0 && leakfile )
				MarkLeakTrail( p );
			return true;
		}
		p = p->next[!s];
	}
	
	return false;
}

static void MarkFacesInside_r( node_t *node )
{
	if( node->planenum == PLANENUM_LEAF )
	{
		for( face_t **fp = node->markfaces; *fp != NULL; fp++ )
			(*fp)->outputnumber = 0;
	}
	else
	{
		MarkFacesInside_r( node->children[0] );
		MarkFacesInside_r( node->children[1] );
	}
}

/*
==================
ClearOutFaces_r

Removes unused nodes
==================
*/
node_t *ClearOutFaces_r( node_t *node )
{
	face_t	*f, *fnext;
	portal_t	*p;

	// mark the node and all it's faces, so they
	// can be removed if no children use them
	node->valid = 0; // will be set if any children touch it

	for( f = node->faces; f != NULL; f = f->next )
		f->outputnumber = -1;

	// go down the children
	if( !FBitSet( node->flags, FNODE_LEAFPORTAL ))
	{
		// decision node
		node->children[0] = ClearOutFaces_r( node->children[0] );
		node->children[1] = ClearOutFaces_r( node->children[1] );

		// free any faces not in open child leafs
		f = node->faces;
		node->faces = NULL;

		for( ; f != NULL; f = fnext )
		{
			fnext = f->next;

			if( f->outputnumber == -1 )
			{	
				// never referenced, so free it
				c_free_faces++;
				FreeFace( f );
			}
			else
			{
				c_keep_faces++;
				f->next = node->faces;
				node->faces = f;
			}
		}

		// TODO: free memory
		if( !node->valid )
		{
			// this node does not touch any interior leafs
			// if both children are solid, just make this node solid
			if( node->children[0]->contents == CONTENTS_SOLID && node->children[1]->contents == CONTENTS_SOLID )
			{
				SetBits( node->flags, FNODE_LEAFPORTAL );
				node->contents = CONTENTS_SOLID;
				node->planenum = PLANENUM_LEAF;
				c_splitnodes--;
				return node;
			}

			// if one child is solid, shortcut down the other side
			if( node->children[0]->contents == CONTENTS_SOLID )
			{
				c_splitnodes--;
				return node->children[1];
			}

			if( node->children[1]->contents == CONTENTS_SOLID )
			{
				c_splitnodes--;
				return node->children[0];
			}
			c_falsenodes++;
		}
		return node;
	}

	// leaf node
	if( node->contents != CONTENTS_SOLID )
	{
		// this node is still inside

		// mark all the nodes used as portals
		for( p = node->portals; p != NULL; )
		{
			if( p->onnode )
				p->onnode->valid = 1;

			// only write out from first leaf
			if( p->nodes[0] == node )
				p = p->next[0];
			else p = p->next[1];		
		}

		MarkFacesInside_r( node );

		return node;
	}

	return node;
}

/*
===========
FillOutside

===========
*/
bool FillOutside( tree_t *tree, int hullnum, bool leakfile )
{
	bool	inside = false;
	node_t	*node = tree->headnode;
	bool	leaked = false;
	int	s, i, c_nodes;
	vec3_t	origin;

	MsgDev( D_REPORT, "----- FillOutside ----\n" );
	c_nodes = c_splitnodes;

	// place markers for all entities so
	// we know if we leak inside
	for( i = 1; i < g_numentities; i++ )
	{
		entity_t	*mapent = &g_entities[i];

		GetVectorForKey( mapent, "origin", origin );

		// make sure what entity doesn't have brush-model
		if( !VectorIsNull( origin ) && ValueForKey( mapent, "model" )[0] != '*' )
		{
			origin[2] += 1.0;

			if( PlaceOccupant( i, origin, node ))
				inside = true;
		}
	}

	if( !inside )
	{
		Msg( "%i nodes (not optimized)\n", c_nodes );
		MsgDev( D_ERROR, "No entities in empty space -- no filling performed\n" );
		return false;
	}

	s = !(g_outside_node.portals->nodes[1] == &g_outside_node);
	// first check to see if an occupied leaf is hit
	prevleaknode = NULL;
	outleafs = 0;
	valid++;

	if( RecursiveFillOutside( g_outside_node.portals->nodes[s], false, leakfile ))
	{
		if( pointfile ) fclose( pointfile );
		if( linefile ) fclose( linefile );
		pointfile = linefile = NULL;
		leaked = true;

		// do animation
		MsgAnim(D_INFO, "^3=== LEAK in hull %i ===\n", hullnum);
		GetVectorForKey(&g_entities[hit_occupied], "origin", origin);
		MsgDev(D_REPORT, "\nEntity %i @ (%4.0f,%4.0f,%4.0f)\n", hit_occupied, origin[0], origin[1], origin[2]);
		MsgDev(D_REPORT, "no filling performed\n");
		MsgDev(D_REPORT, "point file and line file generated\n");

		// allow to vis maps even with leak
		if( g_forcevis == false )
		{
			Msg( "%i nodes (not optimized)\n", c_nodes );
			return false;
		}
	}

	// now go back and fill things in
	valid++;
	RecursiveFillOutside( g_outside_node.portals->nodes[s], true, false );

	// remove faces and nodes from filled in leafs	
	c_falsenodes = 0;
	c_free_faces = 0;
	c_keep_faces = 0;
	tree->headnode = ClearOutFaces_r( node );

	MsgDev( D_REPORT, "%5i outleafs\n", outleafs );
	MsgDev( D_REPORT, "%5i freed faces\n", c_free_faces );
	MsgDev( D_REPORT, "%5i keep faces\n", c_keep_faces );
	MsgDev( D_REPORT, "%5i falsenodes\n", c_falsenodes );
	Msg( "%i nodes (%i after merging)\n", c_nodes, c_splitnodes );

	// save portal file for vis tracing
	if( leakfile ) WritePortalfile( tree, leaked );

	return true;
}

//=============================================================================

/*
===========
ResetMark_r
===========
*/
void ResetMark_r( node_t *node )
{
	if( FBitSet( node->flags, FNODE_LEAFPORTAL ))
	{
		if( node->opaque( ))
			ClearBits( node->flags, FNODE_EMPTY );
		else SetBits( node->flags, FNODE_EMPTY );
	}
	else
	{
		ResetMark_r( node->children[0] );
		ResetMark_r( node->children[1] );
	}
}

/*
===========
MarkOccupied_r
===========
*/
void MarkOccupied_r( node_t *node )
{
	portal_t	*p;
	int	s;

	if( FBitSet( node->flags, FNODE_EMPTY ))
	{
		ClearBits( node->flags, FNODE_EMPTY );

		for( p = node->portals; p != NULL; p = p->next[!s] )
		{
			s = (p->nodes[0] == node);
			MarkOccupied_r( p->nodes[s] );
		}
	}
}

/*
===========
RemoveUnused_r
===========
*/
void RemoveUnused_r( node_t *node )
{
	if( FBitSet( node->flags, FNODE_LEAFPORTAL ))
	{
		if( FBitSet( node->flags, FNODE_EMPTY ))
		{
			FillLeaf( node );
		}
	}
	else
	{
		RemoveUnused_r( node->children[0] );
		RemoveUnused_r( node->children[1] );
	}
}

/*
===========
FillInside

===========
*/
void FillInside( node_t *node )
{
	vec3_t	origin;
	node_t	*innode;

	ClearBits( g_outside_node.flags, FNODE_EMPTY );
	ResetMark_r( node );

	for( int i = 1; i < g_numentities; i++ )
	{
		entity_t	*mapent = &g_entities[i];

		if( CheckKey( mapent, "origin" ))
		{
			GetVectorForKey( mapent, "origin", origin );

			origin[2] += 1.0;
			innode = PointInLeaf( node, origin );
			MarkOccupied_r( innode );

			origin[2] -= 2.0;
			innode = PointInLeaf( node, origin );
			MarkOccupied_r( innode );
		}
	}

	RemoveUnused_r( node );
}