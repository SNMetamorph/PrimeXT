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

#define MarkFacesCount( mf )	( (mf) ? mf->count : 0 )

typedef struct
{
	face_t		**array;
	int		count;
	int		reserve;
} markfaces_t;

typedef struct surfnode_s
{
	int		size;		// can be zero, which invalidates mins and maxs
	int		size_discardable;
	vec3_t		mins;
	vec3_t		maxs;
	bool		isleaf;

	// node
	surfnode_s	*children[2];
	markfaces_t	*nodefaces;
	int		nodefaces_discardablesize;

	// leaf
	markfaces_t	*leaffaces;
} surfnode_t;

typedef struct
{
	bool		dontbuild;
	vec_t		epsilon;		// if a face is not epsilon far from the splitting plane, put it in result.middle
	surfnode_t	*headnode;

	// result
	int		frontsize;
	int		backsize;
	markfaces_t	*middle;	// may contains coplanar faces and discardable(SOLIDHINT) faces
} surftree_t;

static double	g_splitvalue[MAX_INTERNAL_MAP_PLANES][2];

// organize all surfaces into a tree structure to accelerate intersection test
// can reduce more than 90% compile time for very complicated maps
surfnode_t *AllocSurfNode( void )
{
	return (surfnode_t *)Mem_Alloc( sizeof( surfnode_t ), C_LEAFNODE );
}

void FreeSurfNode( surfnode_t *node )
{
	Mem_Free( node, C_LEAFNODE );
}

markfaces_t *AllocMarkFaces( void )
{
	return (markfaces_t *)Mem_Alloc( sizeof( markfaces_t ));
}

void FreeMarkFaces( markfaces_t **mf )
{
	if( !mf || !*mf ) return;
	if((*mf)->array )
		Mem_Free((*mf)->array );
	Mem_Free( *mf );
	*mf = NULL;
}

void InsertMarkFace( markfaces_t *mf, face_t *f )
{
	if( !mf ) return;

	if( mf->reserve <= 0 )
	{
		mf->reserve = 256;	// alloc reserve to avoid realloc on each face
		mf->array = (face_t **)Mem_Realloc( mf->array, sizeof( face_t* ) * ( mf->count + mf->reserve + 1 ));
	}

	mf->array[mf->count++] = f;
	mf->array[mf->count] = NULL;
	mf->reserve--;
}

void ClearMarkFaces( markfaces_t *mf )
{
	if( !mf ) return;

	if( mf->array )
	{
		Mem_Free( mf->array );
		mf->array = NULL;
	}
	mf->reserve = mf->count = 0;
}

void BuildSurfaceTree_r( surftree_t *tree, surfnode_t *node )
{
	face_t	*f, **fp;

	node->size = MarkFacesCount( node->leaffaces );
	node->size_discardable = 0;

	if( node->size == 0 )
	{
		node->isleaf = true;
		return;
	}

	ClearBounds( node->mins, node->maxs );

	for( fp = node->leaffaces->array; fp && *fp != NULL; fp++ )
	{
		f = *fp;
		WindingBounds( f->w, node->mins, node->maxs, true );

		if( f->facestyle == face_discardable )
			node->size_discardable++;
	}

	int	bestaxis = -1;
	vec_t	bestdelta = 0;

	for( int k = 0; k < 3; k++ )
	{
		if( node->maxs[k] - node->mins[k] > bestdelta + BSPCHOP_EPSILON )
		{
			bestdelta = node->maxs[k] - node->mins[k];
			bestaxis = k;
		}
	}

	if( node->size <= 5 || tree->dontbuild || bestaxis == -1 )
	{
		node->isleaf = true;
		return;
	}

	vec_t	dist, dist1, dist2;

	node->isleaf = false;
	dist = (node->mins[bestaxis] + node->maxs[bestaxis]) * 0.5;
	dist1 = (3 * node->mins[bestaxis] + node->maxs[bestaxis]) * 0.25;
	dist2 = (node->mins[bestaxis] + 3 * node->maxs[bestaxis]) * 0.25;

	// each child node is at most 3/4 the size of the parent node.
	// Most faces should be passed to a child node, faces left in the
	// parent node are the ones whose dimensions are large enough to
	// be comparable to the dimension of the parent node.
	node->nodefaces = AllocMarkFaces();
	node->nodefaces_discardablesize = 0;

	node->children[0] = AllocSurfNode();
	node->children[0]->leaffaces = AllocMarkFaces();
	node->children[1] = AllocSurfNode();
	node->children[1]->leaffaces = AllocMarkFaces();

	for( fp = node->leaffaces->array; fp && *fp != NULL; fp++ )
	{
		f = *fp;

		winding_t	*w = f->w;
		vec_t	low = BOGUS_RANGE;
		vec_t	high = -BOGUS_RANGE;

		if( !w || w->numpoints < 3 )
			continue;

		for( int x = 0; x < w->numpoints; x++ )
		{
			low = Q_min( low, w->p[x][bestaxis] );
			high = Q_max( high, w->p[x][bestaxis] );
		}

		if( low < dist1 + BSPCHOP_EPSILON && high > dist2 - BSPCHOP_EPSILON )
		{
			InsertMarkFace( node->nodefaces, f );

			if( f->facestyle == face_discardable )
				node->nodefaces_discardablesize++;
		}
		else if( low >= dist1 && high <= dist2 )
		{
			if(( low + high ) * 0.5 > dist )
			{
				InsertMarkFace( node->children[0]->leaffaces, f );
			}
			else
			{
				InsertMarkFace( node->children[1]->leaffaces, f );
			}
		}
		else if( low >= dist1 )
		{
			InsertMarkFace( node->children[0]->leaffaces, f );
		}
		else if( high <= dist2 )
		{
			InsertMarkFace( node->children[1]->leaffaces, f );
		}
	}

	int leafcount = MarkFacesCount( node->leaffaces );

	if( MarkFacesCount( node->children[0]->leaffaces ) == leafcount || MarkFacesCount( node->children[1]->leaffaces ) == leafcount )
	{
		MsgDev( D_WARN, "BuildSurfaceTree_r: didn't split the node\n" );

		FreeMarkFaces( &node->children[0]->leaffaces );
		FreeMarkFaces( &node->children[1]->leaffaces );
		FreeSurfNode( node->children[0] );
		FreeSurfNode( node->children[1] );
		FreeMarkFaces( &node->nodefaces );
		node->isleaf = true;
		return;
	}

	FreeMarkFaces( &node->leaffaces );
	BuildSurfaceTree_r( tree, node->children[0] );
	BuildSurfaceTree_r( tree, node->children[1] );
}

