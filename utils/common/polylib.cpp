/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "cmdlib.h"
#include "mathlib.h"
#include "polylib.h"
#include <string.h>
#include "threads.h"
#include "bspfile.h"

void pw( winding_t *w )
{
	for( int i = 0; w && i < w->numpoints; i++ )
		Msg( "(%5.1f, %5.1f, %5.1f)\n", w->p[i][0], w->p[i][1], w->p[i][2] );
}

/*
=============
AllocWinding
=============
*/
winding_t	*AllocWinding( int numpoints )
{
	if( numpoints > MAX_POINTS_ON_WINDING )
		COM_FatalError( "AllocWinding: MAX_POINTS_ON_WINDING limit exceeded\n", numpoints );
	return (winding_t *)Mem_Alloc( (int)((winding_t *)0)->p[numpoints], C_WINDING );
}

/*
=============
FreeWinding
=============
*/
void FreeWinding( winding_t *w )
{
	// simple sentinel by Carmack
	if( *(unsigned *)w == 0xDEADC0DE )
		COM_FatalError( "FreeWinding: freed a freed winding\n" );

	*(unsigned *)w = 0xDEADC0DE;
	Mem_Free( w, C_WINDING );
}

/*
==================
CopyWinding
==================
*/
winding_t	*CopyWinding( const winding_t *w )
{
	winding_t	*c = AllocWinding( w->numpoints );
	memcpy( c, w, (int)((winding_t *)0)->p[w->numpoints] );

	return c;
}

/*
==================
CopyWindingExt
==================
*/
winding_t *CopyWinding( int numpoints, vec3_t *points )
{
	winding_t	*c = AllocWinding( numpoints );
	memcpy( c->p, points, sizeof( *points ) * numpoints );
	c->numpoints = numpoints;

	return c;
}

/*
==================
ReverseWinding
==================
*/
void ReverseWinding( winding_t **inout )
{
	winding_t	*c, *w;

	if( !inout || !*inout )
		return;

	w = *inout;
	c = AllocWinding( w->numpoints );

	for( int i = 0; i < w->numpoints; i++ )
		VectorCopy( w->p[w->numpoints-1-i], c->p[i] );
	c->numpoints = w->numpoints;

	FreeWinding( w );
	*inout = c;
}

/*
============
RemoveColinearPoints
============
*/
void RemoveColinearPointsEpsilon( winding_t *w, vec_t epsilon )
{
	vec3_t	v1, v2;
	vec_t	*p1, *p2, *p3;

	for( int i = 0; w && i < w->numpoints; i++ )
	{
		p1 = w->p[(i+w->numpoints-1) % w->numpoints];
		p2 = w->p[i];
		p3 = w->p[(i+1)%w->numpoints];
		VectorSubtract( p2, p1, v1 );
		VectorSubtract( p3, p2, v2 );

		// v1 or v2 might be close to 0
		if( DotProduct( v1, v2 ) * DotProduct( v1, v2 ) >= DotProduct( v1, v1 ) * DotProduct( v2, v2 ) 
		- epsilon * epsilon * ( DotProduct( v1, v1 ) + DotProduct( v2, v2 ) + epsilon * epsilon ))
		{
			w->numpoints--;

			for( ; i < w->numpoints; i++ )
				VectorCopy( w->p[i+1], w->p[i] );
			i = -1;
			continue;
		}
	}
}

/*
============
WindingPlane
============
*/
void WindingPlane( winding_t *w, vec3_t normal, vec_t *dist )
{
	vec3_t	v1, v2;

	if( w && w->numpoints >= 3 )
	{
		VectorSubtract( w->p[2], w->p[1], v1 );
		VectorSubtract( w->p[0], w->p[1], v2 );
		CrossProduct( v2, v1, normal );
		VectorNormalize( normal );
		*dist = DotProduct( w->p[0], normal );
	}
	else
	{
		VectorClear( normal );
		*dist = 0.0;
	}
}

/*
================
WindingIsTiny

Returns true if the winding would be crunched out of
existance by the vertex snapping.
================
*/
bool WindingIsTiny( winding_t *w )
{
	vec3_t	delta;
	vec_t	length;
	int	edges = 0;

	for( int i = 0; w != NULL && i < w->numpoints; i++ )
	{
		int j = ( i == ( w->numpoints - 1 )) ? 0 : ( i + 1 );
		VectorSubtract( w->p[j], w->p[i], delta );
		length = VectorLength( delta );

		if( length > EDGE_LENGTH )
		{
			if( ++edges == 3 )
				return false;
		}
	}

	return true;
}

/*
================
WindingIsHuge

Returns true if the winding still has one of the points
from basewinding for plane
================
*/
bool WindingIsHuge( winding_t *w )
{
	for( int i = 0; w != NULL && i < w->numpoints; i++ )
	{
		for( int j = 0; j < 3; j++ )
		{
			if( w->p[i][j] < WORLD_MINS || w->p[i][j] > WORLD_MAXS )
				return true;
		}
	}

	return false;
}

