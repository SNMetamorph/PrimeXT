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
#include "model_trace.h"
#include "../../engine/studio.h"

#ifdef HLRAD_LIGHTMAPMODELS

#define TRI_BORDER		0.4f	// FIXME: this is too much

typedef struct
{
	int	modelnum : 10;
	int	facenum : 22;
} faceremap_t;

typedef struct
{
	float	*point;
	float	*coord;
} trivert_t;

static entity_t	*g_modellight[MAX_MAP_MODELS];
static int	g_modellight_modnum;
static faceremap_t	*g_modellight_indexes;
static uint	g_modellight_numindexes;
static float	g_nudge[2][9] = {{ 0, -1, 0, 1, -1, 1, -1, 0, 1 }, { 0, -1, -1, -1, 0, 0, 1, 1, 1 }};
static float	g_studio_blur;

static void CalcLightmapAxis( tmesh_t *mesh, lface_t *face, trivert_t *a, trivert_t *b, trivert_t *c )
{
	int	ssize = face->texture_step;
	vec3_t	mins, maxs, size;
	vec3_t	planeNormal;
	int	i, axis;
	float	d;

	ClearBounds( mins, maxs );
	AddPointToBounds( a->point, mins, maxs );
	AddPointToBounds( b->point, mins, maxs );
	AddPointToBounds( c->point, mins, maxs );

	// round to the lightmap resolution
	for( i = 0; i < 3; i++ )
	{
		mins[i] = ssize * floor( mins[i] / ssize );
		maxs[i] = ssize * ceil( maxs[i] / ssize );
		size[i] = (maxs[i] - mins[i]) / ssize;
	}

	// the two largest axis will be the lightmap size
	planeNormal[0] = fabs( face->normal[0] );
	planeNormal[1] = fabs( face->normal[1] );
	planeNormal[2] = fabs( face->normal[2] );

	if( planeNormal[0] >= planeNormal[1] && planeNormal[0] >= planeNormal[2] )
	{
		face->extents[0] = size[1];
		face->extents[1] = size[2];
		face->lmvecs[0][1] = 1.0 / ssize;
		face->lmvecs[1][2] = 1.0 / ssize;
		axis = 0;
	}
	else if( planeNormal[1] >= planeNormal[0] && planeNormal[1] >= planeNormal[2] )
	{
		face->extents[0] = size[0];
		face->extents[1] = size[2];
		face->lmvecs[0][0] = 1.0 / ssize;
		face->lmvecs[1][2] = 1.0 / ssize;
		axis = 1;
	}
	else
	{
		face->extents[0] = size[0];
		face->extents[1] = size[1];
		face->lmvecs[0][0] = 1.0 / ssize;
		face->lmvecs[1][1] = 1.0 / ssize;
		axis = 2;
	}

	if( !face->normal[axis] )
		COM_FatalError( "Chose a 0 valued axis\n" );

	if( face->extents[0] > ( MAX_STUDIO_LIGHTMAP_SIZE - 1 ))
	{
		VectorScale( face->lmvecs[0], (float)(MAX_STUDIO_LIGHTMAP_SIZE - 1.0f) / face->extents[0], face->lmvecs[0] );
		face->extents[0] = MAX_STUDIO_LIGHTMAP_SIZE - 1;
	}
	
	if( face->extents[1] > ( MAX_STUDIO_LIGHTMAP_SIZE - 1 ))
	{
		VectorScale( face->lmvecs[1], (float)(MAX_STUDIO_LIGHTMAP_SIZE - 1.0f) / face->extents[1], face->lmvecs[1] );
		face->extents[1] = MAX_STUDIO_LIGHTMAP_SIZE - 1;
	}

	// calculate the world coordinates of the lightmap samples

	// project mins onto plane to get origin
	d = DotProduct( mins, face->normal ) - DotProduct( face->normal, a->point );
	d /= face->normal[axis];
	VectorCopy( mins, face->origin );
	face->origin[axis] -= d;

	// project stepped lightmap blocks and subtract to get planevecs
	for( i = 0; i < 2 ; i++ )
	{
		vec3_t	normalized;
		float	len;

		len = VectorNormalizeLength2( face->lmvecs[i], normalized );
		VectorScale( normalized, (1.0 / len), face->lmvecs[i] );
		d = DotProduct( face->lmvecs[i], face->normal );
		d /= face->normal[axis];
		face->lmvecs[i][axis] -= d;
	}

}

// we need to compute tangent vectors
void CalcTriangleVectors( int modelnum, int threadnum = -1 )
{
	entity_t	*mapent = g_modellight[modelnum];
	tmesh_t	*mesh;
	trivert_t	v[3];
	int hashSize;

	// sanity check
	if( !mapent || !mapent->cache )
		return;

	mesh = (tmesh_t *)mapent->cache;
	if( !mesh->verts || mesh->numverts <= 0 )
		return;

	if( !mesh->faces || mesh->numfaces <= 0 )
		return;

	// build the smoothed normals
	if( !FBitSet( mesh->flags, FMESH_DONT_SMOOTH ) && g_smoothing_threshold )
	{
		vec3_t	*normals = (vec3_t *)Mem_Alloc( mesh->numverts * sizeof( vec3_t ));

		for( hashSize = 1; hashSize < mesh->numverts; hashSize <<= 1 );
		hashSize = hashSize >> 2;

		// build a map from vertex to a list of triangles that share the vert.
		CUtlArray<CIntVector> vertHashMap;

		vertHashMap.AddMultipleToTail( hashSize );

		for( int vertID = 0; vertID < mesh->numverts; vertID++ )
		{
			tvert_t *tv = &mesh->verts[vertID];
			uint hash = VertexHashKey( tv->point, hashSize );
			vertHashMap[hash].AddToTail( vertID );
		}

		for( int hashID = 0; hashID < hashSize; hashID++ )
		{
			for( int i = 0; i < vertHashMap[hashID].Size(); i++ )
			{
				int vertID = vertHashMap[hashID][i];
				tvert_t *tv0 = &mesh->verts[vertID];

				for( int j = 0; j < vertHashMap[hashID].Size(); j++ )
				{
					tvert_t *tv1 = &mesh->verts[vertHashMap[hashID][j]];

					if( !VectorCompareEpsilon( tv0->point, tv1->point, ON_EPSILON ))
						continue;

					if( DotProduct( tv0->normal, tv1->normal ) >= g_smoothing_threshold )
						VectorAdd( normals[vertID], tv1->normal, normals[vertID] );
				}
			}
		}

		// copy smoothed normals back
		for( int j = 0; j < mesh->numverts; j++ )
		{
			VectorCopy( normals[j], mesh->verts[j].normal );
			VectorNormalize2( mesh->verts[j].normal );
		}

		Mem_Free( normals );
	}

	for( int triID = 0; triID < mesh->numfaces; triID++ )
	{
		tface_t	*face = &mesh->faces[triID];

		if( !face->light ) continue;

		// get positions and UV from three points
		v[0].point = (float *)&mesh->verts[face->a].point;
		v[0].coord = (float *)&mesh->verts[face->a].st;
		v[1].point = (float *)&mesh->verts[face->b].point;
		v[1].coord = (float *)&mesh->verts[face->b].st;
		v[2].point = (float *)&mesh->verts[face->c].point;
		v[2].coord = (float *)&mesh->verts[face->c].st;
		face->light->texture_step = mesh->texture_step;

		if( VectorIsNull( face->light->normal ))
			continue;

		CalcLightmapAxis( mesh, face->light, &v[0], &v[1], &v[2] );
	}
}