surftree_t *BuildSurfaceTree( surface_t *surfaces, vec_t epsilon )
{
	surftree_t	*tree = (surftree_t *)Mem_Alloc( sizeof( surftree_t ), C_BSPTREE );
	surface_t		*p2;
	face_t		*f;

	tree->headnode = AllocSurfNode();
	tree->headnode->leaffaces = AllocMarkFaces();
	tree->middle = AllocMarkFaces();
	tree->epsilon = epsilon;

	for( p2 = surfaces; p2 != NULL; p2 = p2->next )
	{
		if( p2->onnode )
			continue;

		for( f = p2->faces; f != NULL; f = f->next )
		{
			InsertMarkFace( tree->headnode->leaffaces, f );
			InsertMarkFace( tree->middle, f );
		}
	}

	tree->dontbuild = MarkFacesCount( tree->headnode->leaffaces ) < 20;
	BuildSurfaceTree_r( tree, tree->headnode );

	if( tree->dontbuild )
	{
		tree->backsize = 0;
		tree->frontsize = 0;
	}
	else ClearMarkFaces( tree->middle );

	return tree;
}

void TestSurfaceTree_r( surftree_t *tree, const surfnode_t *node, const plane_t *split )
{
	vec_t	low, high;
	face_t	**fp;

	if( node->size == 0 )
		return;

	low = high = -split->dist;

	for( int k = 0; k < 3; k++ )
	{
		if( split->normal[k] >= 0 )
		{
			high += split->normal[k] * node->maxs[k];
			low += split->normal[k] * node->mins[k];
		}
		else
		{
			high += split->normal[k] * node->mins[k];
			low += split->normal[k] * node->maxs[k];
		}
	}

	if( low > tree->epsilon )
	{
		tree->frontsize += node->size;
		tree->frontsize -= node->size_discardable;
		return;
	}

	if( high < -tree->epsilon )
	{
		tree->backsize += node->size;
		tree->backsize -= node->size_discardable;
		return;
	}

	if( node->isleaf )
	{
		for( fp = node->leaffaces->array; fp && *fp != NULL; fp++ )
		{
			InsertMarkFace( tree->middle, *fp );
		}
	}
	else
	{
		for( fp = node->nodefaces->array; fp && *fp != NULL; fp++ )
		{
			InsertMarkFace( tree->middle, *fp );
		}

		TestSurfaceTree_r( tree, node->children[0], split );
		TestSurfaceTree_r( tree, node->children[1], split );
	}
}

void TestSurfaceTree( surftree_t *tree, const plane_t *split )
{
	if( tree->dontbuild )
		return;

	ClearMarkFaces( tree->middle );
	tree->backsize = tree->frontsize = 0;
	TestSurfaceTree_r( tree, tree->headnode, split );
}