/*
=============
WindingArea
=============
*/
vec_t WindingArea( const winding_t *w )
{
	vec3_t	d1, d2, cross;
	vec_t	total;

	total = 0.0;

	for( int i = 2; w != NULL && w->numpoints >= 3 && i < w->numpoints; i++ )
	{
		VectorSubtract( w->p[i-1], w->p[0], d1 );
		VectorSubtract( w->p[i], w->p[0], d2 );
		CrossProduct( d1, d2, cross );
		total += 0.5 * VectorLength ( cross );
	}

	return total;
}

/*
=============
WindingBounds
=============
*/
void WindingBounds( winding_t *w, vec3_t mins, vec3_t maxs, bool merge )
{
	if( !merge ) ClearBounds( mins, maxs );

	for( int i = 0; w != NULL && i < w->numpoints; i++ )
	{
		for( int j = 0; j < 3; j++ )
		{
			vec_t v = w->p[i][j];
			if( v < mins[j] ) mins[j] = v;
			if( v > maxs[j] ) maxs[j] = v;
		}
	}
}

/*
=============
WindingCenter
=============
*/
void WindingCenter( winding_t *w, vec3_t center )
{
	VectorClear( center );

	if( !w || w->numpoints <= 0 )
		return;

	for( int i = 0; i < w->numpoints; i++ )
		VectorAdd( w->p[i], center, center );

	vec_t scale = 1.0 / w->numpoints;
	VectorScale( center, scale, center );
}

/*
=============
WindingAreaAndBalancePoint
=============
*/
vec_t WindingAreaAndBalancePoint( winding_t *w, vec3_t center )
{
	vec3_t	d1, d2, cross;
	vec_t	total, area;

	VectorClear( center );
	if( !w ) return 0;

	total = 0.0;

	for( int i = 2; i < w->numpoints; i++ )
	{
		VectorSubtract( w->p[i-1], w->p[0], d1 );
		VectorSubtract( w->p[i], w->p[0], d2 );
		CrossProduct( d1, d2, cross );
		area = VectorLength( cross );
		total += area;

		// center of triangle, weighted by area
		VectorMA( center, area / 3.0, w->p[i-1], center );
		VectorMA( center, area / 3.0, w->p[i], center );
		VectorMA( center, area / 3.0, w->p[0], center );
	}

	if( total ) VectorScale( center, 1.0 / total, center );

	return total * 0.5;
}

/*
=================
BaseWindingForPlane
=================
*/
winding_t *BaseWindingForPlane( const vec3_t normal, vec_t dist )
{
	vec3_t	org, vright, vup;
	vec_t	v, max = 0.56;
	int	x = -1;
	winding_t	*w;

	// find the major axis
	for( int i = 0; i < 3; i++ )
	{
		v = fabs( normal[i] );

		if( v > max )
		{
			max = v;
			x = i;
		}
	}

	if( x == -1 ) COM_FatalError( "BaseWindingForPlane: no axis found\n" );

	switch( x )
	{
	case 0:
	case 1:	// fall through to next case.
		vright[0] = -normal[1];
		vright[1] = normal[0];
		vright[2] = 0;
		break;
	case 2:
		vright[0] = 0;
		vright[1] = -normal[2];
		vright[2] = normal[1];
		break;
	}

	VectorScale( vright, (vec_t)WORLD_MAXS * 4.0, vright );
	CrossProduct( normal, vright, vup );
	VectorScale( normal, (vec_t)dist, org );

	// project a really big axis aligned box onto the plane
	w = AllocWinding( 4 );
	
	VectorSubtract( org, vright, w->p[0] );
	VectorAdd( w->p[0], vup, w->p[0] );
	
	VectorAdd( org, vright, w->p[1] );
	VectorAdd( w->p[1], vup, w->p[1] );
	
	VectorAdd( org, vright, w->p[2] );
	VectorSubtract( w->p[2], vup, w->p[2] );
	
	VectorSubtract( org, vright, w->p[3] );
	VectorSubtract( w->p[3], vup, w->p[3] );

	w->numpoints = 4;
	
	return w;	
}