bool GetTrianglePhongNormal( lightinfo_t *l, const vec3_t spot, vec3_t phongnormal )
{
	tface_t	*face = l->tface;
	tvert_t	*a = &l->mesh->verts[face->a];
	tvert_t	*b = &l->mesh->verts[face->b];
	tvert_t	*c = &l->mesh->verts[face->c];
	vec3_t	edge1, edge2, p0, p1;
	float	uu, uv, vv, wu, wv;
	float	d1, d2, d, frac;
	vec3_t	q, n, p;
	float	u, v, w;

	// setup fallback
	VectorCopy( l->tface->light->normal, phongnormal );
#if 0
	VectorSubtract( b->point, a->point, edge1 );
	VectorSubtract( c->point, a->point, edge2 );
	CrossProduct( edge2, edge1, n );
	VectorMA( spot, DEFAULT_HUNT_OFFSET, n, p0 );
	VectorMA( spot,-DEFAULT_HUNT_OFFSET, n, p1 );

	VectorSubtract( p1, p0, p );
	VectorSubtract( p0, a->point, q );

	d1 = -DotProduct( n, q );
	d2 = DotProduct( n, p );

	if( fabs( d2 ) < FRAC_EPSILON )
		return false; // parallel with plane

	// get intersect point of ray with triangle plane
	frac = d1 / d2;
	if( frac < -0.001f ) return false;

	// calculate the impact point
	VectorLerp( p0, frac, p1, p );

	// does p lie inside triangle?
	uu = DotProduct( edge1, edge1 );
	uv = DotProduct( edge1, edge2 );
	vv = DotProduct( edge2, edge2 );

	VectorSubtract( p, a->point, q );
	wu = DotProduct( q, edge1 );
	wv = DotProduct( q, edge2 );
	d = uv * uv - uu * vv;

	// get and test parametric coords
	u = (uv * wv - vv * wu) / d;
	if( u < -TRI_BORDER || u > ( 1.0f + TRI_BORDER ))
		return false; // p is outside

	v = (uv * wu - uu * wv) / d;
	if( v < -TRI_BORDER || (u + v) > ( 1.0f + TRI_BORDER ))
		return false; // p is outside
	// calculate w parameter
	w = 1.0f - ( u + v );
#else
	// now, check 3 edges
	float hitc1 = spot[face->pcoord0] + ( -phongnormal[face->pcoord0] );
	float hitc2 = spot[face->pcoord1] + ( -phongnormal[face->pcoord1] );
					
	// do barycentric coordinate check
	v = face->edge1[0] * hitc1 + face->edge1[1] * hitc2 + face->edge1[2];
	if( v < -TRI_BORDER ) return false;

	w = face->edge2[0] * hitc1 + face->edge2[1] * hitc2 + face->edge2[2];
	if( w < -TRI_BORDER ) return false;

	u = v + w;
	if( u > 1.0f + TRI_BORDER ) return false;

	// calculate w parameter
	u = 1.0f - u;
#endif
	if( g_smoothing_threshold > 0.0 )
	{
		u = bound( 0.0f, u, 1.0f );
		v = bound( 0.0f, v, 1.0f );
		w = bound( 0.0f, w, 1.0f );

		// calculate st from uvw (barycentric) coordinates
		phongnormal[0] = w * a->normal[0] + u * b->normal[0] + v * c->normal[0];
		phongnormal[1] = w * a->normal[1] + u * b->normal[1] + v * c->normal[1];
		phongnormal[2] = w * a->normal[2] + u * b->normal[2] + v * c->normal[2];
//		VectorNormalize2( phongnormal );
	}

	return true;
}

/*
================
CalcTriangleExtents

Fills in s->texmins[] and s->texsize[]
also sets exactmins[] and exactmaxs[]
================
*/
void CalcTriangleExtents( lightinfo_t *l )
{
	l->texsize[0] = l->tface->light->extents[0];
	l->texsize[1] = l->tface->light->extents[1];

	if( l->texsize[0] * l->texsize[1] > ( MAX_SINGLEMAP_MODEL / 3 ))
		COM_FatalError( "surface to large to map %d > %d\n", l->texsize[0] * l->texsize[1], ( MAX_SINGLEMAP_MODEL / 3 ));

	if( l->texsize[0] < 0 || l->texsize[1] < 0 )
		COM_FatalError( "negative extents\n" );

	l->lmcache_density = 1;
	l->lmcache_side = (int)ceil(( 0.5 * g_studio_blur * l->lmcache_density - 0.5 ) * ( 1.0 - NORMAL_EPSILON ));
	l->lmcache_offset = l->lmcache_side;
	l->lmcachewidth = l->texsize[0] * l->lmcache_density + 1 + 2 * l->lmcache_side;
	l->lmcacheheight = l->texsize[1] * l->lmcache_density + 1 + 2 * l->lmcache_side;

	l->light = (vec3_t (*)[MAXLIGHTMAPS])Mem_Alloc( l->lmcachewidth * l->lmcacheheight * sizeof( vec3_t[MAXLIGHTMAPS] ));
#ifdef HLRAD_DELUXEMAPPING
	l->deluxe = (vec3_t (*)[MAXLIGHTMAPS])Mem_Alloc( l->lmcachewidth * l->lmcacheheight * sizeof( vec3_t[MAXLIGHTMAPS] ));
	l->normals = (vec3_t *)Mem_Alloc( l->lmcachewidth * l->lmcacheheight * sizeof( vec3_t ));
#ifdef HLRAD_SHADOWMAPPING
	l->shadow = (vec_t (*)[MAXLIGHTMAPS])Mem_Alloc( l->lmcachewidth * l->lmcacheheight * sizeof( vec_t[MAXLIGHTMAPS] ));
#endif
#endif
}

