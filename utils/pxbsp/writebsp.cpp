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

//===========================================================================
plane_t		g_mapplanes[MAX_INTERNAL_MAP_PLANES];
int		g_mappedtexinfo[MAX_INTERNAL_MAP_TEXINFO];
dtexinfo_t	g_maptexinfo[MAX_MAP_TEXINFO];
int		g_nummaptexinfo;
int		g_nummapplanes;

/*
=============================================================================

PLANE FINDING

=============================================================================
*/
/*
================
PlaneEqual
================
*/
static bool PlaneEqual( const plane_t *p, const vec3_t normal, vec_t dist )
{
	vec_t	t;

	if( -DIST_EPSILON < ( t = p->dist - dist ) && t < DIST_EPSILON
	 && -DIR_EPSILON < ( t = p->normal[0] - normal[0] ) && t < DIR_EPSILON
	 && -DIR_EPSILON < ( t = p->normal[1] - normal[1] ) && t < DIR_EPSILON
	 && -DIR_EPSILON < ( t = p->normal[2] - normal[2] ) && t < DIR_EPSILON )
		return true;

	return false;
}

/*
================
CreateNewFloatPlane
================
*/
static int CreateNewFloatPlane( const vec3_t srcnormal, vec_t dist )
{
	plane_t	*p0, *p1, temp;
	vec3_t	normal;
	int	type;

	if( VectorLength( srcnormal ) < 0.5 )
		return -1;

	// create a new plane
	if(( g_nummapplanes + 2 ) > MAX_INTERNAL_MAP_PLANES )
		COM_FatalError( "MAX_INTERNAL_MAP_PLANES limit exceeded\n" );

	p0 = &g_mapplanes[g_nummapplanes+0];
	p1 = &g_mapplanes[g_nummapplanes+1];

	// snap plane normal
	VectorCopy( srcnormal, normal );
	type = SnapNormal( normal );

	// only snap distance if the normal is an axis. Otherwise there
	// is nothing "natural" about snapping the distance to an integer.
	if( VectorIsOnAxis( normal ) && fabs( dist - Q_rint( dist )) < DIST_EPSILON )
		dist = Q_rint( dist ); // catch -0.0

	VectorCopy( normal, p0->normal );
	VectorNegate( normal, p1->normal );
	p0->dist = dist;
	p1->dist = -dist;
	p0->type = type;
	p1->type = type;
	g_nummapplanes += 2;

	// always put axial planes facing positive first
	if( normal[type % 3] < 0 )
	{
		// flip order
		temp = *p0;
		*p0 = *p1;
		*p1 = temp;

		return g_nummapplanes - 1;
	}

	return g_nummapplanes - 2;
}

/*
=============
FindFloatPlane

=============
*/
int FindFloatPlane( const vec3_t normal, vec_t dist )
{
	vec_t	srcdist = dist;
	plane_t	*p;

	// NOTE: bsp only alloc a few planes for world portalize
	// so linear search doesn't hit by perfomance
	for( int i = 0; i < g_nummapplanes; i++ )
	{
		p = &g_mapplanes[i];

		if( PlaneEqual( p, normal, dist ))
			return p - g_mapplanes;
	}

	// allocate a new two opposite planes
	return CreateNewFloatPlane( normal, srcdist );
}

/*
==================
EmitPlane

emit a mapped plane
==================
*/
int EmitPlane( int planenum, bool check )
{
	plane_t	*plane = &g_mapplanes[planenum & (~1)];

	if( plane->outplanenum == -1 )
	{
		int	i = g_numplanes;
		dplane_t	*dplane = &g_dplanes[i];

		if( check && g_numplanes == MAX_MAP_PLANES )
			COM_FatalError( "MAX_MAP_PLANES limit exceeded\n" );

		VectorCopy( plane->normal, dplane->normal );
		dplane->dist = plane->dist;
		dplane->type = plane->type;
		plane->outplanenum = g_numplanes++;
	}

	return plane->outplanenum;
}

