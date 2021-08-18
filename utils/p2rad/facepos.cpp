/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "qrad.h"

#define MAX_NUDGES	12

static const vec3_t g_nudgelist[MAX_NUDGES] =
{
{ 0.1, 0.0, 0.0 }, {-0.1, 0.0, 0.0 }, { 0.0, 0.1, 0.0 },
{ 0.0,-0.1, 0.0 }, { 0.3, 0.0, 0.0 }, {-0.3, 0.0, 0.0 },
{ 0.0, 0.3, 0.0 }, { 0.0,-0.3, 0.0 }, { 0.3, 0.3, 0.0 },
{-0.3, 0.3, 0.0 }, {-0.3,-0.3, 0.0 }, { 0.3,-0.3, 0.0 }
};
						
typedef struct
{
	bool	valid;
	vec_t	best_s;	// FindNearestPosition will return this value
	vec_t	best_t;
	vec3_t	pos;	// with DEFAULT_HUNT_OFFSET
} position_t;

// Size of position_t (21) * positions per sample (9) * max number of samples (max AllocBlock (64) * 128 * 128)
//   = 200MB of RAM
// But they are freed before BuildVisLeafs, so it's not a problem.

typedef struct
{
	bool		valid;
	int		facenum;
	vec3_t		face_offset;
	matrix3x4 	worldtotex;
	matrix3x4 	textoworld;
	winding_t		*facewinding;
	dplane_t		faceplane;
	winding_t		*facewindingwithoffset;
	dplane_t		faceplanewithoffset;
	winding_t		*texwinding;
	dplane_t		texplane; // (0, 0, 1, 0) or (0, 0, -1, 0)
	vec3_t		texcentroid;
	vec3_t		start; // s_start, t_start, 0
	vec3_t		step; // s_step, t_step, 0
	int		w; // number of s
	int		h; // number of t
	position_t	*grid; // [h][w]
} positionmap_t;

static positionmap_t	g_face_positions[MAX_MAP_FACES];

static bool IsPositionValid( int threadnum, positionmap_t *map, const vec3_t pos_st, vec3_t pos_out, bool phong = true, bool edgetest = true )
{
	vec3_t	pos, pos_normal;
	vec_t	hunt_scale = 0.2;
	int	hunt_size = 2;
	vec_t	hunt_offset;

	Matrix3x4_VectorTransform( map->textoworld, pos_st, pos );
	VectorAdd( pos, map->face_offset, pos );

	if( phong )
	{
		GetPhongNormal( map->facenum, pos, pos_normal );
	}
	else
	{
		VectorCopy( map->faceplanewithoffset.normal, pos_normal );
	}

	VectorMA( pos, DEFAULT_HUNT_OFFSET, pos_normal, pos );

	// might be smaller than DEFAULT_HUNT_OFFSET
	hunt_offset = DotProduct( pos, map->faceplanewithoffset.normal ) - map->faceplanewithoffset.dist;

	// push the point 0.2 units around to avoid walls
	if( !HuntForWorld( pos, vec3_origin, &map->faceplanewithoffset, hunt_size, hunt_scale, hunt_offset ))
	{
		return false;
	}

	if( edgetest && !PointInWindingEpsilon( map->facewindingwithoffset, map->faceplanewithoffset.normal, pos, DEFAULT_EDGE_WIDTH ))
	{
		vec_t	width = DEFAULT_EDGE_WIDTH;
		vec3_t	test;

		VectorCopy( pos, test );

		// if the sample has gone beyond face boundaries, be careful that it hasn't passed a wall
		WindingSnapPointEpsilon( map->facewindingwithoffset, map->faceplanewithoffset.normal, test, width, width * 4 );

		if( !HuntForWorld( test, vec3_origin, &map->faceplanewithoffset, hunt_size, hunt_scale, hunt_offset ))
		{
			return false;
		}

		if( TestLine( threadnum, pos, test, true ) != CONTENTS_EMPTY )
			return false;
	}

	VectorCopy( pos, pos_out );

	return true;
}

