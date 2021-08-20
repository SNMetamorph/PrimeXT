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


node_t	g_outside_node;		// portals outside the world face this

//=============================================================================
/*
=============
AddPortalToNodes
=============
*/
void AddPortalToNodes( portal_t *p, node_t *front, node_t *back )
{
	if( p->nodes[0] || p->nodes[1] )
		COM_FatalError( "AddPortalToNode: allready included\n" );

	p->nodes[0] = front;
	p->next[0] = front->portals;
	front->portals = p;
	
	p->nodes[1] = back;
	p->next[1] = back->portals;
	back->portals = p;
}

/*
=============
RemovePortalFromNode
=============
*/
void RemovePortalFromNode( portal_t *portal, node_t *l )
{
	portal_t	**pp, *t;
	
	// remove reference to the current portal
	pp = &l->portals;
	while( 1 )
	{
		t = *pp;
		if( !t ) COM_FatalError( "RemovePortalFromNode: portal not in leaf\n" );	

		if( t == portal )
			break;

		if( t->nodes[0] == l )
			pp = &t->next[0];
		else if( t->nodes[1] == l )
			pp = &t->next[1];
		else COM_FatalError( "RemovePortalFromNode: portal not bounding leaf\n" );
	}
	
	if( portal->nodes[0] == l )
	{
		*pp = portal->next[0];
		portal->nodes[0] = NULL;
	}
	else if( portal->nodes[1] == l )
	{
		*pp = portal->next[1];	
		portal->nodes[1] = NULL;
	}
}

//============================================================================

void PrintPortal( portal_t *p )
{
	pw( p->winding );
}

/*
================
MakeHeadnodePortals

The created portals will face the global outside_node
================
*/
void MakeHeadnodePortals( node_t *node, const vec3_t mins, const vec3_t maxs )
{
	vec3_t	bounds[2];
	portal_t	*p, *portals[6];
	plane_t	bplanes[6], *pl;
	int	i, j, n;

	// pad with some space so there will never be null volume leafs
	VectorCopy( mins, bounds[0] );
	VectorCopy( maxs, bounds[1] );

	ExpandBounds( bounds[0], bounds[1], SIDESPACE );
	g_outside_node.contents = CONTENTS_SOLID;
	g_outside_node.portals = NULL;

	for( i = 0; i < 3; i++ )
	{
		for( j = 0; j < 2; j++ )
		{
			n = j * 3 + i;

			p = AllocPortal ();
			portals[n] = p;
			
			pl = &bplanes[n];
			memset( pl, 0, sizeof( *pl ));

			if( j )
			{
				pl->normal[i] = -1;
				pl->dist = -bounds[j][i];
			}
			else
			{
				pl->normal[i] = 1;
				pl->dist = bounds[j][i];
			}

			p->planenum = FindFloatPlane( pl->normal, pl->dist );
			p->winding = BaseWindingForPlane( pl->normal, pl->dist );
			AddPortalToNodes( p, node, &g_outside_node );
		}
	}		

	// clip the basewindings by all the other planes
	for( i = 0; i < 6; i++ )
	{
		for( j = 0; j < 6; j++ )
		{
			if( j == i ) continue;
			ChopWindingInPlace( &portals[i]->winding, bplanes[j].normal, bplanes[j].dist, g_prtepsilon );
		}
	}
}

//============================================================================

/*
================
PortalSidesVisible

translucent water support
================
*/
bool PortalSidesVisible( portal_t *p )
{
#if 1
	int	contents0 = p->nodes[0]->contents;
	int	contents1 = p->nodes[1]->contents;

	// can't see through solids
	if( contents0 == CONTENTS_SOLID || contents1 == CONTENTS_SOLID )
		return false;

	// if contents values are the same and not solid, can see through
	if( contents0 == contents1 )
		return true;

	// can't see through func_illusionary_visblocker
	if( contents0 == CONTENTS_VISBLOCKER || contents1 == CONTENTS_VISBLOCKER )
		return false;

	if( IsLiquidContents( contents0 ) && contents1 == CONTENTS_EMPTY )
		return true;

	if( IsLiquidContents( contents1 ) && contents0 == CONTENTS_EMPTY )
		return true;

	return false;
#else
	if( p->nodes[0]->contents == p->nodes[1]->contents )
		return true;

	if( p->nodes[0]->contents != CONTENTS_SOLID && p->nodes[1]->contents != CONTENTS_SOLID
	 && p->nodes[0]->contents != CONTENTS_SKY && p->nodes[1]->contents != CONTENTS_SKY )
		return true;

	return false;
#endif
}

/*
==============================================================================

PORTAL FILE GENERATION

==============================================================================
*/

FILE	*pf;
int	num_visleafs;	// leafs the player can be in
int	num_visportals;
int	c_backward_portals;
int	c_colinear_portals;