void DeleteSurfaceTree_r( surfnode_t *node )
{
	if( node->isleaf )
	{
		FreeMarkFaces( &node->leaffaces );
	}
	else
	{
		DeleteSurfaceTree_r( node->children[0] );
		FreeSurfNode( node->children[0] );
		DeleteSurfaceTree_r( node->children[1] );
		FreeSurfNode( node->children[1] );
		FreeMarkFaces( &node->nodefaces );
	}
}

void DeleteSurfaceTree( surftree_t *tree )
{
	DeleteSurfaceTree_r( tree->headnode );
	FreeSurfNode( tree->headnode );
	FreeMarkFaces( &tree->middle );
	Mem_Free( tree, C_BSPTREE );
}

/*
==================
FaceSide

For BSP hueristic
==================
*/
static int FaceSide( const face_t *in, const plane_t *split, double *epsilonsplit = NULL )
{
	vec_t	d_front = 0.0;
	vec_t	d_back = 0.0;
	vec_t	dot;
	winding_t	*w;
	vec_t	*p;
	int	i;

	ASSERT( in && in->w );

	w = in->w;
	
	// axial planes are fast
	if( split->type <= PLANE_LAST_AXIAL )
	{
		for( i = 0, p = w->p[0] + split->type; i < w->numpoints; i++, p += 3 )
		{
			dot = *p - split->dist;
			d_front = Q_max( dot, d_front );
			d_back = Q_min( dot, d_back );
		}
	}
	else	
	{
		// sloping planes take longer
		for( i = 0, p = w->p[0]; i < w->numpoints; i++, p += 3 )
		{
			dot = DotProduct( p, split->normal ) - split->dist;
			d_front = Q_max( dot, d_front );
			d_back = Q_min( dot, d_back );
		}
	}	

	if( d_front <= BSPCHOP_EPSILON )
	{
		if( epsilonsplit && ( d_front > MINSPLIT_EPSILON || d_back > -MAXSPLIT_EPSILON ))
			(*epsilonsplit)++;
		return SIDE_BACK;
	}

	if( d_back >= -BSPCHOP_EPSILON )
	{
		if( epsilonsplit && ( d_back < -MINSPLIT_EPSILON || d_front < MAXSPLIT_EPSILON ))
			(*epsilonsplit)++;
		return SIDE_FRONT;
	}

	if( epsilonsplit && ( d_front < MAXSPLIT_EPSILON || d_back > -MAXSPLIT_EPSILON ))
		(*epsilonsplit)++;

	return SIDE_ON;
}

/*
==================
ChooseMidPlaneFromList

When there are a huge number of planes, just choose one closest
to the middle.
==================
*/
surface_t *ChooseMidPlaneFromList( surface_t *surfaces, const vec3_t mins, const vec3_t maxs, int detaillevel )
{
	surface_t		*p, *bestsurface;
	vec_t		bestvalue, value;
	plane_t		*plane;
	surftree_t	*surfacetree;
	vec_t		dist;
	face_t		*f, **fp;

	surfacetree = BuildSurfaceTree( surfaces, BSPCHOP_EPSILON );

	// pick the plane that splits the least
	bestsurface = NULL;
	bestvalue = 9e30;

	for( p = surfaces; p != NULL; p = p->next )
	{
		if( p->onnode || p->detaillevel != detaillevel )
			continue;

		plane = &g_mapplanes[p->planenum];

		// check for axis aligned surfaces
		int l = plane->type;

		if( l > PLANE_LAST_AXIAL )
			continue;

		// calculate the split metric along axis l, smaller values are better
		value = 0;

		dist = plane->dist * plane->normal[l];

		if( maxs[l] - dist < g_maxnode_size / 2.0 - ON_EPSILON || dist - mins[l] < g_maxnode_size / 2.0 - ON_EPSILON )
			continue;

		double	crosscount = 0;
		double	frontcount = 0;
		double	backcount = 0;
		double	coplanarcount = 0;

		TestSurfaceTree( surfacetree, plane );
		frontcount += surfacetree->frontsize;
		backcount += surfacetree->backsize;

		for( fp = surfacetree->middle->array; fp && *fp != NULL; fp++ )
		{
			f = *fp;

			if( f->facestyle == face_discardable )
				continue;

			if( f->planenum == p->planenum || f->planenum == ( p->planenum ^ 1 ))
			{
				coplanarcount++;
				continue;
			}

			switch( FaceSide( f, plane ))
			{
			case SIDE_FRONT:
				frontcount++;
				break;
			case SIDE_BACK:
				backcount++;
				break;
			case SIDE_ON:
				crosscount++;
				break;
			}
		}

		double	frontsize = frontcount + 0.5 * coplanarcount + 0.5 * crosscount;
		double	frontfrac = (maxs[l] - dist) / (maxs[l] - mins[l]);
		double	backsize = backcount + 0.5 * coplanarcount + 0.5 * crosscount;
		double	backfrac = (dist - mins[l]) / (maxs[l] - mins[l]);

		value = crosscount + 0.1 * (frontsize * (log( frontfrac ) / log( 2.0 )) + backsize * ( log( backfrac ) / log( 2.0 )));
		// the first part is how the split will increase the number of faces
		// the second part is how the split will increase the average depth of the bsp tree

		if( value > bestvalue )
			continue;

		// currently the best!
		bestvalue = value;
		bestsurface = p;
	}

	DeleteSurfaceTree( surfacetree );

	return bestsurface;
}