static void CalcSinglePosition( int threadnum, positionmap_t *map, int is, int it )
{
	vec_t		smin, smax, tmin, tmax;
	const vec3_t	v_s = { 1.0, 0.0, 0.0 };
	const vec3_t	v_t = { 0.0, 1.0, 0.0 };
	dplane_t		clipplanes[4];
	winding_t		*zone;
	position_t	*p;

	p = &map->grid[is + map->w * it];
	smin = map->start[0] + is * map->step[0];
	smax = map->start[0] + (is + 1) * map->step[0];
	tmin = map->start[1] + it * map->step[1];
	tmax = map->start[1] + (it + 1) * map->step[1];

	// setup clip planes
	VectorScale( v_s,  1.0, clipplanes[0].normal );
	clipplanes[0].dist =  smin;

	VectorScale( v_s, -1.0, clipplanes[1].normal );
	clipplanes[1].dist = -smax;

	VectorScale( v_t,  1.0, clipplanes[2].normal );
	clipplanes[2].dist =  tmin;

	VectorScale( v_t, -1.0, clipplanes[3].normal );
	clipplanes[3].dist = -tmax;

	zone = CopyWinding( map->texwinding );

	for( int x = 0; x < 4 && zone != NULL; x++ )
		ChopWindingInPlace( &zone, clipplanes[x].normal, clipplanes[x].dist, ON_EPSILON, false );

	if( !zone )
	{
		p->valid = false;
	}
	else
	{
		vec3_t	original_st;
		vec3_t	test_st;

		original_st[0] = map->start[0] + (is + 0.5) * map->step[0];
		original_st[1] = map->start[1] + (it + 0.5) * map->step[1];
		original_st[2] = 0.0;
		p->valid = false;

		VectorCopy (original_st, test_st);
		WindingSnapPoint( zone, map->texplane.normal, test_st );

		if( IsPositionValid( threadnum, map, test_st, p->pos ))
		{
			p->best_s = test_st[0];
			p->best_t = test_st[1];
			p->valid = true;
		}

		if( !p->valid )
		{
			WindingCenter( zone, test_st );

			if( IsPositionValid( threadnum, map, test_st, p->pos ))
			{
				p->best_s = test_st[0];
				p->best_t = test_st[1];
				p->valid = true;
			}
		}

		if( !p->valid && !g_fastmode )
		{
			for( int i = 0; i < MAX_NUDGES; i++ )
			{
				VectorMultiply( g_nudgelist[i], map->step, test_st );
				VectorAdd( test_st, original_st, test_st );
				WindingSnapPoint( zone, map->texplane.normal, test_st );

				if( IsPositionValid( threadnum, map, test_st, p->pos ))
				{
					p->best_s = test_st[0];
					p->best_t = test_st[1];
					p->valid = true;
					break;
				}
			}
		}

		FreeWinding( zone );
	}
}