/*
=============
ClipWindingEpsilon
=============
*/
void ClipWindingEpsilon( winding_t *in, const vec3_t normal, vec_t dist, vec_t epsilon, winding_t **front, winding_t **back, bool keepon )
{
	vec_t	dists[MAX_POINTS_ON_WINDING+4];
	int	sides[MAX_POINTS_ON_WINDING+4];
	int	counts[3];
	vec_t	dot;
	int	i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*f, *b;
	int	maxpts;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for( i = 0; i < in->numpoints; i++ )
	{
		dot = DotProduct( in->p[i], normal );
		dot -= dist;
		dists[i] = dot;

		if( dot > epsilon )
			sides[i] = SIDE_FRONT;
		else if( dot < -epsilon )
			sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;

		counts[sides[i]]++;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];

	*front = *back = NULL;

	if( keepon && !counts[SIDE_FRONT] && !counts[SIDE_BACK] )
	{
		vec_t sum = 0.0;
		for( i = 0; i < in->numpoints; i++)
		{
			dot = DotProduct( in->p[i], normal );
			dot -= dist;
			sum += dot;
		}

		if( sum > NORMAL_EPSILON )
			*front = CopyWinding( in );
		else *back = CopyWinding( in );
		return;
	}

	if( !counts[SIDE_FRONT] )
	{
		*back = CopyWinding( in );
		return;
	}

	if( !counts[SIDE_BACK] )
	{
		*front = CopyWinding( in );
		return;
	}

	maxpts = in->numpoints + 4;	// cant use counts[0] + 2 because of fp grouping errors

	*front = f = AllocWinding( maxpts );
	*back = b = AllocWinding( maxpts );
	VectorClear( mid );

	for( i = 0; i < in->numpoints; i++ )
	{
		p1 = in->p[i];

		if( sides[i] == SIDE_ON )
		{
			VectorCopy( p1, f->p[f->numpoints] );
			VectorCopy( p1, b->p[b->numpoints] );
			f->numpoints++;
			b->numpoints++;
			continue;
		}
		else if( sides[i] == SIDE_FRONT )
		{
			VectorCopy( p1, f->p[f->numpoints] );
			f->numpoints++;
		}
		else if( sides[i] == SIDE_BACK )
		{
			VectorCopy( p1, b->p[b->numpoints] );
			b->numpoints++;
		}

		if(( sides[i+1] == SIDE_ON ) | ( sides[i+1] == sides[i] ))  // | instead of || for branch optimization
			continue;

		// generate a split point
		p2 = in->p[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i+1]);

		for( j = 0; j < 3; j++ )
		{
			// avoid round off error when possible
			if( normal[j] == 1 )
				mid[j] = dist;
			else if( normal[j] == -1 )
				mid[j] = -dist;
			else mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}

		VectorCopy( mid, f->p[f->numpoints] );
		VectorCopy( mid, b->p[b->numpoints] );
		f->numpoints++;
		b->numpoints++;
	}

	if(( f->numpoints > maxpts ) | ( b->numpoints > maxpts ))
		COM_FatalError( "ClipWinding: points exceeded estimate\n" );

	if(( f->numpoints > MAX_POINTS_ON_WINDING ) | ( b->numpoints > MAX_POINTS_ON_WINDING ))
		COM_FatalError( "ClipWinding: MAX_POINTS_ON_WINDING limit exceeded\n" );

	RemoveColinearPointsEpsilon( f, epsilon );
	RemoveColinearPointsEpsilon( b, epsilon );

	if( f->numpoints < 3 )
	{
		FreeWinding( f );
		*front = NULL;
	}

	if( b->numpoints < 3 )
	{
		FreeWinding( b );
		*back = NULL;
	}
}