/*
=================
CalcPoints

For each texture aligned grid point, back project onto the plane
to get the world xyz value of the sample point
=================
*/
void CalcTrianglePoints( lightinfo_t *l, float sofs, float tofs )
{
	int	h = l->texsize[1] + 1;
	int	w = l->texsize[0] + 1;
	lface_t	*face = l->tface->light;
	int	s, t, i, j;
	dleaf_t	*leaf;

	l->surfpt = (surfpt_t *)Mem_Alloc( w * h * sizeof( surfpt_t ));
	l->numsurfpt = w * h;

	for( t = 0; t < h; t++ )
	{
		for( s = 0; s < w; s++ )
		{
			surfpt_t	*surf = &l->surfpt[s+w*t];

			// calculate texture point
			for( j = 0; j < 3; j++ )
				surf->point[j] = face->origin[j] + face->normal[j] + s * face->lmvecs[0][j] + t * face->lmvecs[1][j];

			// we may need to slightly nudge the sample point
			// if directly on a wall
			for( i = 0; i < 9; i++ )
			{
				// calculate texture point
				for( j = 0; j < 3; j++ )
					surf->position[j] = surf->point[j]
					+ ( g_nudge[0][i] / 16.0f ) * face->lmvecs[0][j]
					+ ( g_nudge[1][i] / 16.0f ) * face->lmvecs[1][j];

				leaf = PointInLeaf( surf->position );

				if( leaf->contents != CONTENTS_SOLID )
				{
//					if( TestLine( -1, face->origin, surf->position ) == CONTENTS_EMPTY )
						break; // got it
				}
			}

			if( i == 9 ) surf->occluded = true;
		}
	}
}

/*
=============
InitLightinfo
=============
*/
void InitModelLightinfo( lightinfo_t *l, entity_t *mapent, tmesh_t *mesh, tface_t *tf )
{
	memset( l, 0, sizeof( *l ));
	l->mapent = mapent;
	l->mesh = mesh;
	l->tface = tf;

	CalcTriangleExtents( l );
}

