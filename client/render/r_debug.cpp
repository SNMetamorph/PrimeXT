/*
r_debug.cpp - visual debug routines
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include <stringlib.h>
#include "pm_movevars.h"
#include "mathlib.h"
#include "r_studio.h"
#include "r_sprite.h"
#include "r_particle.h"
#include "entity_types.h"
#include "r_weather.h"

struct TraceWork_t
{
	// input
	Vector		start_l;	// local trace start
	Vector		end_l;	// local trace end
	Vector		absmin;	// trace absmin
	Vector		absmax;	// trace absmax
	model_t		*model;

	// output
	msurface_t	*surface;
	float		fraction;
	int		hitbox;	// not a hitgroup!!!
	mplane_t		plane;	// hit plane
	Vector		endpos;
	bool		startsolid;
	bool		allsolid;
};

static TraceWork_t	tw;

/*
=================
R_TraceAgainstTriangle

Ray-triangle intersection as per
http://geometryalgorithms.com/Archive/algorithm_0105/algorithm_0105.htm
(original paper by Dan Sunday)
=================
*/
static void R_TraceAgainstTriangle( const Vector &a, const Vector &b, const Vector &c )
{
	float uu, uv, vv, wu, wv, s, t;
	const Vector p1 = tw.start_l;
	const Vector p2 = tw.end_l;
	const Vector p0 = a;
	Vector u, v, w, n, p;
	float d1, d2, d, frac;

	// calculate two mostly perpendicular edge directions
	u = b - p0;
	v = c - p0;

	// we have two edge directions, we can calculate the normal
	n = CrossProduct( v, u );
	if( n == g_vecZero ) return; // degenerate triangle

	p = p2 - p1;
	w = p1 - p0;

	d1 = -DotProduct( n, w );
	d2 = DotProduct( n, p );
	if( fabs( d2 ) < 0.0001 )
		return;

	// get intersect point of ray with triangle plane
	frac = d1 / d2;

	if( frac <= 0 ) return;
	if( frac >= tw.fraction )
		return; // we have hit something earlier

	// calculate the impact point
	VectorLerp( p1, frac, p2, p );

	// does p lie inside triangle?
	uu = DotProduct( u, u );
	uv = DotProduct( u, v );
	vv = DotProduct( v, v );

	w = p - p0;
	wu = DotProduct( w, u );
	wv = DotProduct( w, v );
	d = uv * uv - uu * vv;

	// get and test parametric coords
	s = (uv * wv - vv * wu) / d;
	if( s < 0.0f || s > 1.0f )
		return; // p is outside

	t = (uv * wu - uu * wv) / d;
	if( t < 0.0 || (s + t) > 1.0 )
		return; // p is outside

	tw.fraction = frac;
	tw.plane.normal = n;
	tw.endpos = p;
}

/*
=================
R_TraceAgainstSurface
=================
*/
static int R_TraceAgainstSurface( msurface_t *surf, mextrasurf_t *info )
{
	msurfmesh_t	*mesh;
	unsigned short	*elem;
	glvert_t		*verts;

	if( !info ) return 0;
	mesh = info->mesh;
	if( !mesh ) return 0;
	elem = mesh->elems;
	if( !elem ) return 0;
	verts = mesh->verts;		
	if( !verts ) return 0;

	float old_frac = tw.fraction;

	// clip each triangle individually
	for( int i = 0; i < mesh->numElems; i += 3, elem += 3 )
	{
		R_TraceAgainstTriangle( verts[elem[0]].vertex, verts[elem[1]].vertex, verts[elem[2]].vertex );

		if( old_frac > tw.fraction )
		{
			// flip normal is we are on the backside (does it really happen?)...
			if( DotProduct( tw.plane.normal, surf->plane->normal ) < 0 )
				tw.plane.normal = -tw.plane.normal;
			return 1;
		}
	}

	return 0;
}