/*
==================
DivideWindingEpsilon

Divides a winding by a plane, producing one or two windings. The
original winding is not damaged or freed. If only on one side, the
returned winding will be the input winding. If on both sides, two
new windings will be created.
==================
*/
void DivideWindingEpsilon( winding_t *in, vec3_t normal, vec_t dist, vec_t epsilon, winding_t **front, winding_t **back, vec_t *fnormal, bool keepon )
{
	vec_t	dists[MAX_POINTS_ON_WINDING+4];
	int	sides[MAX_POINTS_ON_WINDING+4];
	int	counts[3];
	vec_t	dot;
	int	i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*f, *b;
	int	maxpts;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for( i = 0; i < in->numpoints; i++ )
	{
		dot = DotProduct( in->p[i], normal );
		dot -= dist;
		dists[i] = dot;

		if( dot > epsilon )
			sides[i] = SIDE_FRONT;
		else if( dot < -epsilon )
			sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;

		counts[sides[i]]++;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];

	*front = *back = NULL;

	if( keepon && !counts[SIDE_FRONT] && !counts[SIDE_BACK] )
	{
		if( fnormal )
		{
			// put front face in front node, and back face in back node.
			if( DotProduct( fnormal, normal ) > NORMAL_EPSILON ) // usually near 1.0 or -1.0
				*front = in;
			else *back = in;
		}
		else
		{
			vec_t sum = 0.0;
			for( i = 0; i < in->numpoints; i++)
			{
				dot = DotProduct( in->p[i], normal );
				dot -= dist;
				sum += dot;
			}

			if( sum > NORMAL_EPSILON )
				*front = in;
			else *back = in;
		}
		return;
	}

	if( !counts[SIDE_FRONT] )
	{
		*back = in;
		return;
	}

	if( !counts[SIDE_BACK] )
	{
		*front = in;
		return;
	}

	maxpts = in->numpoints + 4;	// can't use counts[0]+2 because of fp grouping errors

	*front = f = AllocWinding( maxpts );
	*back = b = AllocWinding( maxpts );

	for( i = 0; i < in->numpoints; i++ )
	{
		p1 = in->p[i];

		if( sides[i] == SIDE_ON )
		{
			VectorCopy( p1, f->p[f->numpoints] );
			VectorCopy( p1, b->p[b->numpoints] );
			f->numpoints++;
			b->numpoints++;
			continue;
		}

		if( sides[i] == SIDE_FRONT )
		{
			VectorCopy( p1, f->p[f->numpoints] );
			f->numpoints++;
		}
		else if( sides[i] == SIDE_BACK )
		{
			VectorCopy( p1, b->p[b->numpoints] );
			b->numpoints++;
		}

		if(( sides[i+1] == SIDE_ON ) | ( sides[i+1] == sides[i] ))  // | instead of || for branch optimization
			continue;

		// generate a split point
		p2 = in->p[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i+1]);

		for( j = 0; j < 3; j++ )
		{
			// avoid round off error when possible
			if( normal[j] == 1 )
				mid[j] = dist;
			else if( normal[j] == -1 )
				mid[j] = -dist;
			else mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}

		VectorCopy( mid, f->p[f->numpoints] );
		VectorCopy( mid, b->p[b->numpoints] );
		f->numpoints++;
		b->numpoints++;
	}

	if(( f->numpoints > maxpts ) | ( b->numpoints > maxpts ))
		COM_FatalError( "ClipWinding: points exceeded estimate\n" );

	if(( f->numpoints > MAX_POINTS_ON_WINDING ) | ( b->numpoints > MAX_POINTS_ON_WINDING ))
		COM_FatalError( "ClipWinding: MAX_POINTS_ON_WINDING limit exceeded\n" );

	RemoveColinearPointsEpsilon( f, epsilon );
	RemoveColinearPointsEpsilon( b, epsilon );

	if( f->numpoints < 3 )
	{
		FreeWinding( f );
		FreeWinding( b );
		*front = NULL;
		*back = in;
	}
	else if( b->numpoints < 3 )
	{
		FreeWinding( f );
		FreeWinding( b );
		*front = in;
		*back = NULL;
	}
}

/*
=============
ChopWindingInPlace
=============
*/
bool ChopWindingInPlace( winding_t **inout, const vec3_t normal, vec_t dist, vec_t epsilon, bool keepon )
{
	winding_t		*in;
	vec_t		dists[MAX_POINTS_ON_WINDING+4];
	int		sides[MAX_POINTS_ON_WINDING+4];
	int		counts[3];
	vec_t		dot;
	int		i, j;
	vec_t		*p1, *p2;
	vec3_t		mid;
	winding_t		*f;
	int		maxpts;

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for( i = 0; i < in->numpoints; i++ )
	{
		dot = DotProduct( in->p[i], normal );
		dot -= dist;
		dists[i] = dot;

		if( dot > epsilon )
			sides[i] = SIDE_FRONT;
		else if( dot < -epsilon )
			sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;

		counts[sides[i]]++;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];

	if( keepon && !counts[SIDE_FRONT] && !counts[SIDE_BACK] )
		return true; // SIDE_ON

	if( !counts[0] )
	{
		FreeWinding( in );
		*inout = NULL;
		return false;
	}

	if( !counts[1] ) return true;	// inout stays the same

	maxpts = in->numpoints + 4;	// cant use counts[0] + 2 because of fp grouping errors
	VectorClear( mid );

	f = AllocWinding( maxpts );

	for( i = 0; i < in->numpoints; i++ )
	{
		p1 = in->p[i];

		if( sides[i] == SIDE_ON )
		{
			VectorCopy( p1, f->p[f->numpoints] );
			f->numpoints++;
			continue;
		}

		if( sides[i] == SIDE_FRONT )
		{
			VectorCopy( p1, f->p[f->numpoints] );
			f->numpoints++;
		}

		if(( sides[i+1] == SIDE_ON ) | ( sides[i+1] == sides[i] ))  // | instead of || for branch optimization
			continue;

		// generate a split point
		p2 = in->p[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i+1]);

		for( j = 0; j < 3; j++ )
		{	
			// avoid round off error when possible
			if( normal[j] == 1 )
				mid[j] = dist;
			else if( normal[j] == -1 )
				mid[j] = -dist;
			else mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}

		VectorCopy( mid, f->p[f->numpoints] );
		f->numpoints++;
	}

	if( f->numpoints > maxpts )
		COM_FatalError( "ChopWinding: points exceeded estimate\n" );
	if( f->numpoints > MAX_POINTS_ON_WINDING )
		COM_FatalError( "ChopWinding: MAX_POINTS_ON_WINDING limit exceeded\n" );

	RemoveColinearPointsEpsilon( f, epsilon );

	if( f->numpoints < 3 )
	{
		FreeWinding( f );
		*inout = NULL;
		return false;
	}

	FreeWinding( in );
	*inout = f;
	return true;
}

