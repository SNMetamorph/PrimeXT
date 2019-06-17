/*
tracemesh.cpp - trace triangle meshes
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

#include	"extdll.h"
#include  "util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include  "nodes.h"
#include	"decals.h"
#include	"gamerules.h"
#include	"game.h"
#include	"com_model.h"
#include  "tracemesh.h"

void TraceMesh :: SetupTrace( const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, trace_t *tr )
{
	trace = tr;
	memset( trace, 0, sizeof( *trace ));
	trace->fraction = m_flRealFraction = 1.0f;

	m_vecStart = start;
	m_vecEnd = end;

	// build a bounding box of the entire move
	ClearBounds( m_vecAbsMins, m_vecAbsMaxs );

	m_vecStartMins = m_vecStart + mins;
	AddPointToBounds( m_vecStartMins, m_vecAbsMins, m_vecAbsMaxs );

	m_vecStartMaxs = m_vecStart + maxs;
	AddPointToBounds( m_vecStartMaxs, m_vecAbsMins, m_vecAbsMaxs );

	m_vecEndMins = m_vecEnd + mins;
	AddPointToBounds( m_vecEndMins, m_vecAbsMins, m_vecAbsMaxs );

	m_vecEndMaxs = m_vecEnd + maxs;
	AddPointToBounds( m_vecEndMaxs, m_vecAbsMins, m_vecAbsMaxs );

	// spread min\max by a pixel
	for( int i = 0; i < 3; i++ )
	{
		m_vecAbsMins[i] -= 1.0f;
		m_vecAbsMaxs[i] += 1.0f;
	}

	if( start == end )
		bIsTestPosition = true;
	else bIsTestPosition = false;
}

void TraceMesh :: ClipBoxToFacet( mfacet_t *facet )
{
	mplane_t	*p, *clipplane, *planes;
	float	enterfrac, leavefrac, distfrac;
	bool	getout, startout;
	float	d, d1, d2, f;

	if( !facet->numplanes )
		return;

	enterfrac = -1.0f;
	leavefrac = 1.0f;
	clipplane = NULL;
	checkcount++;

	getout = false;
	startout = false;
	planes = facet->planes;

	for( int i = 0; i < facet->numplanes; i++, planes++ )
	{
		p = planes;

		// push the plane out apropriately for mins/maxs
		if( p->type < 3 )
		{
			d1 = m_vecStartMins[p->type] - p->dist;
			d2 = m_vecEndMins[p->type] - p->dist;
		}
		else
		{
			switch( p->signbits )
			{
			case 0:
				d1 = p->normal.x * m_vecStartMins.x + p->normal.y * m_vecStartMins.y + p->normal.z * m_vecStartMins.z - p->dist;
				d2 = p->normal.x * m_vecEndMins.x + p->normal.y * m_vecEndMins.y + p->normal.z * m_vecEndMins.z - p->dist;
				break;
			case 1:
				d1 = p->normal.x * m_vecStartMaxs.x + p->normal.y * m_vecStartMins.y + p->normal.z * m_vecStartMins.z - p->dist;
				d2 = p->normal.x * m_vecEndMaxs.x + p->normal.y * m_vecEndMins.y + p->normal.z * m_vecEndMins.z - p->dist;
				break;
			case 2:
				d1 = p->normal.x * m_vecStartMins.x + p->normal.y * m_vecStartMaxs.y + p->normal.z * m_vecStartMins.z - p->dist;
				d2 = p->normal.x * m_vecEndMins.x + p->normal.y * m_vecEndMaxs.y + p->normal.z * m_vecEndMins.z - p->dist;
				break;
			case 3:
				d1 = p->normal.x * m_vecStartMaxs.x + p->normal.y * m_vecStartMaxs.y + p->normal.z * m_vecStartMins.z - p->dist;
				d2 = p->normal.x * m_vecEndMaxs.x + p->normal.y * m_vecEndMaxs.y + p->normal.z * m_vecEndMins.z - p->dist;
				break;
			case 4:
				d1 = p->normal.x * m_vecStartMins.x + p->normal.y * m_vecStartMins.y + p->normal.z * m_vecStartMaxs.z - p->dist;
				d2 = p->normal.x * m_vecEndMins.x + p->normal.y * m_vecEndMins.y + p->normal.z * m_vecEndMaxs.z - p->dist;
				break;
			case 5:
				d1 = p->normal.x * m_vecStartMaxs.x + p->normal.y * m_vecStartMins.y + p->normal.z * m_vecStartMaxs.z - p->dist;
				d2 = p->normal.x * m_vecEndMaxs.x + p->normal.y * m_vecEndMins.y + p->normal.z * m_vecEndMaxs.z - p->dist;
				break;
			case 6:
				d1 = p->normal.x * m_vecStartMins.x + p->normal.y * m_vecStartMaxs.y + p->normal.z * m_vecStartMaxs.z - p->dist;
				d2 = p->normal.x * m_vecEndMins.x + p->normal.y * m_vecEndMaxs.y + p->normal.z * m_vecEndMaxs.z - p->dist;
				break;
			case 7:
				d1 = p->normal.x * m_vecStartMaxs.x + p->normal.y * m_vecStartMaxs.y + p->normal.z * m_vecStartMaxs.z - p->dist;
				d2 = p->normal.x * m_vecEndMaxs.x + p->normal.y * m_vecEndMaxs.y + p->normal.z * m_vecEndMaxs.z - p->dist;
				break;
			default:
				d1 = d2 = 0.0f; // shut up compiler
				break;
			}
		}

		if( d2 > 0.0f ) getout = true;	// endpoint is not in solid
		if( d1 > 0.0f ) startout = true;

		// if completely in front of face, no intersection
		if( d1 > 0 && d2 >= d1 )
			return;

		if( d1 <= 0 && d2 <= 0 )
			continue;

		// crosses face
		d = 1 / (d1 - d2);
		f = d1 * d;

		if( d > 0.0f )
		{	
			// enter
			if( f > enterfrac )
			{
				distfrac = d;
				enterfrac = f;
				clipplane = p;
			}
		}
		else if( d < 0.0f )
		{	
			// leave
			if( f < leavefrac )
				leavefrac = f;
		}
	}

	if( !startout )
	{
		// original point was inside brush
		trace->startsolid = true;
		if( !getout ) trace->allsolid = true;
		return;
	}

	if( enterfrac - FRAC_EPSILON <= leavefrac )
	{
		if( enterfrac > -1 && enterfrac < m_flRealFraction )
		{
			if( enterfrac < 0 ) enterfrac = 0;
			m_flRealFraction = enterfrac;
			trace->plane.normal = clipplane->normal;
			trace->plane.dist = clipplane->dist;
			trace->fraction = enterfrac - DIST_EPSILON * distfrac;
		}
	}
}

void TraceMesh :: TestBoxInFacet( mfacet_t *facet )
{
	mplane_t	*p, *planes;

	if( !facet->numplanes )
		return;

	planes = facet->planes;
	checkcount++;

	for( int i = 0; i < facet->numplanes; i++, planes++ )
	{
		p = planes;

		// push the plane out apropriately for mins/maxs
		// if completely in front of face, no intersection
		if( p->type < 3 )
		{
			if( m_vecStartMins[p->type] > p->dist )
				return;
		}
		else
		{
			switch( p->signbits )
			{
			case 0:
				if( p->normal.x * m_vecStartMins.x + p->normal.y * m_vecStartMins.y + p->normal.z * m_vecStartMins.z > p->dist )
					return;
				break;
			case 1:
				if( p->normal.x * m_vecStartMaxs.x + p->normal.y * m_vecStartMins.y + p->normal.z * m_vecStartMins.z > p->dist )
					return;
				break;
			case 2:
				if( p->normal.x * m_vecStartMins.x + p->normal.y * m_vecStartMaxs.y + p->normal.z * m_vecStartMins.z > p->dist )
					return;
				break;
			case 3:
				if( p->normal.x * m_vecStartMaxs.x + p->normal.y * m_vecStartMaxs.y + p->normal.z * m_vecStartMins.z > p->dist )
					return;
				break;
			case 4:
				if( p->normal.x * m_vecStartMins.x + p->normal.y * m_vecStartMins.y + p->normal.z * m_vecStartMaxs.z > p->dist )
					return;
				break;
			case 5:
				if( p->normal.x * m_vecStartMaxs.x + p->normal.y * m_vecStartMins.y + p->normal.z * m_vecStartMaxs.z > p->dist )
					return;
				break;
			case 6:
				if( p->normal.x * m_vecStartMins.x + p->normal.y * m_vecStartMaxs.y + p->normal.z * m_vecStartMaxs.z > p->dist )
					return;
				break;
			case 7:
				if( p->normal.x * m_vecStartMaxs.x + p->normal.y * m_vecStartMaxs.y + p->normal.z * m_vecStartMaxs.z > p->dist )
					return;
				break;
			default:
				// signbits not initialized
				return;
			}
		}
	}

	// inside this brush
	m_flRealFraction = 0.0f;
	trace->startsolid = true;
	trace->allsolid = true;
}

void TraceMesh :: ClipToLinks( areanode_t *node )
{
	link_t	*l, *next;
	mfacet_t	*facet;

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;

		facet = FACET_FROM_AREA( l );

		if( !BoundsIntersect( m_vecAbsMins, m_vecAbsMaxs, facet->mins, facet->maxs ))
			continue;

		// might intersect, so do an exact clip
		if( !m_flRealFraction ) return;

		if( bIsTestPosition )
			TestBoxInFacet( facet );
		else ClipBoxToFacet( facet );
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( m_vecAbsMaxs[node->axis] > node->dist )
		ClipToLinks( node->children[0] );
	if( m_vecAbsMins[node->axis] < node->dist )
		ClipToLinks( node->children[1] );
}

bool TraceMesh :: DoTrace( void )
{
	if( !mesh || !BoundsIntersect( mesh->mins, mesh->maxs, m_vecAbsMins, m_vecAbsMaxs ))
		return false; // invalid mesh or no intersection

	checkcount = 0;

	if( areanodes )
	{
		ClipToLinks( areanodes );
	}
	else
	{
		mfacet_t *facet = mesh->facets;
		for( int i = 0; i < mesh->numfacets; i++, facet++ )
		{
			if( bIsTestPosition )
				TestBoxInFacet( facet );
			else ClipBoxToFacet( facet );

			if( !m_flRealFraction )
				break;
		}
	}

//	ALERT( at_aiconsole, "total %i checks for %s\n", checkcount, areanodes ? "tree" : "brute force" );

	trace->fraction = bound( 0.0f, trace->fraction, 1.0f );
	if( trace->fraction == 1.0f ) trace->endpos = m_vecEnd;
	else VectorLerp( m_vecStart, trace->fraction, m_vecEnd, trace->endpos );

	return (trace->fraction != 1.0f);
}