/*
============
FindFacePositions

assume g_face_offset and g_face_centroids are valid
and g_edgeshare have been calculated
============
*/
void FindFacePositions( int facenum, int threadnum )
{
	vec_t		texmins[2], texmaxs[2];
	int		imins[2], imaxs[2];
	const vec3_t	v_up = { 0, 0, 1 };
	int		x, k, is, it;
	vec_t		density;
	positionmap_t	*map;
	dtexinfo_t	*ti;
	dface_t		*f;
	vec3_t		v;

	f = &g_dfaces[facenum];
	map = &g_face_positions[facenum];
	map->valid = true;
	map->facenum = facenum;
	map->facewinding = NULL;
	map->facewindingwithoffset = NULL;
	map->texwinding = NULL;
	map->grid = NULL;
	
	ti = &g_texinfo[f->texinfo];

	if( FBitSet( ti->flags, TEX_SPECIAL ))
	{
		map->valid = false;
		return;
	}

	VectorCopy( g_face_offset[facenum], map->face_offset );
	TranslateWorldToTex( facenum, map->worldtotex );

	if( !Matrix3x4_Invert_Full( map->textoworld, map->worldtotex ))
	{
		map->valid = false;
		return;
	}
	
	map->facewinding = WindingFromFace( f );
	map->faceplane = *GetPlaneFromFace( f );
	map->facewindingwithoffset = AllocWinding( map->facewinding->numpoints );
	map->facewindingwithoffset->numpoints = map->facewinding->numpoints;

	for( x = 0; x < map->facewinding->numpoints; x++ )
		VectorAdd( map->facewinding->p[x], map->face_offset, map->facewindingwithoffset->p[x] );

	map->faceplanewithoffset = map->faceplane;
	map->faceplanewithoffset.dist = map->faceplane.dist + DotProduct( map->face_offset, map->faceplane.normal );

	map->texwinding = AllocWinding( map->facewinding->numpoints );
	map->texwinding->numpoints = map->facewinding->numpoints;

	for( x = 0; x < map->facewinding->numpoints; x++ )
	{
		Matrix3x4_VectorTransform( map->worldtotex, map->facewinding->p[x], map->texwinding->p[x] );
		map->texwinding->p[x][2] = 0.0;
	}

	RemoveColinearPointsEpsilon( map->texwinding, ON_EPSILON );
	VectorCopy( v_up, map->texplane.normal );

	if( Matrix3x4_CalcSign( map->worldtotex ) < 0.0 )
		map->texplane.normal[2] *= -1.0;
	map->texplane.dist = 0.0;

	if( map->texwinding->numpoints == 0 )
	{
		FreeWinding( map->facewindingwithoffset );
		FreeWinding( map->facewinding );
		FreeWinding( map->texwinding );

		map->facewindingwithoffset = NULL;
		map->facewinding = NULL;
		map->texwinding = NULL;

		map->valid = false;
		return;
	}

	VectorSubtract( g_face_centroids[facenum], map->face_offset, v );
	Matrix3x4_VectorTransform( map->worldtotex, v, map->texcentroid );
	map->texcentroid[2] = 0.0;

	int	texture_step = GetTextureStep( ti );

	for( x = 0; x < map->texwinding->numpoints; x++ )
	{
		for( k = 0; k < 2; k++ )
		{
			if( x == 0 || map->texwinding->p[x][k] < texmins[k] )
				texmins[k] = map->texwinding->p[x][k];
			if( x == 0 || map->texwinding->p[x][k] > texmaxs[k] )
				texmaxs[k] = map->texwinding->p[x][k];
		}
	}

	if( g_fastmode || texture_step == 1.0 )
		density = 1.0;
	else if( texture_step < 8.0 )
		density = 2.0;
	else density = 3.0;

	// experimental
	density = 1.0f;

	map->step[0] = (vec_t)texture_step / density;
	map->step[1] = (vec_t)texture_step / density;
	map->step[2] = 1.0;

	for( k = 0; k < 2; k++ )
	{
		imins[k] = (int)floor( texmins[k] / map->step[k] + 0.5 - ON_EPSILON );
		imaxs[k] = (int)ceil( texmaxs[k] / map->step[k] - 0.5 + ON_EPSILON );
	}

	map->start[0] = (imins[0] - 0.5) * map->step[0];
	map->start[1] = (imins[1] - 0.5) * map->step[1];
	map->start[2] = 0.0;
	map->w = imaxs[0] - imins[0] + 1;
	map->h = imaxs[1] - imins[1] + 1;

	if( map->w <= 0 || map->h <= 0 || (double)map->w * (double)map->h > 99999999 )
	{
		FreeWinding( map->facewindingwithoffset );
		FreeWinding( map->facewinding );
		FreeWinding( map->texwinding );

		map->facewindingwithoffset = NULL;
		map->facewinding = NULL;
		map->texwinding = NULL;

		map->valid = false;
		return;
	}

	map->grid = (position_t *)Mem_Alloc( map->w * map->h * sizeof( position_t ));

	for( it = 0; it < map->h; it++ )
	{
		for( is = 0; is < map->w; is++ )
		{
			CalcSinglePosition( threadnum, map, is, it );
		}
	}
}

void FreeFacePositions( void )
{
	if( g_drawsample )
	{
		char	path[MAX_PATH], name[MAX_PATH];

		Q_strncpy( path, source, sizeof( path ));
		COM_StripExtension( path );
		Q_snprintf( name, sizeof( name ), "%s.pts", path );
		Msg( "Writing '%s' ...\n", name );

		FILE *f = fopen( name, "w" );

		if( f )
		{
			vec3_t	v;

			for( int i = 0; i < g_numfaces; i++ )
			{
				positionmap_t	*map = &g_face_positions[i];

				if( !map->valid ) continue;

				for( int j = 0; j < map->h * map->w; j++ )
				{
					if( !map->grid[j].valid )
						continue;

					VectorCopy( map->grid[j].pos, v );
					fprintf( f, "%g %g %g\n", v[0], v[1], v[2] );
				}
			}

			fclose( f );
			Msg( "OK.\n" );
		}
		else Msg( "Error.\n" );
	}

	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		positionmap_t	*map = &g_face_positions[facenum];

		if( map->valid )
		{
			FreeWinding( map->facewindingwithoffset );
			FreeWinding( map->facewinding );
			FreeWinding( map->texwinding );
			Mem_Free( map->grid );

			map->facewindingwithoffset = NULL;
			map->facewinding = NULL;
			map->texwinding = NULL;
			map->valid = false;
			map->grid = NULL;
		}
	}
}