/*
=============
TryMergeWinding

If two polygons share a common edge and the edges that meet at the
common points are both inside the other polygons, merge them

Returns NULL if the windings couldn't be merged, or the new winding.
The originals will NOT be freed.
=============
*/
winding_t *TryMergeWindingEpsilon( winding_t *w1, winding_t *w2, const vec3_t planenormal, vec_t epsilon )
{
	vec_t	*front, *back;
	bool	keep1, keep2;
	vec3_t	normal, delta;
	int	edge1, edge2;
	winding_t	*neww;
	vec_t	dot;
	int	k;

	// check for early out
	if( !w1 || !w2 ) return NULL;

	edge1 = edge2 = -1;

	//
	// find a common edge
	//
	for( int i = 0; i < w1->numpoints; i++ )
	{
		vec_t	*p1 = w1->p[i];
		vec_t	*p2 = w1->p[(i + 1) % w1->numpoints];

		for( int j = 0; j < w2->numpoints; j++ )
		{
			vec_t	*p3 = w2->p[j];
			vec_t	*p4 = w2->p[(j + 1) % w2->numpoints];

			if( VectorCompareEpsilon( p1, p4, epsilon ) && VectorCompareEpsilon( p2, p3, epsilon ))
			{
				edge2 = j;
				break;
			}
		}

		if( edge2 >= 0 )
		{
			edge1 = i;
			break;
		}
	}

	if( edge1 < 0 || edge2 < 0 )
		return false; // no collinear edge

	//
	// check slope of connected lines
	// if the slopes are colinear, the point can be removed
	//
	front = w1->p[edge1];
	back = w1->p[(i - 1 + w1->numpoints) % w1->numpoints];
	VectorSubtract( front, back, delta );
	CrossProduct( planenormal, delta, normal );
	VectorNormalize( normal );

	back = w2->p[(edge2 + 2) % w2->numpoints];
	VectorSubtract( back, front, delta );
	dot = DotProduct( delta, normal );
	if( dot > epsilon ) return NULL; // not a convex polygon
	keep1 = dot < -epsilon;

	front = w2->p[edge2];
	back = w1->p[(edge1 + 2) % w1->numpoints];
	VectorSubtract( back, front, delta );
	CrossProduct( planenormal, delta, normal );
	VectorNormalize( normal );

	back = w2->p[(edge2 - 1 + w2->numpoints) % w2->numpoints];
	VectorSubtract( back, front, delta );
	dot = DotProduct( delta, normal );
	if( dot > epsilon ) return NULL; // not a convex polygon
	keep2 = dot < -epsilon;

	//
	// build the new polygon
	//
	neww = AllocWinding( w1->numpoints + w2->numpoints - 4 + keep1 + keep2 );

	// copy first polygon
	for( k = (edge1 + 1) % w1->numpoints; k != edge1; k = (k + 1) % w1->numpoints )
	{
		if( k == (edge1 + 1) % w1->numpoints && !keep2 )
			continue;

		VectorCopy( w1->p[k], neww->p[neww->numpoints] );
		neww->numpoints++;
	}

	// copy second polygon
	for( k = (edge2 + 1) % w2->numpoints; k != edge2; k = (k + 1) % w2->numpoints )
	{
		if( k == (edge2 + 1) % w2->numpoints && !keep1 )
			continue;

		VectorCopy( w2->p[k], neww->p[neww->numpoints] );
		neww->numpoints++;
	}

	RemoveColinearPointsEpsilon( neww, epsilon );

	if( neww->numpoints < 3 )
	{
		FreeWinding( neww );
		return NULL;
	}

	return neww;
}