/*
=================
R_TraceAgainstLeaf
=================
*/
static int R_TraceAgainstLeaf( mleaf_t *leaf )
{
	msurface_t *psurf, **mark;
	mextrasurf_t *info;

	if( leaf->contents == CONTENTS_SOLID )
	{
		tw.startsolid = true;
		return 1;	// hit a solid leaf
          }

	mark = leaf->firstmarksurface;

	for( int i = 0; i < leaf->nummarksurfaces; i++, mark++ )
	{
		psurf = *mark;
		info = SURF_INFO( psurf, tw.model );

		if( info->tracecount == tr.traceframecount )
			continue;	// do not test the same surface more than once
		info->tracecount = tr.traceframecount;

		if( info->mesh )
		{
			if( R_TraceAgainstSurface( psurf, info ))
				tw.surface = psurf;	// impact surface
		}
	}

	return 0;
}

/*
=================
R_RecursiveHullCheck

simplified version for traceline
=================
*/
static int R_RecursiveHullCheck( mnode_t *node, const Vector &p1, const Vector &p2 )
{
loc0:
	if( node->contents < 0 )
	{
		if( node->contents != CONTENTS_SOLID )
			tw.allsolid = false;

		return R_TraceAgainstLeaf( ( mleaf_t * )node );
	}

	mplane_t *plane = node->plane;
	float t1, t2;
	Vector mid;

	if( plane->type < 3 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
	}

	if( t1 >= -ON_EPSILON && t2 >= -ON_EPSILON )
	{
		node = node->children[0];
		goto loc0;
	}
	
	if( t1 < ON_EPSILON && t2 < ON_EPSILON )
	{
		node = node->children[1];
		goto loc0;
	}

	int side = t1 < 0;
	float frac = t1 / (t1 - t2);
	VectorLerp( p1, frac, p2, mid );

	int r = R_RecursiveHullCheck( node->children[side], p1, mid );
	if( r ) return r;

	return R_RecursiveHullCheck( node->children[!side], mid, p2 );
}

/*
=================
R_TraceAgainstBmodel
=================
*/
static void R_TraceAgainstBmodel( model_t *clmodel )
{
	// small bmodels can be traced as brute-force
	if( clmodel->nummodelsurfaces <= 16 )
	{
		msurface_t *psurf;
		mextrasurf_t *info;

		for( int i = 0; i < clmodel->nummodelsurfaces; i++ )
		{
			psurf = &clmodel->surfaces[clmodel->firstmodelsurface + i];
			info = SURF_INFO( psurf, tw.model );

			if( R_TraceAgainstSurface( psurf, info ))
				tw.surface = psurf;	// impact point
		}
	}
	else
	{
		// a little speed-up here (for big models of course)
		R_RecursiveHullCheck( clmodel->nodes + clmodel->hulls[0].firstclipnode, tw.start_l, tw.end_l );
	}
}

/*
=================
R_ClipToEntity
=================
*/
msurface_t *R_ClipToEntity( pmtrace_t *out, const Vector &start, const Vector &end, const cl_entity_t *ent, int flags )
{
	tr.traceframecount++;	// for multi-check avoidance

	tw.hitbox = -1;
	tw.endpos = end;
	tw.surface = NULL;
	tw.fraction = 1.0f;
	tw.model = ent->model;
	tw.startsolid = false;
	tw.allsolid = true;

	memset( &tw.plane, 0, sizeof( mplane_t ));

	// skip glass ents
	if( !tw.model || ( flags & FTRACE_IGNORE_GLASS ) && ent->curstate.rendermode != kRenderNormal )
	{
		out->fraction = tw.fraction;
		out->endpos = tw.endpos;

		return tw.surface;
	}

	if( tw.model->type == mod_brush )
	{
		bool rotated = (ent->curstate.angles != g_vecZero) ? true : false;
		matrix4x4	matrix;

		if( rotated )
		{
			matrix = matrix4x4( ent->origin, ent->angles );

			tw.start_l = matrix.VectorITransform( start );
			tw.end_l = matrix.VectorITransform( end );
		}
		else
		{
			tw.start_l = start - ent->origin;
			tw.end_l = end - ent->origin;
		}

		// world uses a recursive approach using BSP tree,
		// submodels just walk the list of surfaces linearly
		if( ent == GET_ENTITY( 0 ))
		{
			R_RecursiveHullCheck( tw.model->nodes, tw.start_l, tw.end_l );
		}
		else
		{
			R_TraceAgainstBmodel( tw.model );
		}

		if( tw.fraction != 1.0f )
		{
			// compute endpos (generic case)
			VectorLerp( start, tw.fraction, end, out->endpos );

			if( rotated )
			{
				// transform plane
				matrix.TransformPositivePlane( tw.plane, tw.plane );
				out->plane.normal = tw.plane.normal;
				out->plane.dist = tw.plane.dist;
			}
			else
			{
				out->plane.normal = tw.plane.normal;
				out->plane.dist = DotProduct( tw.endpos, tw.plane.normal );
			}
		}
	}
	else if( tw.model->type == mod_studio )
	{
		// TODO: write hitbox trace
	}

	out->hitgroup = tw.hitbox;	
	out->fraction = tw.fraction;
	out->allsolid = tw.allsolid;
	out->startsolid = tw.startsolid;

	if( tw.fraction < 1.0f )
		out->ent = ent->index;

	return tw.surface;
}