static void CalcModelLightmap( int thread, lightinfo_t *l, facelight_t *fl )
{
	vec_t	density = (vec_t)l->lmcache_density;
	vec_t	texture_step = l->tface->light->texture_step;
	int	w = l->texsize[0] + 1;
	int	h = l->texsize[1] + 1;
	lface_t	*f = l->tface->light;
	byte	*vislight = NULL;
	int	i, j, count;
	vec3_t	acolor, adelux;
	float	ashadow;

#ifdef HLRAD_COMPUTE_VISLIGHTMATRIX
	vislight = l->mesh->vislight;
#endif
	ASSERT( l->numsurfpt > 0 );

	// allocate light samples
	fl->samples = (sample_t *)Mem_Alloc( l->numsurfpt * sizeof( sample_t ));
	fl->numsamples = l->numsurfpt;

	// stats
	g_direct_luxels[thread] += fl->numsamples;

	// copy surf points from lightinfo with offset 0,0
	for( i = 0; i < fl->numsamples; i++ )
	{
		VectorCopy( l->surfpt[i].point, fl->samples[i].pos );
		fl->samples[i].occluded = l->surfpt[i].occluded; 
		fl->samples[i].surface = l->surfpt[i].surface;
	}

	// for each sample whose light we need to calculate
	for( i = 0; i < l->lmcachewidth * l->lmcacheheight; i++ )
	{
		vec3_t	pointnormal;
		vec3_t	spot, surfpt;
		bool	blocked;

		// prepare input parameter and output parameter
		vec_t	s = ((i % l->lmcachewidth) - l->lmcache_offset) / (vec_t)l->lmcache_density;
		vec_t	t = ((i / l->lmcachewidth) - l->lmcache_offset) / (vec_t)l->lmcache_density;
		int	nearest_s = Q_max( 0, Q_min((int)floor( s + 0.5 ), l->texsize[0] ));
		int	nearest_t = Q_max( 0, Q_min((int)floor( t + 0.5 ), l->texsize[1] ));

		j = nearest_s + w * nearest_t;

		VectorCopy( l->surfpt[j].position, surfpt );
		VectorMA( surfpt, DEFAULT_HUNT_OFFSET, f->normal, spot );

		// calculate normal for the sample
		if( !GetTrianglePhongNormal( l, surfpt, pointnormal ))
			l->surfpt[j].occluded = true;

		// find world's position for the sample
		blocked = l->surfpt[j].occluded;

#ifdef HLRAD_DELUXEMAPPING
		VectorCopy( pointnormal, l->normals[i] );
#endif
		if( blocked ) continue;

		// calculate visibility for the sample
		int leaf = PointInLeaf( spot ) - g_dleafs;

		// gather light
#if defined( HLRAD_DELUXEMAPPING ) && defined( HLRAD_SHADOWMAPPING )
		GatherSampleLight( thread, -1, &spot, leaf, pointnormal, l->light[i], l->deluxe[i], l->shadow[i], f->styles, vislight, 0, l->mapent );
		GatherSampleLight( thread, -1, &spot, leaf, pointnormal, l->light[i], l->deluxe[i], l->shadow[i], f->styles, vislight, 1, l->mapent );
#elif defined( HLRAD_DELUXEMAPPING )
		GatherSampleLight( thread, -1, &spot, leaf, pointnormal, l->light[i], l->deluxe[i], NULL, f->styles, vislight, 0, l->mapent );
		GatherSampleLight( thread, -1, &spot, leaf, pointnormal, l->light[i], l->deluxe[i], NULL, f->styles, vislight, 1, l->mapent );
#else
		GatherSampleLight( thread, -1, &spot, leaf, pointnormal, l->light[i], NULL, NULL, f->styles, vislight, 0, l->mapent );
		GatherSampleLight( thread, -1, &spot, leaf, pointnormal, l->light[i], NULL, NULL, f->styles, vislight, 1, l->mapent );
#endif
	}

	for( i = 0; i < fl->numsamples; i++ )
	{
#ifdef HLRAD_DELUXEMAPPING
		vec_t	weighting_correction;
		vec3_t	centernormal;
#endif
		int	s_center, t_center;
		vec_t	weighting, subsamples;
		int	s, t, pos;
		vec_t	sizehalf;

		s_center = (i % w) * l->lmcache_density + l->lmcache_offset;
		t_center = (i / w) * l->lmcache_density + l->lmcache_offset;
		sizehalf = 0.5 * g_studio_blur * l->lmcache_density;
		subsamples = 0.0;
#ifdef HLRAD_DELUXEMAPPING
		VectorCopy( l->normals[s_center + l->lmcachewidth * t_center], centernormal );
#endif
		for( s = s_center - l->lmcache_side; s <= s_center + l->lmcache_side; s++ )
		{
			for( t = t_center - l->lmcache_side; t <= t_center + l->lmcache_side; t++ )
			{
				weighting = (Q_min( 0.5, sizehalf - ( s - s_center )) - Q_max( -0.5, -sizehalf - ( s - s_center )));
				weighting	*=(Q_min( 0.5, sizehalf - ( t - t_center )) - Q_max( -0.5, -sizehalf - ( t - t_center )));

				pos = s + l->lmcachewidth * t;
#ifdef HLRAD_DELUXEMAPPING
				// when blur distance (g_blur) is large, the subsample can be very far from the original lightmap sample
				// in some cases such as a thin cylinder, the subsample can even grow into the opposite side
				// as a result, when exposed to a directional light, the light on the cylinder may "leak" into
				// the opposite dark side this correction limits the effect of blur distance when the normal changes very fast
				// this correction will not break the smoothness that HLRAD_GROWSAMPLE ensures
				weighting_correction = DotProduct( l->normals[pos], centernormal );
				weighting_correction = (weighting_correction > 0) ? weighting_correction * weighting_correction : 0;
				weighting = weighting * weighting_correction;
#endif
				for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
				{
					VectorMA( fl->samples[i].light[j], weighting, l->light[pos][j], fl->samples[i].light[j] );
#ifdef HLRAD_DELUXEMAPPING
					VectorMA( fl->samples[i].deluxe[j], weighting, l->deluxe[pos][j], fl->samples[i].deluxe[j] );
#ifdef HLRAD_SHADOWMAPPING
					fl->samples[i].shadow[j] += l->shadow[pos][j] * weighting;
#endif
#endif
				}
				subsamples += weighting;
			}
		}

		if( subsamples > NORMAL_EPSILON )
		{
#ifdef HLRAD_DELUXEMAPPING
			VectorCopy( centernormal, fl->samples[i].normal );
#endif
			for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
			{
				VectorScale( fl->samples[i].light[j], (1.0 / subsamples), fl->samples[i].light[j] );
#ifdef HLRAD_DELUXEMAPPING
				VectorScale( fl->samples[i].deluxe[j], (1.0 / subsamples), fl->samples[i].deluxe[j] );
#ifdef HLRAD_SHADOWMAPPING
				fl->samples[i].shadow[j] *= (1.0 / subsamples);
#endif
#endif
			}
		}
	}

#ifdef HLRAD_DELUXEMAPPING
#ifdef HLRAD_SHADOWMAPPING
	// multiply light by shadow to prevent blur artifacts
	for( i = 0; i < fl->numsamples; i++ )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
//			VectorScale( fl->samples[i].light[j], fl->samples[i].shadow[j], fl->samples[i].light[j] );
//			VectorScale( fl->samples[i].deluxe[j], fl->samples[i].shadow[j], fl->samples[i].deluxe[j] );
		}
	}

	// output occlusion shouldn't be blured
	for( i = 0; i < fl->numsamples; i++ )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
			int	s = (i % w) + l->lmcache_side;
			int	t = (i / w) + l->lmcache_side;
			int	pos = s + l->lmcachewidth * t;
			fl->samples[i].shadow[j] = l->shadow[pos][j];
		}
	}
#endif
#endif
	// calculate average values for occluded samples
	for( int k = 0; k < MAXLIGHTMAPS && f->styles[k] != 255; k++ )
	{
		for( i = 0; i < w; i++ )
		{
			for( j = 0; j < h; j++ )
			{
				int pos0 = i+w*j;
				if( !l->surfpt[pos0].occluded )
					continue;

				int step = 1; // 3x3

				// scan all surrounding samples
				VectorClear( acolor );
				VectorClear( adelux );
				ashadow = 0.0f;
				count = 0;

				for( int x = -step; x <= step; x++ )
				{
					for( int y = -step; y <= step; y++ )
					{
						if( i + x < 0 || i + x >= w )
							continue;

						if( j + y < 0 || j + y >= h )
							continue;

						int pos1 = (i+x)+w*(j+y);
						if( l->surfpt[pos1].occluded )
							continue;

						VectorAdd( fl->samples[pos1].light[k], acolor, acolor );
#ifdef HLRAD_DELUXEMAPPING
						VectorAdd( fl->samples[pos1].deluxe[k], adelux, adelux );
#ifdef HLRAD_SHADOWMAPPING
						ashadow += fl->samples[pos1].shadow[k];
#endif
#endif
						count++;
					}
				}

				if( !count ) continue;
				VectorScale( acolor, 1.0f / count, fl->samples[pos0].light[k] );
#ifdef HLRAD_DELUXEMAPPING
				VectorScale( adelux, 1.0f / count, fl->samples[pos0].deluxe[k] );
#ifdef HLRAD_SHADOWMAPPING
				fl->samples[pos0].shadow[k] = ashadow * (1.0f / count);				
#endif
#endif
			}
		}
	}

	Mem_Free( l->light );
#ifdef HLRAD_DELUXEMAPPING
	Mem_Free( l->normals );
	Mem_Free( l->deluxe );
#ifdef HLRAD_SHADOWMAPPING
	Mem_Free( l->shadow );
#endif
#endif
}