/*
=============
TryMergeWinding

If two polygons share a common edge and the edges that meet at the
common points are both inside the other polygons, merge them

Returns NULL if the windings couldn't be merged, or the new winding.
The originals will NOT be freed.
=============
*/
bool BiteWindingEpsilon( winding_t **in1, winding_t **in2, const vec3_t planenormal, vec_t epsilon )
{
	vec_t	*front, *back;
	winding_t	*neww, *fw, *bw;
	bool	convex1, convex2;
	vec3_t	normal, delta;
	int	edge1, edge2;
	int	edgetype = 0;
	vec_t	len1, len2;
	winding_t	*w1 = *in1;
	winding_t	*w2 = *in2;
	vec3_t	dir1, dir2;
	vec_t	dot, dist;

	if( !w1 || !w2 )
		return false;

	edge1 = edge2 = -1;

	// p11 = p1
	// p12 = p2
	// p21 = p3
	// p22 = p4

	//
	// find a common edge
	//
	for( int i = 0; i < w1->numpoints; i++ )
	{
		vec_t	*p1 = w1->p[i];
		vec_t	*p2 = w1->p[(i + 1) % w1->numpoints];

		for( int j = 0; j < w2->numpoints; j++ )
		{
			vec_t	*p3 = w2->p[j];
			vec_t	*p4 = w2->p[(j + 1) % w2->numpoints];

			if( VectorCompareEpsilon( p1, p4, epsilon ) && VectorCompareEpsilon( p2, p3, epsilon ))
			{
				// really common edge
				edgetype = 0;
				edge2 = j;
				break;
			}

			VectorSubtract( p1, p2, dir1 );
			VectorSubtract( p4, p3, dir2 );

			len1 = VectorNormalize( dir1 );
			len2 = VectorNormalize( dir2 );
			dot = DotProduct( dir1, dir2 );

			if( VectorCompareEpsilon( p1, p4, epsilon ) && ( dot > ( 1.0 - NORMAL_EPSILON )) && ( len1 < len2 ))
			{
				// collinear edge with common point 1
				edgetype = 1;
				edge2 = j;
				break;
			}

			if( VectorCompareEpsilon( p2, p3, epsilon ) && ( dot > ( 1.0 - NORMAL_EPSILON )) && ( len1 < len2 ))
			{
				// collinear edge with common point 2
				edgetype = 2;
				edge2 = j;
				break;
			}
		}

		if( edge2 >= 0 )
		{
			edge1 = i;
			break;
		}
	}

	if( edge1 < 0 || edge2 < 0 )
		return false; // no collinear edge

	if( edgetype == 0 )
	{
		// Detemine collinear edge, and split at non-collinear
		// If both edges are non-collinear, don't split at all!
		front = w1->p[edge1];
		back = w1->p[(edge1 + w1->numpoints - 1) % w1->numpoints];
		VectorSubtract( front, back, delta );
		CrossProduct( planenormal, delta, normal );
		VectorNormalize( normal );

		back = w2->p[(edge2 + 2) % w2->numpoints];
		VectorSubtract( back, front, delta );
		dot = DotProduct( delta, normal );
		convex1 = dot <= epsilon;

		front = w2->p[edge2];
		back = w1->p[(edge1 + 2) % w1->numpoints];
		VectorSubtract( back, front, delta );
		CrossProduct( planenormal, delta, normal );
		VectorNormalize( normal );

		back = w2->p[(edge2 + w2->numpoints - 1) % w2->numpoints];
		VectorSubtract( back, front, delta );
		dot = DotProduct( delta, normal );
		convex2 = dot <= epsilon;

		if( convex1 && convex2 )
			return false;

		front = w1->p[edge1];
		back = w1->p[(edge1 + w1->numpoints - 1) % w1->numpoints];
		VectorSubtract( front, back, delta );
		CrossProduct( planenormal, delta, normal );
		VectorNormalize( normal );
		dist = DotProduct( normal, front );

		ClipWindingEpsilon( w2, normal, dist, epsilon, &fw, &bw );

		if( !fw || !bw )
		{
			if( fw ) FreeWinding( fw );
			if( bw ) FreeWinding( bw );

			front = w1->p[(edge1 + 2) % w1->numpoints];
			back = w1->p[(edge1 + 1) % w1->numpoints];
			VectorSubtract( front, back, delta );
			CrossProduct( planenormal, delta, normal );
			VectorNormalize( normal );
			dist = DotProduct( normal, front );

			ClipWindingEpsilon( w2, normal, dist, epsilon, &fw, &bw );

			if( !fw || !bw )
			{
				if( fw ) FreeWinding( fw );
				if( bw ) FreeWinding( bw );
				return false;
			}
		}

		neww = TryMergeWindingEpsilon( w1, bw, planenormal, epsilon );

		if( !neww )
		{
			if( fw ) FreeWinding( fw );
			if( bw ) FreeWinding( bw );
			return false;
		}

		// throw old windings
		FreeWinding( bw );
		FreeWinding( w1 );
		FreeWinding( w2 );

		*in1 = neww;
		*in2 = fw;
	}
	else if( edgetype == 1 )
	{
		// common point 1
		front = w1->p[(edge1 + 2) % w1->numpoints];
		back = w1->p[(edge1 + 1) % w1->numpoints];
		VectorSubtract( front, back, delta );
		CrossProduct( planenormal, delta, normal );
		VectorNormalize( normal );
		dist = DotProduct( normal, front );

		ClipWindingEpsilon( w2, normal, dist, epsilon, &fw, &bw );

		if ( !fw || !bw )
		{
			if( fw ) FreeWinding( fw );
			if( bw ) FreeWinding( bw );
			return false;
		}

		neww = TryMergeWindingEpsilon( w1, bw, planenormal, epsilon );

		if( !neww )
		{
			if( fw ) FreeWinding( fw );
			if( bw ) FreeWinding( bw );
			return false;
		}

		// throw old windings
		FreeWinding( bw );
		FreeWinding( w1 );
		FreeWinding( w2 );

		*in1 = neww;
		*in2 = fw;
	}
	else if( edgetype == 2 )
	{
		// common point 2
		front = w1->p[edge1];
		back = w1->p[(edge1 + w1->numpoints - 1) % w1->numpoints];
		VectorSubtract( front, back, delta );
		CrossProduct( planenormal, delta, normal );
		VectorNormalize( normal );
		dist = DotProduct( normal, front );

		ClipWindingEpsilon( w2, normal, dist, epsilon, &fw, &bw );

		if ( !fw || !bw )
		{
			if( fw ) FreeWinding( fw );
			if( bw ) FreeWinding( bw );
			return false;
		}

		neww = TryMergeWindingEpsilon( w1, bw, planenormal, epsilon );

		if( !neww )
		{
			if( fw ) FreeWinding( fw );
			if( bw ) FreeWinding( bw );
			return false;
		}

		// throw old windings
		FreeWinding( bw );
		FreeWinding( w1 );
		FreeWinding( w2 );

		*in1 = neww;
		*in2 = fw;
	}

	return true;
}

