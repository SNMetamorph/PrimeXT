/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// tjunc.c

#include "bsp5.h"

#define MAX_VERTS_ON_SUPERFACE	8192
#define MAX_HASH_NEIGHBORS		4

typedef struct wvert_s
{
	vec_t		t;
	struct wvert_s	*prev;
	struct wvert_s	*next;
} wvert_t;

typedef struct wedge_s
{
	struct wedge_s	*next;
	vec3_t		dir;
	vec3_t		origin;
	wvert_t		head;
} wedge_t;

typedef struct
{
	int		numpoints;
	vec3_t		points[MAX_VERTS_ON_SUPERFACE];
	face_t		original;
} superface_t;

int	maxwedges, maxwverts;
int	numwedges, numwverts;
int	c_degenerateEdges;
int	c_degenerateFaces;
int	c_tjuncs;
int	c_tjuncfaces;
int	c_rotated;

wvert_t	*wverts;
wedge_t	*wedges;


void PrintFace( face_t *f )
{
	if( f->w ) pw( f->w );
}

//============================================================================

#define NUM_HASH		1024

wedge_t		*wedge_hash[NUM_HASH];

static vec3_t	h_min, h_scale;
static int	h_numslots[3];

static void InitHash( void )
{
	vec3_t	size;
	vec_t	volume;
	vec_t	scale;

	memset( wedge_hash, 0, sizeof( wedge_hash ));
	VectorFill( h_min, -8000 );
	VectorFill( size, 16000 );
	
	volume = size[0] * size[1];
	
	scale = sqrt( volume / NUM_HASH );

	h_numslots[0] = (int)floor( size[0] / scale );
	h_numslots[1] = (int)floor( size[1] / scale );

	while( h_numslots[0] * h_numslots[1] > NUM_HASH )
	{
		h_numslots[0]--;
		h_numslots[1]--;
	}

	h_scale[0] = h_numslots[0] / size[0];
	h_scale[1] = h_numslots[1] / size[1];
}

static int HashVec( const vec3_t vec, int *num, int *hash )
{
	int	h, i, x, y;
	vec_t	normalized[2];
	vec_t	sdiff[2];
	int	slot[2];

	for( i = 0; i < 2; i++ )
	{
		normalized[i] = h_scale[i] * (vec[i] - h_min[i]);
		slot[i] = (int)floor( normalized[i] );
		sdiff[i] = normalized[i] - (vec_t)slot[i];

		slot[i] = (slot[i] + h_numslots[i]) % h_numslots[i];
		slot[i] = (slot[i] + h_numslots[i]) % h_numslots[i]; // do it twice to handle negative values
	}

	h = slot[0] * h_numslots[1] + slot[1];
	*num = 0;

	for( x = -1; x <= 1; x++ )
	{
		if( x == -1 && sdiff[0] > h_scale[0] * ( 2 * ON_EPSILON ) || x == 1 && sdiff[0] < 1 - h_scale[0] * ( 2 * ON_EPSILON ))
			continue;

		for( y = -1; y <= 1; y++ )
		{
			if( y == -1 && sdiff[1] > h_scale[1] * ( 2 * ON_EPSILON ) || y == 1 && sdiff[1] < 1 - h_scale[1] * ( 2 * ON_EPSILON ))
				continue;

			if( *num >= MAX_HASH_NEIGHBORS )
				COM_FatalError( "HashVec: internal error\n" );

			hash[*num] = ((slot[0] + x + h_numslots[0]) % h_numslots[0]) * h_numslots[1] +
				    (slot[1] + y + h_numslots[1]) % h_numslots[1];
			(*num)++;
		}
	}

	return h;
}