static void WritePortalFile_r( node_t *node )
{
	plane_t	*plane, plane2;
	portal_t	*p, *next;
	winding_t	*w;
	int	i;

	if( !FBitSet( node->flags, FNODE_LEAFPORTAL ))
	{
		WritePortalFile_r( node->children[0] );
		WritePortalFile_r( node->children[1] );
		return;
	}
	
	if( node->contents == CONTENTS_SOLID )
		return;

	for( p = node->portals; p != NULL; p = next )
	{
		next = (p->nodes[0] == node) ? p->next[0] : p->next[1];

		if( !p->winding || p->nodes[0] != node )
			continue;

		if( !PortalSidesVisible( p ))
			continue;

		w = p->winding;

		// write out to the file
			
		// sometimes planes get turned around when they are very near
		// the changeover point between different axis.  interpret the
		// plane the same way vis will, and flip the side orders if needed
		WindingPlane( w, plane2.normal, &plane2.dist );
		plane = &g_mapplanes[p->planenum];

		if( DotProduct( plane->normal, plane2.normal ) < ( 1.0 - g_prtepsilon ))
		{
			if( DotProduct( plane->normal, plane2.normal ) > -1.0 + g_prtepsilon )
				c_colinear_portals++;
			else c_backward_portals++;

			fprintf( pf, "%i %i %i ", w->numpoints, p->nodes[1]->visleafnum, p->nodes[0]->visleafnum );
		}
		else
		{
			fprintf( pf, "%i %i %i ", w->numpoints, p->nodes[0]->visleafnum, p->nodes[1]->visleafnum );
		}

		for( i = 0; i < w->numpoints; i++ )
		{
			fprintf( pf, "(%f %f %f) ", w->p[i][0], w->p[i][1], w->p[i][2] );
		}

		fprintf( pf,"\n" );
	}
}
	
/*
================
NumberLeafs_r
================
*/
void NumberLeafs_r( node_t *node )
{
	portal_t	*p;

	if( !FBitSet( node->flags, FNODE_LEAFPORTAL ))
	{	
		// decision node
		node->visleafnum = -99;
		NumberLeafs_r( node->children[0] );
		NumberLeafs_r( node->children[1] );
		return;
	}

	if( node->contents == CONTENTS_SOLID )
	{
		// solid block, viewpoint never inside
		node->visleafnum = -1;
		return;
	}

	node->visleafnum = num_visleafs++;
	
	for( p = node->portals; p != NULL; )
	{
		if( p->nodes[0] == node )
		{
			// only write out from first leaf
			if( p->winding && PortalSidesVisible( p ))
				num_visportals++;
			p = p->next[0];
		}
		else p = p->next[1];		
	}

}

/*
================
CountChildLeafs_r
================
*/
static int CountChildLeafs_r( node_t *node )
{
	if( node->planenum == PLANENUM_LEAF )
	{
		if( FBitSet( node->flags, FNODE_DETAILCONTENTS ))
		{
			// solid
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		// node
		int	count = 0;

		count += CountChildLeafs_r( node->children[0] );
		count += CountChildLeafs_r( node->children[1] );

		return count;
	}
}

/*
================
WriteLeafCount_r
================
*/
static void WriteLeafCount_r( node_t *node )
{
	if( !FBitSet( node->flags, FNODE_LEAFPORTAL ))
	{
		WriteLeafCount_r( node->children[0] );
		WriteLeafCount_r( node->children[1] );
	}
	else
	{
		if( node->contents == CONTENTS_SOLID )
			return;

		int count = CountChildLeafs_r( node );
		fprintf( pf, "%i\n", count );
	}
}

/*
================
WritePortalfile
================
*/
void WritePortalfile( tree_t *tree, bool leaked )
{
	char	shortname[1024];

	num_visleafs = 0;
	num_visportals = 0;
	c_backward_portals = 0;
	c_colinear_portals = 0;

	COM_FileBase( g_portfilename, shortname );

	// set the visleafnum field in every leaf
	// and count the total number of portals
	NumberLeafs_r( tree->headnode );
	
	// write the file
	if( leaked ) MsgDev( D_INFO, "^3BSP probably has a leak, writing portal file:^7 %s.prt\n", shortname );
	else MsgDev( D_INFO, "^2BSP generation successful, writing portal file:^7 %s.prt\n", shortname );
	pf = fopen( g_portfilename, "w" );
	if( !pf ) COM_FatalError( "error opening %s\n", g_portfilename );

	fprintf( pf, "%i\n", num_visleafs );
	fprintf( pf, "%i\n", num_visportals );

	WriteLeafCount_r( tree->headnode );
	WritePortalFile_r( tree->headnode );
	
	fclose( pf );

	MsgDev( D_REPORT, "%i colinear portals\n", c_colinear_portals );
	MsgDev( D_REPORT, "%i backward portals\n", c_backward_portals );
}


//===================================================
void FreePortals( node_t *node )
{
	portal_t	*p, *nextp;

	if( node->children[0] )
		FreePortals( node->children[0] );

	if( node->children[1] )
		FreePortals( node->children[1] );

	for( p = node->portals; p != NULL; p = nextp )
	{
		if( p->nodes[0] == node )
			nextp = p->next[0];
		else nextp = p->next[1];

		RemovePortalFromNode( p, p->nodes[0] );
		RemovePortalFromNode( p, p->nodes[1] );
		FreeWinding( p->winding );
		FreePortal( p );
	}
}

/*
==================
FreeTreePortals
==================
*/
void FreeTreePortals( tree_t *tree )
{
	FreePortals( tree->headnode );
}