/*
=================
CheckWinding

debugging thing
=================
*/
void CheckWindingEpsilon( winding_t *w, vec_t epsilon, bool show_warnings )
{
	vec_t	*p1, *p2;
	vec_t	d, edgedist;
	vec3_t	dir, edgenormal;
	vec_t	area, facedist;
	vec3_t	facenormal;
	int	i, j;

	if( !w ) return;

	if( w->numpoints < 3 )
		COM_FatalError( "CheckWinding: %i points\n", w->numpoints );

	area = WindingArea( w );
	if( area < MICROVOLUME ) COM_FatalError( "CheckWinding: %f area\n", area );

	WindingPlane( w, facenormal, &facedist );

	for( i = 0; i < w->numpoints; i++ )
	{
		p1 = w->p[i];

		for( j = 0; j < 3; j++ )
		{
			if( p1[j] > WORLD_MAXS || p1[j] < WORLD_MINS )
				COM_FatalError( "CheckWinding: BOGUS_RANGE: %f", p1[j] );
		}

		j = i + 1 == w->numpoints ? 0 : i + 1;

		// check the point is on the face plane
		d = DotProduct( p1, facenormal ) - facedist;
		if( show_warnings && ( d < -epsilon || d > epsilon ))
			MsgDev( D_WARN, "CheckWinding: point off plane: %f\n", d );

		// check the edge isn't degenerate
		p2 = w->p[j];
		VectorSubtract( p2, p1, dir );
		d = VectorLength( dir );

		if( show_warnings && ( d < epsilon ))
			MsgDev( D_WARN, "CheckWinding: degenerate edge: %f\n", d );

		CrossProduct( facenormal, dir, edgenormal );
		VectorNormalize( edgenormal );
		edgedist = DotProduct( p1, edgenormal );
		edgedist += epsilon;

		// all other points must be on front side
		for( j = 0; j < w->numpoints; j++ )
		{
			if( j == i ) continue;

			d = DotProduct( w->p[j], edgenormal );
			if( d > edgedist ) COM_FatalError( "CheckWinding: non-convex\n" );
		}
	}
}

/*
============
PointInWindingEpsilon
============
*/
bool PointInWindingEpsilon( const winding_t *w, const vec3_t planenormal, const vec3_t point, vec_t epsilon, bool noedge )
{
	vec3_t	delta;
	vec3_t	normal;
	vec_t	dist;

	ASSERT( w != NULL );
#if 0
	vec3_t	edge, cross0, cross1;

	//
	// get the first normal to test
	//
	VectorSubtract( point, w->p[0], delta );
	VectorSubtract( w->p[1], w->p[0], edge );
	CrossProduct( edge, delta, cross0 );
	VectorNormalize( cross0 );

	for( int i = 1; i < w->numpoints; i++ )
	{
		VectorSubtract( point, w->p[i], delta );
		VectorSubtract( w->p[(i+1)%w->numpoints], w->p[i], edge );
		CrossProduct( edge, delta, cross1 );
		VectorNormalize( cross1 );

		if( DotProduct( cross0, cross1 ) < 0.0f )
			return false;
	}
#else
	for( int i = 0; i < w->numpoints; i++ )
	{
		VectorSubtract( w->p[(i+1) % w->numpoints], w->p[i], delta );
		CrossProduct( delta, planenormal, normal );
		dist = DotProduct( point, normal ) - DotProduct( w->p[i], normal );

		if( noedge )
		{
			if( dist < 0.0 && ( epsilon == 0.0 || dist * dist <= epsilon * epsilon * DotProduct( normal, normal )))
				return false;
		}
		else
		{
			if(dist < 0.0 && ( epsilon == 0.0 || dist * dist > epsilon * epsilon * DotProduct( normal, normal )))
				return false;
		}
	}
#endif
	return true;
}