//============================================================================
bool CanonicalVector( vec3_t vec )
{
	if( VectorNormalize( vec ))
	{
		if( vec[0] > NORMAL_EPSILON )
		{
			return true;
		}
		else if( vec[0] < -NORMAL_EPSILON )
		{
			VectorNegate( vec, vec );
			return true;
		}
		else
		{
			vec[0] = 0;
		}    

		if( vec[1] > NORMAL_EPSILON )
		{
			return true;
		}
		else if( vec[1] < -NORMAL_EPSILON )
		{
			VectorNegate( vec, vec );
			return true;
		}
		else
		{
			vec[1] = 0;
		}

		if( vec[2] > NORMAL_EPSILON )
		{
			return true;
		}
		else if( vec[2] < -NORMAL_EPSILON )
		{
			VectorNegate( vec, vec );
			return true;
		}
		else
		{
			vec[2] = 0;
		}
		return false;
	}

	return false;
}

wedge_t *FindEdge( vec3_t p1, vec3_t p2, vec_t *t1, vec_t *t2 )
{
	vec3_t	origin, dir;
	int	h, num_hashneighbors;
	int	hashneighbors[MAX_HASH_NEIGHBORS];
	vec_t	temp;
	wedge_t	*w;

	VectorSubtract( p2, p1, dir );

	// ignore degenerate edges
	if( !CanonicalVector( dir ))
	{
		c_degenerateEdges++;
		return NULL;
	}

	*t1 = DotProduct( p1, dir );
	*t2 = DotProduct( p2, dir );

	VectorMA( p1, -*t1, dir, origin );

	if( *t1 > *t2 )
	{
		temp = *t1;
		*t1 = *t2;
		*t2 = temp;
	}
	
	h = HashVec( origin, &num_hashneighbors, hashneighbors );

	for( int i = 0; i < num_hashneighbors; i++ )
	{
		for( w = wedge_hash[hashneighbors[i]]; w; w = w->next )
		{
			if( !VectorCompareEpsilon( w->origin, origin, EQUAL_EPSILON ))
				continue;

			if( !VectorCompareEpsilon( w->dir, dir, NORMAL_EPSILON ))
				continue;

			return w;
		}
	}	

	if( numwedges == maxwedges )
		COM_FatalError( "FindEdge: numwedges == MAXWEDGES\n" );
	w = &wedges[numwedges];
	numwedges++;
	
	w->next = wedge_hash[h];
	wedge_hash[h] = w;
	
	VectorCopy( origin, w->origin );
	w->head.next = w->head.prev = &w->head;
	VectorCopy( dir, w->dir );
	w->head.t = 99999;
	return w;
}

/*
===============
AddVert

===============
*/
void AddVert( wedge_t *w, vec_t t )
{
	wvert_t	*v, *newv;
	
	v = w->head.next;
	do
	{
		if( fabs( v->t - t ) < T_EPSILON )
			return;
		if( v->t > t )
			break;
		v = v->next;
	} while( 1 );
		
	// insert a new wvert before v
	if( numwverts == maxwverts )
		COM_FatalError( "AddVert: numwverts == MAXWVERTS\n" );
	newv = &wverts[numwverts];
	numwverts++;
	
	newv->t = t;
	newv->next = v;
	newv->prev = v->prev;
	v->prev->next = newv;
	v->prev = newv;
}

/*
===============
AddEdge

===============
*/
void AddEdge( vec3_t p1, vec3_t p2 )
{
	wedge_t	*w;
	vec_t	t1, t2;
	
	w = FindEdge( p1, p2, &t1, &t2 );
	if( !w ) return;

	AddVert( w, t1 );
	AddVert( w, t2 );
}

/*
===============
AddFaceEdges

===============
*/
void AddFaceEdges( face_t *f )
{
	winding_t	*w = f->w;
	int	i, j;
	
	for( i = 0; i < w->numpoints; i++ )
	{
		 j = (i+1) % w->numpoints;
		 AddEdge( w->p[i], w->p[j] );
	}
}

//============================================================================

superface_t	*superface;
face_t		*newlist;