void BuildModelLightmaps( int indexnum, int thread = -1 )
{
	int		modelnum = g_modellight_indexes[indexnum].modelnum;
	int		facenum = g_modellight_indexes[indexnum].facenum;
	entity_t		*mapent = g_modellight[modelnum];
	entity_t		*ignoreent = NULL;
	byte		*vislight = NULL;
	tmesh_t		*mesh;
	int		i, j;
	sample_t		*s;
	lightinfo_t	l;

	// sanity check
	if( !mapent || !mapent->cache )
		return;

	mesh = (tmesh_t *)mapent->cache;
	if( !mesh->verts || mesh->numverts <= 0 )
		return; 

	if( !mesh->faces || mesh->numfaces <= 0 )
		return; 

	if( !FBitSet( mesh->flags, FMESH_SELF_SHADOW ))
		ignoreent = mapent;

	// lightmaps onto models it's more compilcated than vertexlighting...
	if( !mesh->faces[facenum].light ) return;
	lface_t *f = mesh->faces[facenum].light;
	tface_t *tf = &mesh->faces[facenum];
	facelight_t *fl = &f->facelight;
	f->lightofs = -1;

	if( VectorIsNull( f->normal ))
		return;

	InitModelLightinfo( &l, ignoreent, mesh, tf );
	CalcTrianglePoints( &l, 0.0f, 0.0f );
	CalcModelLightmap( thread, &l, fl );

	Mem_Free( l.surfpt );

	// add an ambient term if desired
	if( g_ambient[0] || g_ambient[1] || g_ambient[2] )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] == 255; j++ );
		if( j == MAXLIGHTMAPS ) f->styles[0] = 0; // adding style

		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
			if( f->styles[j] == 0 )
			{
				s = fl->samples;
				for( i = 0; i < fl->numsamples; i++, s++ )
				{
					VectorAdd( s->light[j], g_ambient, s->light[j] );
#ifdef HLRAD_DELUXEMAPPING
					vec_t avg = VectorAvg( g_ambient );
					VectorMA( s->deluxe[j], -DIFFUSE_DIRECTION_SCALE * avg, s->normal, s->deluxe[j] );
#endif
				}
				break;
			}
		}

	}
}

/*
============
CalcModelSampleSize
============
*/
void CalcModelSampleSize( void )
{
	facelight_t	*fl;
	tface_t		*f;
	size_t		samples_total_size = 0;
	tmesh_t		*mesh;

	for( int i = 0; i < g_modellight_numindexes; i++ )
	{
		int	modelnum = g_modellight_indexes[i].modelnum;
		int	facenum = g_modellight_indexes[i].facenum;
		entity_t	*mapent = g_modellight[modelnum];

         		mesh = (tmesh_t *)mapent->cache;
		f = &mesh->faces[facenum];
		if( !f->light ) continue;
		fl = &f->light->facelight;

		samples_total_size += fl->numsamples * sizeof( sample_t );
	}

	Msg( "total studiolight data: %s\n", Q_memprint( samples_total_size ));
}