/*
============
WindingSnapPoint
============
*/
void WindingSnapPoint( const winding_t *w, const vec3_t planenormal, vec3_t point )
{
	int		x;
	const vec_t	*p1, *p2;
	vec3_t		delta;
	vec3_t		normal;
	vec_t		dist;
	vec_t		dot0, dot1, dot2;
	vec3_t		bestpoint;
	vec_t		bestdist;
	bool		in = true;

	for( x = 0; x < w->numpoints; x++ )
	{
		p1 = w->p[x];
		p2 = w->p[(x + 1) % w->numpoints];
		VectorSubtract( p2, p1, delta );
		CrossProduct( delta, planenormal, normal );
		dist = DotProduct( point, normal ) - DotProduct( p1, normal );

		if( dist < 0.0 )
		{
			in = false;
			CrossProduct( planenormal, normal, delta );
			dot0 = DotProduct( delta, point );
			dot1 = DotProduct( delta, p1 );
			dot2 = DotProduct( delta, p2 );

			if( dot1 < dot0 && dot0 < dot2 )
			{
				dist = dist / DotProduct( normal, normal );
				VectorMA( point, -dist, normal, point );
				return;
			}
		}
	}

	if( in ) return; // inside

	vec_t	planedot = DotProduct( planenormal, planenormal );

	for( x = 0; x < w->numpoints; x++ )
	{
		p1 = w->p[x];
		VectorSubtract( p1, point, delta );
		dist = DotProduct( delta, planenormal ) / planedot;
		VectorMA( delta, -dist, planenormal, delta );
		dot0 = DotProduct( delta, delta );

		if( x == 0 || dot0 < bestdist )
		{
			VectorAdd( point, delta, bestpoint );
			bestdist = dot0;
		}
	}

	if( w->numpoints > 0 )
		VectorCopy( bestpoint, point );
}

/*
============
WindingSnapPointEpsilon
============
*/
vec_t WindingSnapPointEpsilon( const winding_t *w, const vec3_t planenormal, vec3_t point, vec_t width, vec_t maxmove )
{
	dplane_t	planes[MAX_POINTS_ON_WINDING];
	int	x, pass, numplanes;
	vec3_t	v, bestpoint;
	vec_t	bestwidth;
	vec_t	newwidth;

	WindingSnapPoint( w, planenormal, point );
	numplanes = 0;

	for( x = 0; x < w->numpoints; x++ )
	{
		VectorSubtract( w->p[(x + 1) % w->numpoints], w->p[x], v );
		CrossProduct( v, planenormal, planes[numplanes].normal );
		vec_t len = VectorLength( planes[numplanes].normal );
		if( !len ) continue;

		VectorScale( planes[numplanes].normal, 1.0 / len, planes[numplanes].normal );
		planes[numplanes].dist = DotProduct( w->p[x], planes[numplanes].normal );
		numplanes++;
	}
	
	VectorCopy( point, bestpoint );
	newwidth = width;
	bestwidth = 0;

	// apply binary search method for 5 iterations to find the maximal distance that the point can be kept away from all the edges
	for( pass = 0; pass < 5; pass++ )
	{
		winding_t	*neww = CopyWinding( w );
		vec3_t	newpoint, planenormal;
		bool	failed = true;

		for( x = 0; x < numplanes && neww != NULL; x++ )
		{
			VectorCopy( planes[x].normal, planenormal );
			ChopWindingInPlace( &neww, planenormal, planes[x].dist + newwidth, ON_EPSILON, false );
		}

		if( neww && neww->numpoints > 0 )
		{
			VectorCopy( point, newpoint );
			WindingSnapPoint( neww, planenormal, newpoint );
			VectorSubtract( newpoint, point, v );

			if( VectorLength( v ) <= maxmove + ON_EPSILON )
				failed = false;
			FreeWinding( neww );
		}

		if( !failed )
		{
			VectorCopy( newpoint, bestpoint );
			bestwidth = newwidth;
			if( pass == 0 ) break;
			newwidth += width * pow( 0.5, pass + 1 );
		}
		else newwidth -= width * pow( 0.5, pass + 1 );
	}

	VectorCopy( bestpoint, point );

	return bestwidth;
}

/*
============
WindingOnPlaneSide
============
*/
int WindingOnPlaneSide( winding_t *w, const vec3_t normal, vec_t dist, vec_t epsilon )
{
	bool	front = false;
	bool	back = false;
	vec_t	d;

	for( int i = 0; i < w->numpoints; i++ )
	{
		d = DotProduct( w->p[i], normal ) - dist;

		if( d < -epsilon )
		{
			if( front )
				return SIDE_CROSS;
			back = true;
			continue;
		}

		if( d > epsilon )
		{
			if( back )
				return SIDE_CROSS;
			front = true;
			continue;
		}
	}

	if( back ) return SIDE_BACK;
	if( front ) return SIDE_FRONT;

	return SIDE_ON;
}