/*
==================
ChoosePlaneFromList

Choose the plane that splits the least faces
==================
*/
surface_t *ChoosePlaneFromList( surface_t *surfaces, const vec3_t mins, const vec3_t maxs, int detaillevel )
{
	surface_t		*p, *bestsurface;
	vec_t		value, bestvalue;
	double		totalsplit;
	double		avesplit;
	double		planecount;
	surftree_t*	surfacetree;
	plane_t		*plane;
	face_t		*f, **fp;

	planecount = totalsplit = 0;
	surfacetree = BuildSurfaceTree( surfaces, BSPCHOP_EPSILON );

	// pick the plane that splits the least
	bestvalue = 9e30;
	bestsurface = NULL;

	for( p = surfaces; p != NULL; p = p->next )
	{
		if( p->onnode || p->detaillevel != detaillevel )
			continue;

		planecount++;

		double	crosscount = 0;
		double	frontcount = 0;
		double	backcount = 0;
		double	coplanarcount = 0;
		double	epsilonsplit = 0;

		plane = &g_mapplanes[p->planenum];

		for( f = p->faces; f != NULL; f = f->next )
		{
			if( f->facestyle == face_discardable )
				continue;
			coplanarcount++;
		}

		TestSurfaceTree( surfacetree, plane );

		frontcount += surfacetree->frontsize;
		backcount += surfacetree->backsize;

		for( fp = surfacetree->middle->array; fp && *fp != NULL; fp++ )
		{
			f = *fp;

			if( f->planenum == p->planenum || f->planenum == ( p->planenum ^ 1 ))
				continue;

			if( f->facestyle == face_discardable )
			{
				FaceSide( f, plane, &epsilonsplit );
				continue;
			}

			switch( FaceSide( f, plane, &epsilonsplit ))
			{
			case SIDE_FRONT:
				frontcount++;
				break;
			case SIDE_BACK:
				backcount++;
				break;
			case SIDE_ON:
				totalsplit++;
				crosscount++;
				break;
			}
		}

		value = crosscount - sqrt( coplanarcount ); // Not optimized. --vluzacn
		if( coplanarcount == 0 ) crosscount += 1;

		// This is the most efficient code among what I have ever tested:
		// (1) BSP file is small, despite possibility of slowing down vis and rad
		// (but still faster than the original non BSP balancing method).
		// (2) Factors need not adjust across various maps.
		double	frac = (coplanarcount / 2 + crosscount / 2 + frontcount) / (coplanarcount + frontcount + backcount + crosscount);
		double	ent = 0.0;

		if( frac > 0.0001 && frac < 0.9999 )
			ent = (-frac * log( frac ) / log( 2.0 ) - (1.0 - frac) * log( 1.0 - frac ) / log( 2.0 ));
		g_splitvalue[p->planenum][1] = crosscount * (1.0 - ent);
		value += epsilonsplit * 10000;
		g_splitvalue[p->planenum][0] = value;
	}

	avesplit = totalsplit / planecount;

	for( p = surfaces; p != NULL; p = p->next )
	{
		if( p->onnode || p->detaillevel != detaillevel )
			continue;

		value = g_splitvalue[p->planenum][0] + avesplit * g_splitvalue[p->planenum][1];

		if( value < bestvalue )
		{
			bestvalue = value;
			bestsurface = p;
		}
	}

	if( !bestsurface )
		COM_FatalError( "ChoosePlaneFromList: no valid planes\n" );

	DeleteSurfaceTree( surfacetree );

	return bestsurface;
}

/*
==================
SelectPartition

Selects a surface from a linked list of surfaces to split the group on
returns NULL if the surface list can not be divided any more (a leaf)
==================
*/
surface_t *SelectPartition( surface_t *surfaces, node_t *node, bool midsplit, int splitdetaillevel, vec3_t validmins, vec3_t validmaxs )
{
	surface_t	*p;

	if( splitdetaillevel == -1 )
		return NULL;

	if( midsplit )
	{
		// do fast way for clipping hull
		if(( p = ChooseMidPlaneFromList( surfaces, validmins, validmaxs, splitdetaillevel )) != NULL )
			return p;
	}
		
	// do slow way to save poly splits for drawing hull
	return ChoosePlaneFromList( surfaces, node->mins, node->maxs, splitdetaillevel );
}