/*
============
PrecompModelLightmapOffsets
============
*/
void PrecompModelLightmapOffsets( void )
{
	int		lightstyles;
	tmesh_t		*mesh;
	facelight_t	*fl;
	lface_t		*f;

	for( int l = 0; l < g_modellight_numindexes; l++ )
	{
		int	modelnum = g_modellight_indexes[l].modelnum;
		int	facenum = g_modellight_indexes[l].facenum;
		entity_t	*mapent = g_modellight[modelnum];
		vec3_t	maxlights1[MAXSTYLES];
		vec3_t	maxlights2[MAXSTYLES];
		vec_t	maxlights[MAXSTYLES];
		int	i, j, k;

         		mesh = (tmesh_t *)mapent->cache;
		f = mesh->faces[facenum].light;
		if( !f ) continue;
		fl = &f->facelight;

		for( j = 0; j < MAXSTYLES; j++ )
		{
			VectorClear( maxlights1[j] );
			VectorClear( maxlights2[j] );
		}

		for( k = 0; k < MAXLIGHTMAPS && f->styles[k] != 255; k++ )
		{
			for( i = 0; i < fl->numsamples; i++ )
			{
				VectorCompareMax( maxlights1[f->styles[k]], fl->samples[i].light[k], maxlights1[f->styles[k]] );
			}
		}
#ifdef LATER
		int	numpatches;
		const int	*patches;

		GetTriangulationPatches( facenum, &numpatches, &patches ); // collect patches and their neighbors

		for( i = 0; i < numpatches; i++ )
		{
			patch_t	*patch = &g_patches[patches[i]];

			for( k = 0; k < MAXLIGHTMAPS && patch->totalstyle[k] != 255; k++ )
			{
				VectorCompareMax( maxlights2[patch->totalstyle[k]], patch->totallight[k], maxlights2[patch->totalstyle[k]] );
			}
		}
#endif
		for( j = 0; j < MAXSTYLES; j++ )
		{
			vec3_t	v;

			VectorAdd( maxlights1[j], maxlights2[j], v );
			maxlights[j] = VectorMaximum( v );

			if( maxlights[j] <= EQUAL_EPSILON )
				maxlights[j] = 0;
		}

		byte	oldstyles[MAXLIGHTMAPS];
		sample_t	*oldsamples = (sample_t *)Mem_Alloc( sizeof( sample_t ) * fl->numsamples );

		for( k = 0; k < MAXLIGHTMAPS; k++ )
			oldstyles[k] = f->styles[k];

		// make backup and clear the source
		for( k = 0; k < fl->numsamples; k++ )
		{
			for( j = 0; j < MAXLIGHTMAPS; j++ )
			{
				VectorCopy( fl->samples[k].light[j], oldsamples[k].light[j] );
				VectorClear( fl->samples[k].light[j] );
#ifdef HLRAD_DELUXEMAPPING
				VectorCopy( fl->samples[k].deluxe[j], oldsamples[k].deluxe[j] );
				VectorClear( fl->samples[k].deluxe[j] );
#ifdef HLRAD_SHADOWMAPPING
				oldsamples[k].shadow[j] = fl->samples[k].shadow[j];
				fl->samples[k].shadow[j] = 0.0f;
#endif
#endif
			}
		}

		for( k = 0; k < MAXLIGHTMAPS; k++ )
		{
			byte	beststyle = 255;

			if( k == 0 )
			{
				beststyle = 0;
			}
			else
			{
				vec_t	bestmaxlight = 0;

				for( j = 1; j < MAXSTYLES; j++ )
				{
					if( maxlights[j] > bestmaxlight + NORMAL_EPSILON )
					{
						bestmaxlight = maxlights[j];
						beststyle = j;
					}
				}
			}

			if( beststyle != 255 )
			{
				maxlights[beststyle] = 0;
				f->styles[k] = beststyle;

				for( i = 0; i < MAXLIGHTMAPS && oldstyles[i] != 255; i++ )
				{
					if( oldstyles[i] == f->styles[k] )
						break;
				}

				if( i < MAXLIGHTMAPS && oldstyles[i] != 255 )
				{
					for( j = 0; j < fl->numsamples; j++ )
					{
						VectorCopy( oldsamples[j].light[i], fl->samples[j].light[k] );
#ifdef HLRAD_DELUXEMAPPING
						VectorCopy( oldsamples[j].deluxe[i], fl->samples[j].deluxe[k] );
#ifdef HLRAD_SHADOWMAPPING
						fl->samples[j].shadow[k] = oldsamples[j].shadow[i];
#endif
#endif
					}
				}
				else
				{
					for( j = 0; j < fl->numsamples; j++ )
					{
						VectorClear( fl->samples[j].light[k] );
#ifdef HLRAD_DELUXEMAPPING
						VectorClear( fl->samples[j].deluxe[k] );
#ifdef HLRAD_SHADOWMAPPING
						fl->samples[j].shadow[k] = 0.0f;
#endif
#endif
					}
				}
			}
			else
			{
				f->styles[k] = 255;

				for( j = 0; j < fl->numsamples; j++ )
				{
					VectorClear( fl->samples[j].light[k] );
#ifdef HLRAD_DELUXEMAPPING
					VectorClear( fl->samples[j].deluxe[k] );
#ifdef HLRAD_SHADOWMAPPING
					fl->samples[j].shadow[k] = 0.0f;
#endif
#endif
				}
			}
		}

		Mem_Free( oldsamples );

		for( lightstyles = 0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
		{
			if( f->styles[lightstyles] == 255 )
				break; // end if styles
		}

		if( !lightstyles ) continue;

		f->lightofs = g_lightdatasize;
		g_lightdatasize += fl->numsamples * 3 * lightstyles;
	}
}

/*
============
ScaleModelDirectLights
============
*/
void ScaleModelDirectLights( void )
{
	tmesh_t		*mesh;
	sample_t		*samp;
	facelight_t	*fl;
	lface_t		*f;

	for( int i = 0; i < g_modellight_numindexes; i++ )
	{
		int	modelnum = g_modellight_indexes[i].modelnum;
		int	facenum = g_modellight_indexes[i].facenum;
		entity_t	*mapent = g_modellight[modelnum];

         		mesh = (tmesh_t *)mapent->cache;
		f = mesh->faces[facenum].light;
		if( !f ) continue;
		fl = &f->facelight;

		for( int k = 0; k < MAXLIGHTMAPS && f->styles[k] != 255; k++ )
		{
			for( int i = 0; i < fl->numsamples; i++ )
			{
				samp = &fl->samples[i];
				VectorScale( samp->light[k], g_direct_scale, samp->light[k] );
#ifdef HLRAD_DELUXEMAPPING
				VectorScale( samp->deluxe[k], g_direct_scale, samp->deluxe[k] );
#endif
			}
		}
	}
}

void FinalModelLightFace( int indexnum, int threadnum )
{
	int		modelnum = g_modellight_indexes[indexnum].modelnum;
	int		facenum = g_modellight_indexes[indexnum].facenum;
	entity_t		*mapent = g_modellight[modelnum];
	int		lightstyles;
	float		minlight;
	int		i, j, k;
	sample_t		*samp;
	tmesh_t		*mesh;
	facelight_t	*fl;
	lface_t		*f;
	vec3_t		lb;

	mesh = (tmesh_t *)mapent->cache;
	f = mesh->faces[facenum].light;
	if( !f ) return;
	fl = &f->facelight;

	for( lightstyles = 0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
	{
		if( f->styles[lightstyles] == 255 )
			break;
	}

	if( !lightstyles ) return;

	// wrote styles into mesh to determine completely black models
	for( i = 0; i < MAXLIGHTMAPS && f->styles[i] != 255; i++ )
	{
		for( k = 0; k < MAXLIGHTMAPS; k++ )
		{
			if( mesh->styles[k] == f->styles[i] || mesh->styles[k] == 255 )
				break;
		}

		// for our purpoces obviously overflow doesn't matter
		if( k == MAXLIGHTMAPS )
			continue;

		// allocate a new one
		if( mesh->styles[k] == 255 )
			mesh->styles[k] = f->styles[i];
	}

	minlight = FloatForKey( mapent, "_minlight" );
	if( minlight < 1.0 ) minlight *= 128.0f; // GoldSrc
	else minlight *= 0.5f; // Quake

	if( g_lightbalance )
		minlight *= g_direct_scale;
	if( g_numbounce > 0 ) minlight = 0.0f; // ignore for radiosity

	for( k = 0; k < lightstyles; k++ )
	{
		samp = fl->samples;

		for( j = 0; j < fl->numsamples; j++, samp++ )
		{
#ifdef HLRAD_DELUXEMAPPING
			vec3_t		directionnormals[3];
			vec3_t		texdirections[2];
			vec3_t		direction;
			int		side;
			vec3_t		v;

			VectorCopy( samp->normal, directionnormals[2] );

			for( side = 0; side < 2; side++ )
			{
				CrossProduct( f->normal, f->lmvecs[!side], texdirections[side] );
				VectorNormalize( texdirections[side] );
				if( DotProduct( texdirections[side], f->lmvecs[side]) < 0.0f )
					VectorNegate( texdirections[side], texdirections[side] );
			}

			for( side = 0; side < 2; side++ )
			{
				vec_t	dot = DotProduct( texdirections[side], samp->normal );
				VectorMA( texdirections[side], -dot, samp->normal, directionnormals[side] );
				VectorNormalize( directionnormals[side] );
			}
#endif
			VectorCopy( samp->light[k], lb ); 
#ifdef HLRAD_DELUXEMAPPING
			VectorCopy( samp->deluxe[k], direction );
			vec_t avg = VectorAvg( lb );
			VectorScale( direction, 1.0 / Q_max( 1.0, avg ), direction );
#endif
			// clip from the bottom first
			lb[0] = Q_max( lb[0], minlight );
			lb[1] = Q_max( lb[1], minlight );
			lb[2] = Q_max( lb[2], minlight );

			// clip from the top
			if( lb[0] > g_maxlight || lb[1] > g_maxlight || lb[2] > g_maxlight )
			{
				// find max value and scale the whole color down;
				float	max = VectorMax( lb );

				for( i = 0; i < 3; i++ )
					lb[i] = ( lb[i] * g_maxlight ) / max;
			}

			// do gamma adjust
			lb[0] = (float)pow( lb[0] / 256.0f, g_gamma ) * 256.0f;
			lb[1] = (float)pow( lb[1] / 256.0f, g_gamma ) * 256.0f;
			lb[2] = (float)pow( lb[2] / 256.0f, g_gamma ) * 256.0f;

#ifdef HLRAD_RIGHTROUND	// when you go down, when you go down down!
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 0] = Q_rint( lb[0] );
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 1] = Q_rint( lb[1] );
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 2] = Q_rint( lb[2] );
#else
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 0] = (byte)lb[0];
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 1] = (byte)lb[1];
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 2] = (byte)lb[2];
#endif