void FaceFromSuperface( face_t *original )
{
	int	firstcorner, lastcorner;
	int	i, numpts;
	face_t	*newf, *chain;
	vec3_t	dir, test;
	vec_t	v;
	
	chain = NULL;
	while( 1 )
	{
		if( superface->numpoints < 3 )
		{
			c_degenerateFaces++;
			return;
		}

		if( superface->numpoints <= MAX_VERTS_ON_FACE )
		{	
			// the face is now small enough without more cutting
			// so copy it back to the original
			*original = superface->original;
			original->w = CopyWinding( superface->numpoints, superface->points );
			original->original = chain;
			original->next = newlist;
			newlist = original;
			return;
		}

		c_tjuncfaces++;
restart:	
		// find the last corner
		VectorSubtract( superface->points[superface->numpoints-1], superface->points[0], dir );
		VectorNormalize( dir );
		for( lastcorner = superface->numpoints - 1; lastcorner > 0; lastcorner-- )
		{
			VectorSubtract (superface->points[lastcorner-1], superface->points[lastcorner], test );
			VectorNormalize (test);
			v = DotProduct (test, dir);

			if( v < 1.0 - ON_EPSILON || v > 1.0 + ON_EPSILON )
			{
				break;
			}
		}
	
		// find the first corner
		VectorSubtract( superface->points[1], superface->points[0], dir );
		VectorNormalize( dir );
		for( firstcorner = 1; firstcorner < superface->numpoints - 1; firstcorner++ )
		{
			VectorSubtract( superface->points[firstcorner+1], superface->points[firstcorner], test );
			VectorNormalize( test );
			v = DotProduct( test, dir );

			if( v < 1.0 - ON_EPSILON || v > 1.0 + ON_EPSILON )
				break;
		}
	
		if( firstcorner + 2 >= MAX_VERTS_ON_FACE )
		{
			c_rotated++;
			// rotate the point winding
			VectorCopy( superface->points[0], test );
			for( i = 1; i < superface->numpoints; i++ )
				VectorCopy( superface->points[i], superface->points[i-1] );
			VectorCopy( test, superface->points[superface->numpoints-1] );
			goto restart;
		}
		
		
		// cut off as big a piece as possible, less than MAXPOINTS,
		// and not past lastcorner
		newf = NewFaceFromFace (&superface->original);
		newf->original = chain;
		chain = newf;
		newf->next = newlist;
		newlist = newf;

		if( superface->numpoints - firstcorner <= MAX_VERTS_ON_FACE )
			numpts = firstcorner + 2;
		else if( lastcorner+2 < MAX_VERTS_ON_FACE && superface->numpoints - lastcorner <= MAX_VERTS_ON_FACE )
			numpts = lastcorner + 2;
		else numpts = MAX_VERTS_ON_FACE;

		if( numpts < 3 )
		{
			c_degenerateFaces++;
			return;
		}

		newf->w = CopyWinding( numpts, superface->points );

		for( i = newf->w->numpoints - 1; i < superface->numpoints; i++ )
			VectorCopy( superface->points[i], superface->points[i - (newf->w->numpoints - 2)]);
		superface->numpoints -= (newf->w->numpoints - 2);
	}
}

/*
===============
FixFaceEdges

===============
*/
void FixFaceEdges( face_t *f )
{
	int	i, j, k;
	vec_t	t1, t2;
	wedge_t	*w;
	wvert_t	*v;

	if( f->w->numpoints > MAX_VERTS_ON_SUPERFACE )
		COM_FatalError( "FixFaceEdges: f->w->numpoints > MAX_VERTS_ON_SUPERFACE\n" );

	superface->original = *f;
	superface->numpoints = f->w->numpoints;
	memcpy( superface->points, f->w->p, sizeof( vec3_t ) * f->w->numpoints );
	FreeWinding( f->w );
	f->w = NULL;
restart:
	for( i = 0; i < superface->numpoints; i++ )
	{
		 j = (i+1) % superface->numpoints;

		w = FindEdge( superface->points[i], superface->points[j], &t1, &t2 );
		if( !w ) continue;
		
		for( v = w->head.next; v->t < t1 + T_EPSILON; v = v->next );

		if( v->t < t2 - T_EPSILON )
		{
			c_tjuncs++;

			// insert a new vertex here
			for( k = superface->numpoints; k > j; k-- )
				VectorCopy( superface->points[k-1], superface->points[k] );
			VectorMA( w->origin, v->t, w->dir, superface->points[j] );
			superface->numpoints++;

			if( superface->numpoints > MAX_VERTS_ON_SUPERFACE )
				COM_FatalError( "FixFaceEdges: superface->numpoints > MAX_VERTS_ON_SUPERFACE\n" );
			goto restart;	
		}
	}

	// the face might be split into multiple faces because of too many edges
	FaceFromSuperface( f );
}