/*
==================
EmitTexinfo

emit a texinfo
==================
*/
int EmitTexinfo( int texinfo )
{
	if( texinfo == -1 ) COM_FatalError( "EmitTexinfo: texinfo == -1\n" );

	dtexinfo_t	*dtexinfo = &g_texinfo[texinfo];

	if( g_mappedtexinfo[texinfo] == -1 )
	{
		int		i = g_nummaptexinfo;
		dtexinfo_t	*mtexinfo = &g_maptexinfo[i];

		if( g_nummaptexinfo == MAX_MAP_TEXINFO )
			COM_FatalError( "MAX_MAP_TEXINFO limit exceeded\n" );

		memcpy( mtexinfo, dtexinfo, sizeof( dtexinfo_t ));
		g_mappedtexinfo[texinfo] = g_nummaptexinfo++;
	}

	return g_mappedtexinfo[texinfo];
}

/*
==================
EmitClipNodes_r
==================
*/
static int EmitClipNodes_r( node_t *node, const node_t *portalleaf )
{
	int		i, c;
	dclipnode32_t	*cn;
	int		num;

	if( FBitSet( node->flags, FNODE_LEAFPORTAL ))
	{
		if( node->contents == CONTENTS_SOLID )
			return CONTENTS_SOLID;
		else portalleaf = node;
	}

	if( node->planenum == PLANENUM_LEAF )
	{
		if( FBitSet( node->flags, FNODE_DETAILCONTENTS ))
			num = CONTENTS_SOLID;
		else num = portalleaf->contents;

		return num;
	}

	// emit a clipnode
	if (g_numclipnodes == MAX_MAP_CLIPNODES) {
		MsgAnim(D_INFO, "^3=== MAX_MAP_CLIPNODES is exceeded. Map will not run under GoldSrc ===\n");
	}

	if (g_numclipnodes == MAX_MAP_CLIPNODES32) {
		COM_FatalError("MAX_MAP_CLIPNODES32 limit exceeded\n");
	}

	c = g_numclipnodes;
	cn = &g_dclipnodes32[g_numclipnodes++];

	if( node->planenum & 1 )
		COM_FatalError( "WriteClipNodes_r: odd planenum\n" );

	cn->planenum = EmitPlane( node->planenum, false );

	for( i = 0; i < 2; i++ )
		cn->children[i] = EmitClipNodes_r( node->children[i], portalleaf );

	return c;
}

//===========================================================================
/*
==================
EmitLeaf
==================
*/
static int EmitLeaf( node_t *node, const node_t *portalleaf )
{
	face_t	**fp, *f;
	dleaf_t	*leaf_p;
	int	leafnum = g_numleafs;
	vec3_t	mins, maxs;

	if( g_numleafs == MAX_MAP_LEAFS )
		COM_FatalError( "MAX_MAP_LEAFS limit exceeded\n" );

	leaf_p = &g_dleafs[g_numleafs++];	// emit a leaf
	leaf_p->contents = portalleaf->contents;
	leaf_p->visofs = -1;		// no vis info yet

	if( FBitSet( node->flags, FNODE_DETAIL ))
	{
		// intersect its loose bounds with the strict bounds of its parent portalleaf
		VectorCompareMax( portalleaf->mins, node->loosemins, mins );
		VectorCompareMin( portalleaf->maxs, node->loosemaxs, maxs );
	}
	else
	{
		VectorCopy( node->mins, mins );
		VectorCopy( node->maxs, maxs );
	}

	// write bounding box info
	for( int k = 0; k < 3; k++ )
	{
		leaf_p->mins[k] = (short)bound( -32767, (int)mins[k], 32767 );
		leaf_p->maxs[k] = (short)bound( -32767, (int)maxs[k], 32767 );
	}

	// write the marksurfaces
	leaf_p->firstmarksurface = g_nummarksurfaces;

	for( fp = node->markfaces; *fp != NULL; fp++ )
	{
		for( f = *fp; f != NULL; f = f->original )
		{
			if( f->outputnumber == -1 )
				continue;

			// grab tjunction split faces
			if( g_nummarksurfaces == MAX_MAP_MARKSURFACES )
				COM_FatalError( "MAX_MAP_MARKSURFACES limit exceeded\n" );
			g_dmarksurfaces[g_nummarksurfaces++] = f->outputnumber; // emit a marksurface
		}
	}

	leaf_p->nummarksurfaces = g_nummarksurfaces - leaf_p->firstmarksurface;

	return leafnum;
}