#ifdef HLRAD_DELUXEMAPPING
			if( g_deluxdatasize )
			{
				VectorScale( direction, 0.225, v ); // the scale is calculated such that length( v ) < 1

				if( DotProduct( v, v ) > ( 1.0 - NORMAL_EPSILON ))
					VectorNormalize( v );

				VectorNegate( v, v ); // let the direction point from face sample to light source

				for( int x = 0; x < 3; x++ )
				{
					lb[x] = DotProduct( v, directionnormals[x] ) * 127.0f + 128.0f;
					lb[x] = bound( 0, lb[x], 255.0 );
				}

				g_ddeluxdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 0] = (byte)lb[0];
				g_ddeluxdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 1] = (byte)lb[1];
				g_ddeluxdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 2] = (byte)lb[2];
			}
#ifdef HLRAD_SHADOWMAPPING
			if( g_shadowdatasize )
				g_dshadowdata[(f->lightofs / 3) + k * fl->numsamples + j] = (byte)samp->shadow[k] * 255;
#endif
#endif
		}
	}
}

void FreeModelFaceLights( void )
{
	facelight_t	*fl;
	lface_t		*f;
	tmesh_t		*mesh;

	for( int i = 0; i < g_modellight_numindexes; i++ )
	{
		int	modelnum = g_modellight_indexes[i].modelnum;
		int	facenum = g_modellight_indexes[i].facenum;
		entity_t	*mapent = g_modellight[modelnum];

         		mesh = (tmesh_t *)mapent->cache;
		f = mesh->faces[facenum].light;
		if( !f ) continue;
		fl = &f->facelight;
		Mem_Free( fl->samples );
	}
}

void ReduceModelLightmap( byte *oldlightdata, byte *olddeluxdata, byte *oldshadowdata )
{
	facelight_t	*fl;
	lface_t		*f;
	tmesh_t		*mesh;

	// first clearing old lightstyles
	for( int i = 0; i < g_modellight_modnum; i++ )
	{
		entity_t	*mapent = g_modellight[i];

         		mesh = (tmesh_t *)mapent->cache;
		mesh->styles[0] = 255;
		mesh->styles[1] = 255;
		mesh->styles[2] = 255;
		mesh->styles[3] = 255;
	}

	for (int index = 0; index < g_modellight_numindexes; index++)
	{
		int	modelnum = g_modellight_indexes[index].modelnum;
		int	facenum = g_modellight_indexes[index].facenum;
		entity_t *mapent = g_modellight[modelnum];

		mesh = (tmesh_t *)mapent->cache;
		f = mesh->faces[facenum].light;
		if (!f) 
			continue;
		fl = &f->facelight;

		if (f->lightofs == -1)
			continue;

		byte	oldstyles[MAXLIGHTMAPS];
		int	numstyles = 0;
		int	i, k, oldofs;

		oldofs = f->lightofs;
		f->lightofs = g_lightdatasize;

		for (k = 0; k < MAXLIGHTMAPS; k++)
		{
			oldstyles[k] = f->styles[k];
			f->styles[k] = 255;
		}

		for (k = 0; k < MAXLIGHTMAPS && oldstyles[k] != 255; k++)
		{
			int	count = fl->numsamples;
			byte	maxb = 0;

			for (i = 0; i < count; i++)
			{
				byte *v = &oldlightdata[oldofs + count * 3 * k + i * 3];
				maxb = Q_max(maxb, VectorMaximum(v));
			}

			if (maxb <= 0) // black
				continue;

			f->styles[numstyles] = oldstyles[k];
			memcpy(&g_dlightdata[f->lightofs + count * 3 * numstyles], &oldlightdata[oldofs + count * 3 * k], count * 3);
#ifdef HLRAD_DELUXEMAPPING
			if (g_ddeluxdata && olddeluxdata) {
				memcpy(&g_ddeluxdata[f->lightofs + count * 3 * numstyles], &olddeluxdata[oldofs + count * 3 * k], count * 3);
			}
#endif
#ifdef HLRAD_SHADOWMAPPING
			if (g_dshadowdata && oldshadowdata) {
				memcpy(&g_dshadowdata[(f->lightofs / 3) + count * numstyles], &oldshadowdata[(oldofs / 3) + count * k], count);
			}
#endif
			numstyles++;
		}
		g_lightdatasize += fl->numsamples * 3 * numstyles;
#ifdef HLRAD_DELUXEMAPPING
		if (g_ddeluxdata) {
			g_deluxdatasize += fl->numsamples * 3 * numstyles;
		}
#endif
#ifdef HLRAD_SHADOWMAPPING
		if (g_dshadowdata) {
			g_shadowdatasize += fl->numsamples * numstyles;
		}
#endif

		// wrote styles into mesh to determine completely black models
		for( i = 0; i < MAXLIGHTMAPS && f->styles[i] != 255; i++ )
		{
			for( k = 0; k < MAXLIGHTMAPS; k++ )
			{
				if( mesh->styles[k] == f->styles[i] || mesh->styles[k] == 255 )
					break;
			}

			// for our purpoces obviously overflow doesn't matter
			if( k == MAXLIGHTMAPS )
				continue;

			// allocate a new one
			if( mesh->styles[k] == 255 )
				mesh->styles[k] = f->styles[i];
		}
	}
}