//============================================================================

static void tjunc_count_r( node_t *node )
{
	face_t	*f;

	if( node->planenum == PLANENUM_LEAF )
		return;

	for( f = node->faces; f != NULL; f = f->next )
		maxwedges += f->w->numpoints;

	tjunc_count_r( node->children[0] );
	tjunc_count_r( node->children[1] );
}

void tjunc_find_r( node_t *node )
{
	face_t	*f;

	if( node->planenum == PLANENUM_LEAF )
		return;
		
	for( f = node->faces; f != NULL; f = f->next )
		AddFaceEdges( f );
		
	tjunc_find_r( node->children[0] );
	tjunc_find_r( node->children[1] );
}

void tjunc_fix_r( node_t *node )
{
	face_t	*f, *next;

	if( node->planenum == PLANENUM_LEAF )
		return;
		
	newlist = NULL;
	
	for( f = node->faces; f != NULL; f = next )
	{
		next = f->next;
		FixFaceEdges( f );
	}

	node->faces = newlist;

	tjunc_fix_r( node->children[0] );
	tjunc_fix_r( node->children[1] );
}

/*
===========
tjunc

===========
*/
void tjunc( node_t *headnode, bool worldmodel )
{
	double	start, end;
	char	str[64];

	if( g_notjunc ) return;
	
	MsgDev( D_REPORT, "---- tjunc ----\n");
	start = I_FloatTime ();
	InitHash();
	
	maxwedges = maxwverts = 0;
	tjunc_count_r( headnode );
	maxwverts = maxwedges * 2;

	// alloc space for work
	wverts = (wvert_t *)Mem_Alloc( sizeof( *wverts ) * maxwverts );
	wedges = (wedge_t *)Mem_Alloc( sizeof( *wedges ) * maxwedges );
	superface = (superface_t *)Mem_Alloc( sizeof( superface_t ));
	numwedges = numwverts = 0;

	//
	// identify all points on common edges
	//
	tjunc_find_r( headnode );
		
	MsgDev( D_REPORT, "%i world edges  %i edge points\n", numwedges, numwverts );

	//
	// add extra vertexes on edges where needed
	//
	c_tjuncs = c_tjuncfaces = c_degenerateEdges = c_degenerateFaces = c_rotated = 0;

	tjunc_fix_r( headnode );

	Mem_Free( wverts );
	Mem_Free( wedges );
	Mem_Free( superface );
	superface = NULL;

	MsgDev( D_REPORT, "%i degenerate edges\n", c_degenerateEdges );
	MsgDev( D_REPORT, "%i degenerate faces\n", c_degenerateFaces );
	MsgDev( D_REPORT, "%i edges added by tjunctions\n", c_tjuncs );
	MsgDev( D_REPORT, "%i faces added by tjunctions\n", c_tjuncfaces );
	MsgDev( D_REPORT, "%i naturally ordered\n", c_tjuncfaces - c_rotated );
	MsgDev( D_REPORT, "%i rotated orders\n", c_rotated );

	end = I_FloatTime ();
	Q_timestring((int)( end - start ), str );
	if( worldmodel ) MsgDev( D_INFO, "t-junction: %s elapsed\n", str );
}