/*
==================
CheckNullFace
==================
*/
static bool CheckNullFace( face_t *f )
{
	// bad winding
	if( !f->w || f->w->numpoints < 3 )
	{
		MsgDev( D_ERROR, "degenerated face\n" );
		return true;
	}

	// no texinfo or face is unreferenced
	if( f->texturenum == -1 || f->referenced == 0 )
		return true; 

	// discardable contents
	if( f->facestyle == face_hint || f->facestyle == face_discardable )
		return true;

	return false;
}

/*
==================
WriteFace
==================
*/
static void WriteFace( face_t *f )
{
	dface_t	*df;
	winding_t	*w = f->w;
	int	i, e;

	if( CheckNullFace( f ))
	{
		f->outputnumber = -1;
		return; // discardable face
	}

	f->outputnumber = g_numfaces;

	df = &g_dfaces[g_numfaces];

	if( g_numfaces >= MAX_MAP_FACES )
		COM_FatalError( "MAX_MAP_FACES limit exceeded\n" );
	g_numfaces++;

	df->planenum = EmitPlane( f->planenum, true );
	df->side = f->planenum & 1;	// odd-even
	df->firstedge = g_numsurfedges;
	df->numedges = f->numedges;
	df->texinfo = EmitTexinfo( f->texturenum );

	for( i = 0; i < f->numedges; i++ )
	{
		e = f->outputedges[i];
		if( g_numsurfedges >= MAX_MAP_SURFEDGES )
			COM_FatalError( "MAX_MAP_SURFEDGES limit exceeded\n" );
		g_dsurfedges[g_numsurfedges] = e;
		g_numsurfedges++;
	}

	Mem_Free( f->outputedges );
	f->outputedges = NULL;
}

/*
==================
EmitDrawNodes_r
==================
*/
static int EmitDrawNodes_r( node_t *node, const node_t *portalleaf )
{
	dnode_t	*n;
	face_t	*f;
	int	nodenum = g_numnodes;
	vec3_t	mins, maxs;

	if( FBitSet( node->flags, FNODE_LEAFPORTAL ))
	{
		if( node->contents == CONTENTS_SOLID )
			return -1;
		else portalleaf = node;
	}

	if( node->planenum == PLANENUM_LEAF )
	{
		if( FBitSet( node->flags, FNODE_DETAILCONTENTS ))
		{
			Mem_Free( node->markfaces );
			node->markfaces = NULL;
			return -1;
		}
		else
		{
			int leafnum = EmitLeaf( node, portalleaf );
			return -1 - leafnum;
		}
	}

	if( g_numnodes == MAX_MAP_NODES )
		COM_FatalError( "MAX_MAP_NODES limit exceeded\n" );

	n = &g_dnodes[g_numnodes++];		// emit a node

	if( FBitSet( node->flags, FNODE_DETAIL ))
	{
		// intersect its loose bounds with the strict bounds of its parent portalleaf
		VectorCompareMax( portalleaf->mins, node->loosemins, mins );
		VectorCompareMin( portalleaf->maxs, node->loosemaxs, maxs );
	}
	else
	{
		VectorCopy( node->mins, mins );
		VectorCopy( node->maxs, maxs );
	}

	// write bounding box info
	for( int k = 0; k < 3; k++ )
	{
		n->mins[k] = (short)bound( -32767, (int)mins[k], 32767 );
		n->maxs[k] = (short)bound( -32767, (int)maxs[k], 32767 );
	}

	if( node->planenum & 1 )
		COM_FatalError( "EmitDrawNodes_r: odd planenum\n" );

	n->planenum = EmitPlane( node->planenum, true );
	n->firstface = g_numfaces;

	for( f = node->faces; f != NULL; f = f->next )
		WriteFace( f );

	n->numfaces = g_numfaces - n->firstface;

	// recursively output the other nodes
	for( int i = 0; i < 2; i++ )
		n->children[i] = EmitDrawNodes_r( node->children[i], portalleaf );

	return nodenum;
}