static void GenerateLightCacheNumbers( void )
{
	char	string[32];
	tmesh_t	*mesh;
	int	i, j;

	for( i = 1; i < g_numentities; i++ )
	{
		entity_t	*mapent = &g_entities[i];

		// no cache - no lighting
		if( !mapent->cache ) continue;

		mesh = (tmesh_t *)mapent->cache;

		if( mapent->modtype != mod_alias && mapent->modtype != mod_studio )
			continue;

		if( !mesh->verts || mesh->numverts <= 0 )
			continue; 

		if( !mesh->faces || mesh->numfaces <= 0 )
			continue; 

		if( !FBitSet( mesh->flags, FMESH_MODEL_LIGHTMAPS ))
			continue;

		mesh->texture_step = TEXTURE_STEP;

		// check texturestep
		int texture_step = Q_max( 0, IntForKey( mapent, "zhlt_texturestep" ));

		// check bounds
		if( texture_step >= MIN_STUDIO_TEXTURE_STEP && texture_step <= MAX_STUDIO_TEXTURE_STEP )
			mesh->texture_step = texture_step;

		short lightid = g_modellight_modnum++;
		// at this point we have valid target for model lightmaps
		Q_snprintf( string, sizeof( string ), "%i", lightid + 1 );
		SetKeyValue( mapent, "flight_cache", string );
		g_modellight[lightid] = mapent;
	}

	g_modellight_numindexes = 0;
	g_studio_blur = g_blur;

	// generate remapping table for more effective CPU utilize
	for( i = 0; i < g_modellight_modnum; i++ )
	{
		entity_t	*mapent = g_modellight[i];
		mesh = (tmesh_t *)mapent->cache;
		g_modellight_numindexes += mesh->numfaces;
	}

	g_modellight_indexes = (faceremap_t *)Mem_Alloc( g_modellight_numindexes * sizeof( faceremap_t ));
	uint curIndex = 0;

	for( i = 0; i < g_modellight_modnum; i++ )
	{
		entity_t	*mapent = g_modellight[i];
		mesh = (tmesh_t *)mapent->cache;

		// encode model as lowpart and facenum as highpart
		for( j = 0; j < mesh->numfaces; j++ )
		{
			g_modellight_indexes[curIndex].modelnum = i;
			g_modellight_indexes[curIndex].facenum = j;
			curIndex++;
		}
		ASSERT( curIndex <= g_modellight_numindexes );
	}
}

static int ModelSize( tmesh_t *mesh )
{
	if( !mesh ) return 0;

	if( !mesh->faces || mesh->numfaces <= 0 )
		return 0; 

	int lightstyles;
	for( lightstyles = 0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
	{
		if( mesh->styles[lightstyles] == 255 )
			break;
	}

	// model is valid but completely not lighted by direct
	if( !lightstyles ) return 0;

	return sizeof( dmodelfacelight_t ) - ( sizeof( dfacelight_t ) * 3 ) + sizeof( dfacelight_t ) * mesh->numfaces + ((g_numworldlights + 7) / 8);
}

static int WriteModelLight( tmesh_t *mesh, byte *out )
{
	int		size = ModelSize( mesh );
	dmodelfacelight_t	*dml;

	if( !size ) return 0;

	dml = (dmodelfacelight_t *)out;
	out += sizeof( dmodelfacelight_t ) - ( sizeof( dfacelight_t ) * 3 ) + sizeof( dfacelight_t ) * mesh->numfaces;

	dml->modelCRC = mesh->modelCRC;
	dml->numfaces = mesh->numfaces;
	dml->texture_step = mesh->texture_step;

	memcpy( dml->styles, mesh->styles, sizeof( dml->styles ));
	memcpy( dml->submodels, mesh->fsubmodels, sizeof( dml->submodels ));
	memcpy( out, mesh->vislight, ((g_numworldlights + 7) / 8));
	VectorCopy( mesh->origin, dml->origin );
	VectorCopy( mesh->angles, dml->angles );
	VectorCopy( mesh->scale, dml->scale );

	// faces could be store now
	for( int i = 0; i < mesh->numfaces; i++ )
	{
		memcpy( dml->faces[i].styles, mesh->faces[i].light->styles, sizeof( dml->styles ));
		dml->faces[i].lightofs = mesh->faces[i].light->lightofs;
	}

	return size;
}

void WriteModelLighting( void )
{
	int		totaldatasize = ( sizeof( int ) * 3 ) + ( sizeof( int ) * g_modellight_modnum );
	int		i, len;
	byte		*data;
	dvlightlump_t	*l;

	for( i = 0; i < g_modellight_modnum; i++ )
	{
		entity_t	*mapent = g_modellight[i];

		// sanity check
		if( !mapent ) continue;

		len = ModelSize( (tmesh_t *)mapent->cache );
		totaldatasize += len;
	}

	Msg( "total modellight data: %s\n", Q_memprint( totaldatasize ));
	g_dflightdata = (byte *)Mem_Realloc( g_dflightdata, totaldatasize );

	// now setup to get the miptex data (or just the headers if using -wadtextures) from the wadfile
	l = (dvlightlump_t *)g_dflightdata;
	data = (byte *)&l->dataofs[g_modellight_modnum];

	l->ident = FLIGHTIDENT;
	l->version = FLIGHT_VERSION;
	l->nummodels = g_modellight_modnum;

	for( i = 0; i < g_modellight_modnum; i++ )
	{
		entity_t	*mapent = g_modellight[i];

		l->dataofs[i] = data - (byte *)l;
		len = WriteModelLight( (tmesh_t *)mapent->cache, data );
		if( !len ) l->dataofs[i] = -1; // completely black model
		data += len;
	}

	g_flightdatasize = data - g_dflightdata;

	if( totaldatasize != g_flightdatasize )
		COM_FatalError( "WriteModelLighting: memory corrupted\n" );

	// vertex cache acesss
	// const char *id = ValueForKey( ent, "flight_cache" );
	// int cacheID = atoi( id ) - 1;
	// if( cacheID < 0 || cacheID > num_map_entities ) return;	// bad cache num
	// if( l->dataofs[cacheID] == -1 ) return;		// cache missed
	// otherwise it's valid
}

void BuildModelLightmaps( void )
{
	GenerateLightCacheNumbers();

	// new code is very fast, so no reason to show progress
	RunThreadsOnIndividual( g_modellight_modnum, false, CalcTriangleVectors );

	if( !g_modellight_numindexes ) return;

	RunThreadsOnIndividual( g_modellight_numindexes, true, BuildModelLightmaps );
}

void FinalModelLightFace( void )
{
	if( !g_modellight_numindexes ) return;

	RunThreadsOnIndividual( g_modellight_numindexes, true, FinalModelLightFace );
}

#endif
