/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// divide.h

#include "bsp5.h"

/*
a surface has all of the faces that could be drawn on a given plane

the outside filling stage can remove some of them so a better bsp can be generated

*/

int	c_totalverts;
int	c_uniqueverts;
int	c_unsplitted_faces;

/*
===============
SubdivideFace

If the face is >256 in either texture direction, carve a valid sized
piece off and insert the remainder in the next link
===============
*/
void SubdivideFace( face_t *f, face_t **prevptr )
{
	face_t		*front, *back, *next;
	int		max_surface_extent;
	int		subdivide_size;
	int		texture_step;
	float		lmvecs[2][4];
	vec_t		mins, maxs;
	vec_t		v, extent;
	plane_t		plane;
	dtexinfo_t	*tex;
	vec3_t		temp;

	if( f->facestyle == face_hint || f->facestyle == face_discardable )
		return; // ideally these should have their tex_special flag set, so its here jic

	if( f->texturenum == -1 )
		return;

	// special (non-surface cached) faces don't need subdivision
	tex = &g_texinfo[f->texturenum];

	if( FBitSet( tex->flags, TEX_SPECIAL ))
		return;

	// compute subdivision size
	max_surface_extent = GetSurfaceExtent( tex );
	texture_step = GetTextureStep( tex );
	subdivide_size = ((max_surface_extent - 1) * texture_step);

	LightMatrixFromTexMatrix( tex, lmvecs );

	for( int axis = 0; axis < 2; axis++ )
	{
		while( 1 )
		{
			mins =  999999;
			maxs = -999999;

			for( int i = 0; i < f->w->numpoints; i++ )
			{
				v = DotProduct( f->w->p[i], lmvecs[axis] );
				mins = Q_min( v, mins );
				maxs = Q_max( v, maxs );
			}

			extent = ceil( maxs ) - floor( mins );
//			extent = maxs - mins;

			if( extent <= subdivide_size )
				break;

			// split it
			VectorCopy( lmvecs[axis], temp );
			v = VectorNormalize( temp );	
			VectorCopy( temp, plane.normal );
			plane.dist = (mins + subdivide_size - texture_step) / v;

			next = f->next;
			SplitFace( f, &plane, &front, &back );
#if 0
			if( !front || !back )
			{
				c_unsplitted_faces++;
				printf( "can't split\n" );
				break;
			}

			*prevptr = back;
			back->next = front;
			front->next = next;
			f = back;
#else
			f = next;

			if( front )
			{
				front->next = f;
				f = front;
			}

			if( back )
			{
				back->next = f;
				f = back;
			}

			*prevptr = f;

			if( !front || !back )
			{
				c_unsplitted_faces++;
				break;
			}
#endif
		}
	}
}

//===========================================================================
#define MAX_HASH		4096
#define MAX_HASH_NEIGHBORS	4

typedef struct hashvert_s
{
	struct hashvert_s	*next;
	vec3_t		point;
	int		planenums[2];
	int		numplanes;	// for corner determination
	int		num;
} hashvert_t;

static face_t	*g_edgefaces[MAX_MAP_EDGES][2];
static hashvert_t	hvertex[MAX_MAP_VERTS];
static hashvert_t	*hashverts[MAX_HASH];
static int	firstmodeledge = 1;
static int	h_numslots[3];
static hashvert_t	*hvert_p;
static vec3_t	h_scale;
static vec3_t	h_min;

//============================================================================
/*
===============
InitHash
===============
*/
void InitHash( void )
{
	vec_t           volume;
	vec_t           scale;
	vec3_t          size;

	memset( hashverts, 0, sizeof( hashverts ));
	VectorFill( h_min, -8000 );
	VectorFill( size, 16000 );

	volume = size[0] * size[1];
	scale = sqrt( volume / MAX_HASH );

	h_numslots[0] = (int)floor( size[0] / scale );
	h_numslots[1] = (int)floor( size[1] / scale );

	while( h_numslots[0] * h_numslots[1] > MAX_HASH )
	{
		h_numslots[0]--;
		h_numslots[1]--;
	}

	h_scale[0] = h_numslots[0] / size[0];
	h_scale[1] = h_numslots[1] / size[1];
	hvert_p = hvertex;
}