bool R_TraceBoundsIntersect( cl_entity_t *ent )
{
	Vector absmin, absmax;

	if( ent->model->type == mod_brush && ent->curstate.angles != g_vecZero )
	{
		float v, max = 0.0f;
		int i;

		// expand mins/maxs for rotation
		for( i = 0; i < 3; i++ )
		{
			v = fabs( ent->curstate.mins[i] );
			if( v > max ) max = v;
			v = fabs( ent->curstate.maxs[i] );
			if( v > max ) max = v;
		}

		for( i = 0; i < 3; i++ )
		{
			absmin[i] = ent->origin[i] - max;
			absmax[i] = ent->origin[i] + max;
		}
	}
	else
	{
		// get untransformed bbox
		absmin = ent->origin + ent->curstate.mins;
		absmax = ent->origin + ent->curstate.maxs;
	}

	// spread min\max by a pixel
	absmin -= Vector( 1, 1, 1 );
	absmax += Vector( 1, 1, 1 );

	return BoundsIntersect( tw.absmin, tw.absmax, absmin, absmax );
}

msurface_t *R_ClipToLinks( pmtrace_t *result, const Vector &start, const Vector &end, cl_entity_t **entities, int numEnts, int traceFlags )
{
	msurface_t *hitSurf = NULL;
	pmtrace_t	local;

	// trace against entities
	for( int i = 0; i < numEnts; i++ )
	{
		cl_entity_t *e = entities[i];

		if( !e || !e->model || ( e->model->type != mod_brush && e->model->type != mod_studio ))
			continue;

		// don't trace localclient
		if( RP_LOCALCLIENT( e ) && !RI.thirdPerson )
			continue;

		if( traceFlags & FTRACE_SIMPLEBOX && e->model->type == mod_studio )
			continue;

		if( !R_TraceBoundsIntersect( e ))
			continue;	// no intersection with ray

		msurface_t *newsurf = R_ClipToEntity( &local, start, end, e, traceFlags );

		if( local.fraction < result->fraction )
		{
			*result = local; // closer impact point
			hitSurf = newsurf;
		}
	}

	return hitSurf;
}

/*
=============
R_TraceLine
=============
*/
msurface_t *R_TraceLine( pmtrace_t *result, const Vector &start, const Vector &end, int traceFlags )
{
	msurface_t *surf, *surf2;

	// setup ray bbox
	ClearBounds( tw.absmin, tw.absmax );
	AddPointToBounds( start, tw.absmin, tw.absmax );
	AddPointToBounds( end, tw.absmin, tw.absmax );

	// fill in a default trace
	memset( result, 0, sizeof( pmtrace_t ));

	// trace against world
	surf = R_ClipToEntity( result, start, end, GET_ENTITY( 0 ), traceFlags );

	// trace static entities
	if(( surf2 = R_ClipToLinks( result, start, end, tr.static_entities, tr.num_static_entities, traceFlags )) != NULL )
	{
		surf = surf2;
	}

	// trace solid entities
	if(( surf2 = R_ClipToLinks( result, start, end, tr.solid_entities, tr.num_solid_entities, traceFlags )) != NULL )
	{
		surf = surf2;
	}

	// trace transparent entities
	if(( surf2 = R_ClipToLinks( result, start, end, tr.trans_entities, tr.num_trans_entities, traceFlags )) != NULL )
	{
		surf = surf2;
	}

	return surf;
}