bool FindNearestPosition( int facenum, const winding_t *w, vec_t s, vec_t t, vec3_t pos, vec_t *best_s, vec_t *best_t, vec_t *dist )
{
	int		itmin, itmax, ismin, ismax;
	const vec3_t	v_s = { 1, 0, 0 };
	const vec3_t	v_t = { 0, 1, 0 };
	vec3_t		original_st;
	int		x, is, it;
	int		best_is;
	int		best_it;
	vec_t		best_dist;
	bool		found;
	positionmap_t	*map;
	vec3_t		v;

	map = &g_face_positions[facenum];
	if( !map->valid ) return false;

	VectorSet( original_st, s, t, 0.0 );

	if( PointInWindingEpsilon( map->texwinding, map->texplane.normal, original_st, 4 * ON_EPSILON, false ))
	{
		itmin = (int)ceil(( original_st[1] - map->start[1] - 2 * ON_EPSILON ) / map->step[1]) - 1;
		itmax = (int)floor(( original_st[1] - map->start[1] + 2 * ON_EPSILON ) / map->step[1]);
		ismin = (int)ceil(( original_st[0] - map->start[0] - 2 * ON_EPSILON ) / map->step[0]) - 1;
		ismax = (int)floor(( original_st[0] - map->start[0] + 2 * ON_EPSILON ) / map->step[0]);
		itmin = Q_max( 0, itmin );
		itmax = Q_min( itmax, map->h - 1 );
		ismin = Q_max( 0, ismin );
		ismax = Q_min( ismax, map->w - 1 );
		found = false;

		for( it = itmin; it <= itmax; it++ )
		{
			for( is = ismin; is <= ismax; is++ )
			{
				vec3_t		current_st;
				position_t	*p;
				vec_t		d;

				p = &map->grid[is + map->w * it];
				if( !p->valid ) continue;

				current_st[0] = p->best_s;
				current_st[1] = p->best_t;
				current_st[2] = 0.0;

				VectorSubtract( current_st, original_st, v );
				d = VectorLength( v );

				if( !found || d < best_dist - 2 * ON_EPSILON )
				{
					found = true;
					best_is = is;
					best_it = it;
					best_dist = d;
				}
			}
		}

		if( found )
		{
			position_t *p;

			p = &map->grid[best_is + map->w * best_it];
			VectorCopy( p->pos, pos );
			*best_s = p->best_s;
			*best_t = p->best_t;
			*dist = 0.0;

			return true;
		}
	}

	itmin = map->h;
	itmax = -1;
	ismin = map->w;
	ismax = -1;

	for( x = 0; x < w->numpoints; x++ )
	{
		it = (int)floor(( w->p[x][1] - map->start[1] + 0.5 * ON_EPSILON ) / map->step[1]);
		itmin = Q_min( itmin, it );
		it = (int)ceil(( w->p[x][1] - map->start[1] - 0.5 * ON_EPSILON ) / map->step[1]) - 1;
		itmax = Q_max( it, itmax );
		is = (int)floor(( w->p[x][0] - map->start[0] + 0.5 * ON_EPSILON ) / map->step[0]);
		ismin = Q_min( ismin, is );
		is = (int)ceil(( w->p[x][0] - map->start[0] - 0.5 * ON_EPSILON ) / map->step[0]) - 1;
		ismax = Q_max( is, ismax );
	}

	itmin = Q_max( 0, itmin );
	itmax = Q_min( itmax, map->h - 1 );
	ismin = Q_max( 0, ismin );
	ismax = Q_min( ismax, map->w - 1 );
	found = false;

	for( it = itmin; it <= itmax; it++ )
	{
		for( is = ismin; is <= ismax; is++ )
		{
			vec3_t		current_st;
			position_t	*p;
			vec_t		d;

			p = &map->grid[is + map->w * it];
			if( !p->valid ) continue;

			current_st[0] = p->best_s;
			current_st[1] = p->best_t;
			current_st[2] = 0.0;

			VectorSubtract( current_st, original_st, v );
			d = VectorLength( v );

			if( !found || d < best_dist - ON_EPSILON )
			{
				found = true;
				best_is = is;
				best_it = it;
				best_dist = d;
			}
		}
	}

	if( found )
	{
		position_t *p;

		p = &map->grid[best_is + map->w * best_it];
		VectorCopy( p->pos, pos );
		*best_s = p->best_s;
		*best_t = p->best_t;
		*dist = best_dist;
		return true;
	}

	return false;
}

/*
============
CalcPositionsSize
============
*/
void CalcPositionsSize( void )
{
	int	total_size = 0;

	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		positionmap_t	*map = &g_face_positions[facenum];

		if( !map->valid ) continue;

		total_size += map->w * map->h * sizeof( position_t );
		total_size += WindingSize( map->facewindingwithoffset );
		total_size += WindingSize( map->facewinding );
		total_size += WindingSize( map->texwinding );
	}

	MsgDev( D_INFO, "position maps: %s\n", Q_memprint( total_size ));
}