/*
===========
FreeDrawNodes_r
===========
*/
static void FreeDrawNodes_r( node_t *node )
{
	face_t	*f, *next;

	for( int i = 0; i < 2; i++ )
	{
		// NOTE: optimized tree converts some nodes to leafs
		// but leave childrens are valid, so we can throw mem correctly
		if( node->children[i] != NULL )
			FreeDrawNodes_r( node->children[i] );
	}

	if( node->surfaces )
	{
		FreeLeafSurfs( node );
		node->surfaces = NULL;
	}

	if( !node->children[0] && !node->children[1] )
	{
		// real leaf
		Mem_Free( node->markfaces );
		node->markfaces = NULL;
	}
	else
	{
		// free the faces on the node
		for( f = node->faces; f != NULL; f = next )
		{
			next = f->next;
			FreeFace( f );
		}
	}

	FreeNode( node );
}

/*
===========
CalcModelBoundBox
===========
*/
static void CalcModelBoundBox( dmodel_t *bmod, tree_t *tree, int hullnum, int modnum )
{
	vec3_t	mins, maxs;
	int	i;

	if( !tree->surfaces || ( tree->mins[0] > tree->maxs[0] ))
	{
		MsgDev( D_REPORT, "model %d hull %d empty\n", modnum, hullnum );
	}
	else
	{
		if( hullnum && ( bmod->mins[0] <= bmod->maxs[0] ))
			return; // bbox is computed

		VectorSubtract( tree->mins, g_hull_size[0][0], mins );
		VectorSubtract( tree->maxs, g_hull_size[0][1], maxs );

		for( i = 0; i < 3; i++ )
		{
			if( mins[i] > maxs[i] )
			{
				vec_t tmp = (mins[i] + maxs[i]) * 0.5;
				mins[i] = maxs[i] = tmp;
			}
		}

		for( i = 0; i < 3; i++ )
		{
			bmod->maxs[i] = Q_max( bmod->maxs[i], maxs[i] );
			bmod->mins[i] = Q_min( bmod->mins[i], mins[i] );
		}
	}
}

/*
===========
FaceOutputEdges
===========
*/
static void FaceOutputEdges( face_t *f )
{
	if( CheckNullFace( f ))
		return;

	f->outputedges = (int *)Mem_Alloc( f->w->numpoints * sizeof( int ));
	f->numedges = f->w->numpoints;

	for( int i = 0; i < f->w->numpoints; i++ )
	{
		f->outputedges[i] = GetEdge( f->w->p[i], f->w->p[(i + 1) % f->w->numpoints], f );
	}
}

/*
===========
OutputEdges_r
===========
*/
static int OutputEdges_r( node_t *node, int detaillevel )
{
	int	next = -1;

	if( node->planenum == PLANENUM_LEAF )
		return next;

	for( face_t *f = node->faces; f != NULL; f = f->next )
	{
		if( f->detaillevel > detaillevel )
		{
			if( next == -1 ? true : f->detaillevel < next )
			{
				next = f->detaillevel;
			}
		}

		if( f->detaillevel == detaillevel )
			FaceOutputEdges( f );
	}

	for( int i = 0; i < 2; i++ )
	{
		int r = OutputEdges_r( node->children[i], detaillevel );

		if( r == -1 ? false : next == -1 ? true : r < next )
			next = r;
	}

	return next;
}

/*
===========
RemoveCoveredFaces_r
===========
*/
static void RemoveCoveredFaces_r( node_t *node )
{
	if( FBitSet( node->flags, FNODE_LEAFPORTAL ))
	{
		if( node->contents == CONTENTS_SOLID )
		{
			return; // stop here, don't go deeper into children
		}
	}

	if( node->planenum == PLANENUM_LEAF )
	{
		// this is a leaf
		if( FBitSet( node->flags, FNODE_DETAILCONTENTS ))
		{
			return;
		}
		else
		{
			for( face_t **fp = node->markfaces; *fp != NULL; fp++ )
			{
				for( face_t *f = *fp; f != NULL; f = f->original ) // for each tjunc subface
				{
					f->referenced++; // mark the face as referenced
				}
			}
		}
		return;
	}
	
	// this is a node
	for( face_t *f = node->faces; f != NULL; f = f->next )
		f->referenced = 0; // clear the mark

	RemoveCoveredFaces_r( node->children[0] );
	RemoveCoveredFaces_r( node->children[1] );
}