/*
===============
HashVec

returned value: the one bucket that a new vertex may "write" into
returned hashneighbors: the buckets that we should "read" to check for an existing vertex
===============
*/
static int HashVec( const vec3_t vec, int *num, int *hash )
{
	vec_t	normalized[2];
	int	h, i, x, y;
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
		if( x == -1 && sdiff[0] > h_scale[0] * ON_EPSILON || x == 1 && sdiff[0] < 1 - h_scale[0] * ON_EPSILON )
			continue;

		for( y = -1; y <= 1; y++ )
		{
			if( y == -1 && sdiff[1] > h_scale[1] * ON_EPSILON || y == 1 && sdiff[1] < 1 - h_scale[1] * ON_EPSILON )
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

/*
===============
GetVertex
===============
*/
static int GetVertex( const vec3_t in, const int planenum )
{
	int		hashneighbors[MAX_HASH_NEIGHBORS];
	int		num_hashneighbors;
	int		i, h;
	vec3_t		vert;
	hashvert_t	*hv;

	for( i = 0; i < 3; i++ )
	{
		if( fabs( in[i] - Q_round( in[i] )) < 0.001 )
			vert[i] = Q_round( in[i] );
		else vert[i] = in[i];
	}

	h = HashVec( vert, &num_hashneighbors, hashneighbors );

	for( i = 0; i < num_hashneighbors; i++ )
	{
		for( hv = hashverts[hashneighbors[i]]; hv != NULL; hv = hv->next )
		{
			if( !VectorCompareEpsilon( hv->point, vert, ON_EPSILON ))
				continue;

			// already known to be a corner
			if( hv->numplanes == 3 )
				return hv->num;

			for( i = 0; i < hv->numplanes; i++ )
			{
				// already know this plane
				if( hv->planenums[i] == planenum )
					return hv->num; 
			}

			if( hv->numplanes != 2 )
				hv->planenums[hv->numplanes] = planenum;
			hv->numplanes++;

			return hv->num;
		}
	}

	hv = hvert_p;
	hv->numplanes = 1;
	hv->planenums[0] = planenum;
	hv->next = hashverts[h];
	hashverts[h] = hv;
	VectorCopy( vert, hv->point );
	hv->num = g_numvertexes;
	if( hv->num == MAX_MAP_VERTS )
		COM_FatalError( "MAX_MAP_VERTS limit exceeded\n" );
	hvert_p++;

	// emit a vertex
	if( g_numvertexes == MAX_MAP_VERTS )
		COM_FatalError( "MAX_MAP_VERTS limit exceeded\n" );
	VectorCopy( vert, g_dvertexes[g_numvertexes].point );
	g_numvertexes++;

	return hv->num;
}

//===========================================================================

/*
==================
GetEdge

Don't allow four way edges
==================
*/
int GetEdge( vec3_t p1, vec3_t p2, face_t *f )
{
	int	v1, v2;
	dedge_t	*edge;
	int	i;

	if( !f->contents )
		COM_FatalError( "GetEdge: CONTENTS_NONE\n" );

	v1 = GetVertex( p1, f->planenum );
	v2 = GetVertex( p2, f->planenum );

	for( i = firstmodeledge; i < g_numedges; i++ )
	{
		edge = &g_dedges[i];

		if( v1 == edge->v[1] && v2 == edge->v[0] && !g_edgefaces[i][1] && g_edgefaces[i][0]->contents == f->contents
		 && g_edgefaces[i][0]->planenum != ( f->planenum ^ 1 ))
		{
			g_edgefaces[i][1] = f;
			return -i;
		}
	}
	
	// emit an edge
	if( g_numedges >= MAX_MAP_EDGES )
		COM_FatalError( "MAX_MAP_EDGES limit exceeded\n" );
	edge = &g_dedges[g_numedges];
	g_numedges++;

	g_edgefaces[i][0] = f;
	edge->v[0] = v1;
	edge->v[1] = v2;

	return i;
}

/*
================
MakeFaceEdges
================
*/
void MakeFaceEdges( void )
{
	firstmodeledge = g_numedges;	// !!!
//	InitHash();
}