/*
==================
EmitDrawNodes
==================
*/
void EmitDrawNodes( tree_t *tree )
{
	int	firstleaf;
	int	l, next;
	vec3_t	origin;
	dmodel_t	*bm;

	MsgDev( D_REPORT, "--- EmitDrawNodes ---\n" );

	if( g_nummodels == MAX_MAP_MODELS )
		COM_FatalError( "MAX_MAP_MODELS limit exceeded\n" );

	bm = &g_dmodels[g_nummodels++]; // emit a model
	VectorFill( bm->mins,  999999 );
	VectorFill( bm->maxs, -999999 );
	bm->headnode[0] = g_numnodes;
	bm->firstface = g_numfaces;
	firstleaf = g_numleafs;

	// fill "referenced" value
	RemoveCoveredFaces_r( tree->headnode );

	for( l = 0; l != -1; l = next )
		next = OutputEdges_r( tree->headnode, l );

	EmitDrawNodes_r( tree->headnode, NULL );

	bm->numfaces = g_numfaces - bm->firstface;
	bm->visleafs = g_numleafs - firstleaf;

	CalcModelBoundBox( bm, tree, 0, g_nummodels - 1 );

	// g-cont. copy origin from entity settings into dmodel_t struct
	entity_t *ent = EntityForModel( g_nummodels - 1 );
	GetVectorForKey( ent, "origin", origin );
	VectorCopy( origin, bm->origin );

	FreeDrawNodes_r( tree->headnode );
	tree->headnode = NULL;
}

/*
==================
EmitClipNodes

Called after the clipping hull is completed. Generates a disk format
representation and frees the original memory.
==================
*/
void EmitClipNodes( tree_t *tree, int modnum, int hullnum )
{
	dmodel_t	*bm;

	MsgDev( D_REPORT, "--- EmitClipNodes ---\n" );

	bm = &g_dmodels[modnum]; // emit a model

	// if noclip is enabled
	if( tree->headnode->planenum == PLANENUM_LEAF )
		bm->headnode[hullnum] = CONTENTS_EMPTY;
	else bm->headnode[hullnum] = g_numclipnodes;

	EmitClipNodes_r( tree->headnode, NULL );
	CalcModelBoundBox( bm, tree, hullnum, modnum );

	FreeDrawNodes_r( tree->headnode );
	tree->headnode = NULL;
}

/*
==================
EmitVertex
==================
*/
int EmitVertex( const vec3_t point )
{
	int i;
	dvertex_t	*vert;

	if( g_numvertexes == MAX_MAP_VERTS )
		COM_FatalError( "MAX_MAP_VERTS limit exceeded\n" );

	for( i = 0; i < g_numvertexes; i++ )
	{
		if( VectorCompareEpsilon2( point, g_dvertexes[i].point, EQUAL_EPSILON ))
			return i;
	}

	vert = &g_dvertexes[g_numvertexes++];	// emit a new vertex
	VectorCopy( point, vert->point );

	return i;
}

//===========================================================================

/*
==================
BeginBSPFile
==================
*/
void BeginBSPFile (void)
{
	// these values may actually be initialized
	// if the file existed when loaded, so clear them explicitly
	g_nummodels = 0;
	g_numfaces = 0;
	g_numnodes = 0;
	g_numclipnodes = 0;
	g_numvertexes = 0;
	g_nummarksurfaces = 0;
	g_numsurfedges = 0;
	g_numplanes = 0;

	// edge 0 is not used, because 0 can't be negated
	g_numedges = 1;

	// leaf 0 is common solid with no faces
	g_numleafs = 1;
	g_dleafs[0].contents = CONTENTS_SOLID;

	memset( g_mappedtexinfo, -1, sizeof( g_mappedtexinfo ));

	InitHash ();
}


/*
==================
FinishBSPFile
==================
*/
void FinishBSPFile( const char *source )
{
	MsgDev( D_REPORT, "--- FinishBSPFile ---\n" );

	memcpy( g_texinfo, g_maptexinfo, sizeof( dtexinfo_t ) * g_nummaptexinfo );
	MsgDev( D_REPORT, "total texinfo %d, output texinfo %d\n", g_numtexinfo, g_nummaptexinfo );
	g_numtexinfo = g_nummaptexinfo;

	MsgDev( D_REPORT, "total planes %d, output planes %d\n", g_nummapplanes, g_numplanes );

	if( GetDeveloperLevel() >= D_REPORT )
		PrintBSPFileSizes ();

	WriteBSPFile( source );
}