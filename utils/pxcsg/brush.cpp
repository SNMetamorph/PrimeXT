/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// brush.c

#include "csg.h"

plane_t		g_mapplanes[MAX_INTERNAL_MAP_PLANES];
int		g_planehash[PLANE_HASHES];
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
bool PlaneEqual( const plane_t *p, const vec3_t normal, const vec3_t origin, vec_t dist )
{
	vec_t	t;

	if( -DIR_EPSILON < ( t = normal[0] - p->normal[0] ) && t < DIR_EPSILON &&
	    -DIR_EPSILON < ( t = normal[1] - p->normal[1] ) && t < DIR_EPSILON &&
	    -DIR_EPSILON < ( t = normal[2] - p->normal[2] ) && t < DIR_EPSILON )
	{
		t = PlaneDiff2( origin, p );

		if( -DIST_EPSILON < t && t < DIST_EPSILON )
			return true;
	}
	return false;
}

/*
================
AddPlaneToHash
================
*/
void AddPlaneToHash( plane_t *p )
{
	int hash;

	hash = (PLANE_HASHES - 1) & (int)fabs( p->dist );
	p->hash_chain = g_planehash[hash];
	g_planehash[hash] = p - g_mapplanes + 1;
}

/*
================
CreateNewFloatPlane
================
*/
int CreateNewFloatPlane( const vec3_t srcnormal, const vec3_t origin )
{
	plane_t	*p0, *p1, temp;
	vec3_t	normal;
	vec_t	dist;
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

	// snap plane distance
	dist = DotProduct( origin, normal );

	// only snap distance if the normal is an axis. Otherwise there
	// is nothing "natural" about snapping the distance to an integer.
	if( VectorIsOnAxis( normal ) && fabs( dist - Q_rint( dist )) < DIST_EPSILON )
		dist = Q_rint( dist ); // catch -0.0

	VectorCopy( origin, p0->origin );
	VectorCopy( normal, p0->normal );
	VectorCopy( origin, p1->origin );
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

		AddPlaneToHash( p0 );
		AddPlaneToHash( p1 );
		return g_nummapplanes - 1;
	}

	AddPlaneToHash( p0 );
	AddPlaneToHash( p1 );

	return g_nummapplanes - 2;
}

/*
=============
FindFloatPlane

=============
*/
int FindFloatPlane( const vec3_t normal, const vec3_t origin )
{
	int	i, hash, h;
	vec_t	dist;
	plane_t	*p;

	dist = DotProduct( origin, normal );
	hash = (PLANE_HASHES - 1) & (int)fabs( dist );

	ThreadLock();

	// search the border bins as well
	for( i = -1; i <= 1; i++ )
	{
		h = (hash + i) & (PLANE_HASHES - 1);
		for( int pidx = g_planehash[h] - 1; pidx != -1; pidx = g_mapplanes[pidx].hash_chain - 1 )
		{
			p = &g_mapplanes[pidx];

			if( PlaneEqual( p, normal, origin, dist ))
			{
				ThreadUnlock();
				return p - g_mapplanes;
			}
		}
	}

	// allocate a new two opposite planes
	int returnval = CreateNewFloatPlane( normal, origin );

	ThreadUnlock();

	return returnval;
}

int FindFloatPlane2( const vec3_t normal, const vec3_t origin )
{
	int	returnval;
	plane_t	*p, temp;
	vec_t	t;

	returnval = 0;

find_plane:
	for( ; returnval < g_nummapplanes; returnval++ )
	{
		// BUG: there might be some multithread issue --vluzacn
		if( -DIR_EPSILON < ( t = normal[0] - g_mapplanes[returnval].normal[0] ) && t < DIR_EPSILON &&
		    -DIR_EPSILON < ( t = normal[1] - g_mapplanes[returnval].normal[1] ) && t < DIR_EPSILON &&
		    -DIR_EPSILON < ( t = normal[2] - g_mapplanes[returnval].normal[2] ) && t < DIR_EPSILON )
		{
			t = DotProduct (origin, g_mapplanes[returnval].normal) - g_mapplanes[returnval].dist;

			if( -DIST_EPSILON < t && t < DIST_EPSILON )
				return returnval;
		}
	}

	ThreadLock();

	if( returnval != g_nummapplanes ) // make sure we don't race
	{
		ThreadUnlock();
		// check to see if other thread added plane we need
		goto find_plane;
	}

	// create new planes - double check that we have room for 2 planes
	if(( g_nummapplanes + 2 ) > MAX_INTERNAL_MAP_PLANES )
		COM_FatalError( "MAX_INTERNAL_MAP_PLANES limit exceeded\n" );

	p = &g_mapplanes[g_nummapplanes];
	VectorCopy( origin, p->origin );
	VectorCopy( normal, p->normal );
	VectorNormalize( p->normal );
	p->type = MapPlaneTypeForNormal( p->normal );
	// snap normal to nearest axial if possible
	if( p->type <= PLANE_LAST_AXIAL )
	{
		for( int i = 0; i < 3; i++ )
		{
			if( i == p->type )
				p->normal[i] = p->normal[i] > 0 ? 1 : -1;
			else p->normal[i] = 0;
		}
	}

	p->dist = DotProduct( origin, p->normal );
	VectorCopy( origin, (p+1)->origin );
	VectorNegate( p->normal, (p+1)->normal );
	(p+1)->type = p->type;
	(p+1)->dist = -p->dist;

	// always put axial planes facing positive first
	if( normal[(p->type) % 3] < 0 )
	{
		temp = *p;
		*p = *(p+1);
		*(p+1) = temp;
		returnval = g_nummapplanes + 1;
	}
	else
	{
		returnval = g_nummapplanes;
	}

	g_nummapplanes += 2;
	ThreadUnlock();

	return returnval;
}

/*
================
PlaneFromPoints
================
*/
int PlaneFromPoints( const vec_t *p0, const vec_t *p1, const vec_t *p2 )
{
	vec3_t	t1, t2, normal;

	VectorSubtract( p0, p1, t1 );
	VectorSubtract( p2, p1, t2 );
	CrossProduct( t1, t2, normal );

	if( !VectorNormalize( normal ))
		return -1;

	return FindFloatPlane( normal, p0 );
}

/*
=============================================================================

			CONTENTS DETERMINING

=============================================================================
*/
/*
===========
BrushCheckSides

some compilers may use SKIP instead of NULL texture
replace it for consistency
===========
*/
void BrushCheckSides( mapent_t *mapent, brush_t *b )
{
	int	i, nodraw_faces = 0;
	int	skip_faces = 0;
	int	hint_faces = 0;
	side_t	*s;

	// never apply this for a patches!
	if( FBitSet( b->flags, FBRUSH_PATCH ))
		return;

	// explicit non-solid brushes also ignore it
	if( FBitSet( b->flags, FBRUSH_NOCLIP ))
		return;

	for( i = 0; i < b->sides.Count(); i++ )
	{
		s = &b->sides[i];

		if( FBitSet( s->flags, FSIDE_NODRAW ))
			nodraw_faces++;

		if( FBitSet( s->flags, FSIDE_SKIP ) )
			skip_faces++;

		if( FBitSet( s->flags, FSIDE_HINT|FSIDE_SOLIDHINT ))
			hint_faces++;
	}

	// brush have "skip" faces and remaining faces not a hint
	if( skip_faces && !hint_faces )
	{
		for( i = 0; i < b->sides.Count(); i++ )
		{
			s = &b->sides[i];

			if( FBitSet( s->flags, FSIDE_SKIP ))
			{
				Q_strncpy( s->name, "NULL", sizeof( s->name ));
				SetBits( s->flags, FSIDE_NODRAW );
				ClearBits( s->flags, FSIDE_SKIP );
				s->contents = CONTENTS_SOLID;
			}
		}
	}
}

/*
===========
BrushContents
===========
*/
int BrushContents( mapent_t *mapent, brush_t *b )
{
	const char	*name = ValueForKey( (entity_t *)mapent, "classname" );
	int		i, best_i, contents;
	bool		assigned = false;
	int		nodraw_fases = 0;
	int		best_contents;
	side_t		*s;

	// cycle though the sides of the brush and attempt to get our best side contents for
	//  determining overall brush contents
	if( b->sides.Count() == 0 )
		COM_FatalError( "Entity %i, Brush %i: Brush with no sides.\n", b->originalentitynum, b->originalbrushnum );

	// for each face of each brush of this entity
	if( BoolForKey( (entity_t *)mapent, "zhlt_precisionclip" ))
		SetBits( b->flags, FBRUSH_PRECISIONCLIP );

	// non-solid detail entity
	if( !Q_stricmp( name, "func_detail_illusionary" ))
	{
		SetBits( b->flags, FBRUSH_NOCLIP );
		b->detaillevel = 2;
		return CONTENTS_EMPTY;
	}

	// solid detail entity
	if( !Q_stricmp( name, "func_detail_fence" ) || !Q_stricmp( name, "func_detail_wall" ))
	{
		b->detaillevel = 1;
	}

	BrushCheckSides( mapent, b );

	s = &b->sides[0];

	// NULL can't be used to select contents
	if( FBitSet( s->flags, FSIDE_NODRAW ))
		best_contents = CONTENTS_NULL;
	else best_contents = s->contents;
	best_i = 0;

	// SKIP doesn't split space in bsp process, ContentEmpty splits space normally.
	if( FBitSet( s->flags, FSIDE_SKIP ))
		assigned = true;

	for( i = 1; i < b->sides.Count(); i++ )
	{
		s = &b->sides[i];

		if( assigned ) break;

		if( FBitSet( s->flags, FSIDE_SKIP ))
		{
			best_contents = s->contents;
			assigned = true;
			best_i = i;
		}

		// g-cont. NULL has lower priority than other contents
		if( s->contents > best_contents && !FBitSet( s->flags, FSIDE_NODRAW ))
		{
			// if our current surface contents is better (larger)
			// than our best, make it our best.
			best_contents = s->contents;
			best_i = i;
		}
	}

	contents = best_contents;

	for( i = 0; i < b->sides.Count(); i++ )
	{
		s = &b->sides[i];

		if( FBitSet( s->flags, FSIDE_NODRAW ))
			nodraw_fases++;

		if( assigned && !FBitSet( s->flags, FSIDE_SKIP|FSIDE_HINT ) && s->contents != CONTENTS_ORIGIN )
			continue; // overwrite content for this texture

		// sky are not to cause mixed face contents
		if( s->contents == CONTENTS_SKY || FBitSet( s->flags, FSIDE_NODRAW ))
			continue;

		if( s->contents != best_contents )
		{
			MsgDev( D_ERROR, "Entity %i, Brush %i: mixed face contents\n Texture %s and %s\n",
			b->originalentitynum, b->originalbrushnum, b->sides[best_i].name, s->name ); 
			break;
		}
	}

	if( contents != CONTENTS_ORIGIN )
	{
		if( FBitSet( b->flags, FBRUSH_NOCLIP ))
			contents = CONTENTS_EMPTY;

		if( FBitSet( b->flags, FBRUSH_CLIPONLY ))
			contents = CONTENTS_SOLID;
	}

	if( contents == CONTENTS_NULL )
		contents = CONTENTS_SOLID;

	// check to make sure we dont have an origin brush as part of worldspawn
	if(( b->entitynum == 0 ) || ( !Q_strcmp( "func_group", name ) || !Q_strcmp( "func_landscape", name ) || !Q_strcmp( "func_detail", name )))
	{
		if( contents == CONTENTS_ORIGIN )
			MsgDev( D_ERROR, "Entity %i, Brush %i: origin brush not allowed in world\n", b->originalentitynum, b->originalbrushnum );
	}
	else
	{
		// otherwise its not worldspawn, therefore its an entity. check to make sure this brush is allowed
		//  to be an entity.
		switch( contents )
		{
		case CONTENTS_SOLID:
		case CONTENTS_WATER:
		case CONTENTS_SLIME:
		case CONTENTS_LAVA:
		case CONTENTS_ORIGIN:
		case CONTENTS_EMPTY:
			break;
		default:
			COM_FatalError( "Entity %i, Brush %i: %s brushes not allowed in entity", 
				b->originalentitynum, b->originalbrushnum, ContentsToString( contents ));
			break;
		}
	}

	// turn detail null-brushes into clipbrushes
	if( nodraw_fases == b->sides.Count() && b->detaillevel ) 
		SetBits( b->flags, FBRUSH_CLIPONLY );

	// TyrTools style details: chop faces but ignore visportals
	if( !Q_stricmp( "func_detail", name ) && ( contents == CONTENTS_SOLID || contents == CONTENTS_EMPTY ))
		b->detaillevel++;

	return contents;
}

/*
==============================================================================

BEVELED CLIPPING HULL GENERATION

This is done by brute force, and could easily get a lot faster if anyone cares.
==============================================================================
*/
#define MAX_HULL_POINTS	512
#define MAX_HULL_EDGES	1024

typedef struct expand_s
{
	// input
	brush_t	*b;
	int	hullnum;
	// output
	vec3_t	points[MAX_HULL_POINTS];
	vec3_t	corners[MAX_HULL_POINTS * 8];
	int	edges[MAX_HULL_EDGES][2];
	int	numpoints;
	int	numedges;
} expand_t;

/*
============
AddHullPlane
=============
*/
void AddHullPlane( expand_t *ex, const vec3_t normal, const vec3_t origin )
{
	int		planenum = FindFloatPlane( normal, origin );
	brushhull_t	*hull = &ex->b->hull[ex->hullnum];

	for( bface_t *f = hull->faces; f != NULL; f = f->next )
	{
		if( f->planenum == planenum )
			return; // don't add a plane twice
	}

	bface_t *face = AllocFace();
	face->planenum = planenum;
	face->plane = &g_mapplanes[face->planenum];
	face->contents[0] = CONTENTS_EMPTY;
	face->next = hull->faces;
	hull->faces = face;
	face->texinfo = -1;
}

/*
============
TestAddPlane

Adds the given plane to the brush description if all of the original brush
vertexes can be put on the front side
=============
*/
void TestAddPlane( expand_t *ex, const vec3_t normal, const vec3_t origin )
{
	int		planenum = FindFloatPlane( normal, origin );
	brushhull_t	*hull = &ex->b->hull[ex->hullnum];
	int		points_front, points_back;
	vec_t		*corner;
	vec_t		d;

	// see if the plane has allready been added
	for( bface_t *f = hull->faces; f != NULL; f = f->next )
	{
		if( f->planenum == planenum )
			return; // don't add a plane twice
	}

	// check all the corner points
	points_front = points_back = 0;
	corner = ex->corners[0];

	for( int i = 0; i < ex->numpoints * 8; i++, corner += 3 )
	{
		d  = (corner[0] - origin[0]) * normal[0];
		d += (corner[1] - origin[1]) * normal[1];
		d += (corner[2] - origin[2]) * normal[2];

		if( d < -ON_EPSILON )
		{
			if( points_front )
				return;
			points_back = 1;
		}
		else if( d > ON_EPSILON )
		{
			if( points_back )
				return;
			points_front = 1;
		}
	}

	bface_t *face = AllocFace();
	face->planenum = ( points_front ) ? planenum ^ 1 : planenum;
	face->plane = &g_mapplanes[face->planenum];
	face->contents[0] = CONTENTS_EMPTY;
	face->next = hull->faces;
	hull->faces = face;
	face->texinfo = -1;
}

/*
============
AddHullPoint

Doesn't add if duplicated
=============
*/
int AddHullPoint( expand_t *ex, const vec3_t p )
{
	vec_t	*c;
	int	i;

	for( i = 0; i < ex->numpoints; i++ )
	{
		if( VectorCompareEpsilon( p, ex->points[i], EQUAL_EPSILON ))
			return i;
	}	
	
	if( ex->numpoints == MAX_HULL_POINTS )
		COM_FatalError( "MAX_HULL_POINTS limit exceeded\n" );
	VectorCopy( p, ex->points[ex->numpoints] );
	c = ex->corners[i*8];
	ex->numpoints++;

	// also add eight hull corners	
	for( int x = 0; x < 2; x++ )
	{
		for( int y = 0; y < 2; y++ )
		{
			for( int z = 0; z < 2; z++ )
			{
				c[0] = p[0] + g_hull_size[ex->hullnum][x][0];
				c[1] = p[1] + g_hull_size[ex->hullnum][y][1];
				c[2] = p[2] + g_hull_size[ex->hullnum][z][2];
				c += 3;
			}
		}
	}
		
	return i;
}

/*
============
AddHullEdge

Creates all of the hull planes around the given edge, if not done already
=============
*/
void AddHullEdge( expand_t *ex, const vec3_t p1, const vec3_t p2 )
{
	vec3_t	edgevec, planevec;
	vec3_t	normal, origin;
	int	a, b, c, d, e, i;
	int	pt1, pt2;
	vec_t	length;
	
	pt1 = AddHullPoint( ex, p1 );
	pt2 = AddHullPoint( ex, p2 );

	for( i = 0; i < ex->numedges; i++ )
	{
		if(( ex->edges[i][0] == pt1 && ex->edges[i][1] == pt2 ) || ( ex->edges[i][0] == pt2 && ex->edges[i][1] == pt1 ))
			return; // already added
	}		

	if( ex->numedges == MAX_HULL_EDGES )
		COM_FatalError( "MAX_HULL_EDGES limit exceeded\n" );

	ex->edges[i][0] = pt1;
	ex->edges[i][1] = pt2;
	ex->numedges++;
		
	VectorSubtract( p1, p2, edgevec );
	VectorNormalize( edgevec );
	
	for( a = 0; a < 3; a++ )
	{
		b = (a + 1) % 3;
		c = (a + 2) % 3;

		planevec[a] = 1.0;
		planevec[b] = 0.0;
		planevec[c] = 0.0;
		CrossProduct( planevec, edgevec, normal );
		length = VectorNormalize( normal );

		// if this edge is almost parallel to the hull edge, skip it.
		if( length < NORMAL_EPSILON ) continue;

		for( d = 0; d <= 1; d++ )
		{
			for( e = 0; e <= 1; e++ )
			{
				VectorCopy( p1, origin );
				origin[b] += g_hull_size[ex->hullnum][d][b];
				origin[c] += g_hull_size[ex->hullnum][e][c];
				TestAddPlane( ex, normal, origin );
			}
		}
	}
}

/*
============
ExpandBrush
=============
*/
void ExpandBrush2( brush_t *b, int hullnum )
{
	bface_t		*brush_faces, *f;
	vec3_t		origin, normal;
	bool		axial = true;
	int		i, x, s;
	vec_t		corner;
	brushhull_t	*hull;
	expand_t		ex;
	plane_t		*p;

	brush_faces = b->hull[0].faces;
	hull = &b->hull[hullnum];
	ex.hullnum = hullnum;
	ex.numpoints = 0;
	ex.numedges = 0;
	ex.b = b;

	// expand all of the planes
	for( f = brush_faces; f != NULL; f = f->next )
	{
		p = f->plane;

		if( p->type > PLANE_LAST_AXIAL )
			axial = false; // not an xyz axial plane

		VectorCopy( p->origin, origin );
		VectorCopy( p->normal, normal );

		for ( x = 0; x < 3; x++ )
		{
			if( p->normal[x] > 0.0 )
				corner = g_hull_size[hullnum][1][x];
			else if( p->normal[x] < 0.0 )
				corner = -g_hull_size[hullnum][0][x];
			else corner = 0.0;

			origin[x] += p->normal[x] * corner;
		}

		bface_t *face = AllocFace();
		face->planenum = FindFloatPlane( normal, origin );
		face->plane = &g_mapplanes[face->planenum];
		face->contents[0] = CONTENTS_EMPTY;
		face->next = hull->faces;
		hull->faces = face;
		face->texinfo = -1;
	}

	// if this was an axial brush, we are done
	if( axial ) return;

	// add any axis planes not contained in the brush to bevel off corners
	for( x = 0; x < 3; x++ )
	{
		for( s = -1; s <= 1; s += 2 )
		{
			// add the plane
			VectorClear( normal );
			normal[x] = s;

			if( s == -1 ) VectorAdd( b->hull[0].mins, g_hull_size[hullnum][0], origin );
			else VectorAdd( b->hull[0].maxs, g_hull_size[hullnum][1], origin );
			AddHullPlane( &ex, normal, origin );
		}
	}

	// create all the hull points
	for( f = brush_faces; f != NULL; f = f->next )
	{
		for( i = 0; i < f->w->numpoints; i++ )
			AddHullPoint( &ex, f->w->p[i] );
	}

	// add all of the edge bevels
	for( f = brush_faces; f != NULL; f = f->next )
	{
		for( i = 0; i < f->w->numpoints; i++ )
			AddHullEdge( &ex, f->w->p[i], f->w->p[(i + 1) % f->w->numpoints] );
	}
}

/*
==============================================================================

BEVELED CLIPPING HULL GENERATION

This is done by brute force, and could easily get a lot faster if anyone cares.
==============================================================================
*/
// =====================================================================================
//  AddHullPlane (subroutine for replacement of ExpandBrush, KGP)
//  Called to add any and all clip hull planes by the new ExpandBrush.
// =====================================================================================
void AddHullPlane( brushhull_t *hull, const vec3_t normal, const vec3_t origin, bool check_planenum )
{
	int	planenum = FindFloatPlane( normal, origin );

	// check to see if this plane is already in the brush (optional to speed
	// up cases where we know the plane hasn't been added yet, like axial case)
	if( check_planenum )
	{
		// we know axial planes are added in last step
		if( g_mapplanes[planenum].type <= PLANE_LAST_AXIAL )
			return;

		for( bface_t *f = hull->faces; f != NULL; f = f->next )
		{
			if( f->planenum == planenum )
				return; // don't add a plane twice
		}
	}

	bface_t *face = AllocFace();
	face->planenum = planenum;
	face->plane = &g_mapplanes[face->planenum];
	face->contents[0] = CONTENTS_EMPTY;
	face->next = hull->faces;
	hull->faces = face;
	face->texinfo = -1;
}

/*
============
ExpandBrush
=============
*/
void ExpandBrush( brush_t *b, int hullnum )
{
	vec3_t		edge_start, edge_end;
	plane_t		*plane, *plane2;
	vec3_t		edge, bevel_edge;
	bool		start_found, end_found;
	bool		warned = false;
	vec3_t		origin, normal;
	bface_t		*f, *f2;
	winding_t		*w, *w2;
	brushhull_t	*hull;

	hull = &b->hull[hullnum];

	// step 1: for collision between player vertex and brush face. --vluzacn
	for( f = b->hull[0].faces; f != NULL; f = f->next )
	{
		plane = f->plane;

		// don't bother adding axial planes,
		// they're defined by adding the bounding box anyway
		if( plane->type <= PLANE_LAST_AXIAL )
			continue;

		// add the offset non-axial plane to the expanded hull
		VectorCopy( plane->origin, origin );
		VectorCopy( plane->normal, normal );

		// old code multiplied offset by normal -- this led to post-csg "sticky" walls where a
		// slope met an axial plane from the next brush since the offset from the slope would be less
		// than the full offset for the axial plane -- the discontinuity also contributes to increased
		// clipnodes. If the normal is zero along an axis, shifting the origin in that direction won't
		// change the plane number, so I don't explicitly test that case.  The old method is still used if
		// preciseclip is turned off to allow backward compatability -- some of the improperly beveled edges
		// grow using the new origins, and might cause additional problems.
		origin[0] += g_hull_size[hullnum][(normal[0] > 0 ? 1 : 0)][0];
		origin[1] += g_hull_size[hullnum][(normal[1] > 0 ? 1 : 0)][1];
		origin[2] += g_hull_size[hullnum][(normal[2] > 0 ? 1 : 0)][2];
		AddHullPlane( hull, normal, origin, false );
	}

	// step 2: for collision between player edge and brush edge. --vluzacn
	// split bevel check into a second pass so we don't have to check for duplicate planes when adding offset planes
	// in step above -- otherwise a bevel plane might duplicate an offset plane, causing problems later on.
	for( f = b->hull[0].faces; f != NULL; f = f->next )
	{
		plane = f->plane;

		// test to see if the plane is completely non-axial (if it is, need to add bevels to any
		// existing "inflection edges" where there's a sign change with a neighboring plane's normal for
		// a given axis)

		// move along w and find plane on other side of each edge. If normals change sign,
		// add a new plane by offsetting the points of the w to bevel the edge in that direction.
		// It's possible to have inflection in multiple directions -- in this case, a new plane
		// must be added for each sign change in the edge.
		w = f->w;

		// do it for each edge
		for( int i = 0; i < w->numpoints; i++ )
		{
			VectorCopy( w->p[i], edge_start );
			VectorCopy( w->p[(i + 1) % w->numpoints], edge_end );

			// grab the edge (find relative length)
			VectorSubtract( edge_end, edge_start, edge );

			// brute force - need to check every other w for common points
			// if the points match, the other face is the one we need to look at.
			for( f2 = b->hull[0].faces; f2 != NULL; f2 = f2->next )
			{
				if( f == f2 ) continue;

				start_found = false;
				end_found = false;
				w2 = f2->w;

				for( int j = 0; j < w2->numpoints; j++ )
				{
					if( !start_found && VectorCompareEpsilon( w2->p[j], edge_start, EQUAL_EPSILON ))
						start_found = true;

					if( !end_found && VectorCompareEpsilon( w2->p[j], edge_end, EQUAL_EPSILON ))
						end_found = true;

					// we've found the face we want, move on to planar comparison
					if( start_found && end_found )
						break;
				}

				if( start_found && end_found )
					break;
			}

			if( !f2 )
			{
				if( hullnum == 1 && !warned )
				{
					MsgDev( D_WARN, "Illegal Brush (edge without opposite face): Entity %i, Brush %i\n",
					b->originalentitynum, b->originalbrushnum );
					warned = true;
				}
				continue;
			}

			plane2 = f2->plane;


			// check each direction for sign change in normal -- zero can be safely ignored
			for( int dir = 0; dir < 3; dir++ )
			{
				// if sign changed, add bevel
				if( plane->normal[dir] * plane2->normal[dir] < -NORMAL_EPSILON )
				{
					// pick direction of bevel edge by looking at normal of existing planes
					VectorClear( bevel_edge );
					bevel_edge[dir] = (plane->normal[dir] > 0) ? -1 : 1;

					// find normal by taking normalized cross of the edge vector and the bevel edge
					CrossProduct( edge, bevel_edge, normal );
					VectorNormalize( normal );

					if( fabs( normal[(dir+1)%3]) <= NORMAL_EPSILON || fabs( normal[(dir+2)%3] ) <= NORMAL_EPSILON )
					{
						// coincide with axial plane
						continue;
					}

					// get the origin
					VectorCopy( edge_start, origin );

					// note: if normal == 0 in direction indicated, shifting origin doesn't change plane #
					origin[0] += g_hull_size[hullnum][(normal[0] > 0 ? 1 : 0)][0];
					origin[1] += g_hull_size[hullnum][(normal[1] > 0 ? 1 : 0)][1];
					origin[2] += g_hull_size[hullnum][(normal[2] > 0 ? 1 : 0)][2];

					// add the bevel plane to the expanded hull
					AddHullPlane( hull, normal, origin, true );
				}
			}
		}
	}

	// step 3: for collision between player face and brush vertex. --vluzacn
	// add the bounding box to the expanded hull -- for a
	// completely axial brush, this is the only necessary step

	// add mins
	VectorAdd( b->hull[0].mins, g_hull_size[hullnum][0], origin );
	VectorSet( normal, -1.0, 0.0, 0.0 );
	AddHullPlane( hull, normal, origin, false );
	VectorSet( normal, 0.0, -1.0, 0.0 );
	AddHullPlane( hull, normal, origin, false );
	VectorSet( normal, 0.0, 0.0, -1.0 );
	AddHullPlane( hull, normal, origin, false );

	// add maxs
	VectorAdd( b->hull[0].maxs, g_hull_size[hullnum][1], origin );
	VectorSet( normal, 1.0, 0.0, 0.0 );
	AddHullPlane( hull, normal, origin, false );
	VectorSet( normal, 0.0, 1.0, 0.0 );
	AddHullPlane( hull, normal, origin, false );
	VectorSet( normal, 0.0, 0.0, 1.0 );
	AddHullPlane( hull, normal, origin, false );

#if _DEBUG
	// sanity check
	for( f = hull->faces; f != NULL; f = f->next )
	{
		for( f2 = b->hull[0].faces; f2 != NULL; f2 = f2->next )
		{
			if( f2->w->numpoints < 3 )
				continue;

			for( int i = 0; i < f2->w->numpoints; i++ )
			{
				if( DotProduct( f->plane->normal, f->plane->origin ) < DotProduct( f->plane->normal, f2->w->p[i] ))
				{
					MsgDev( D_WARN, "Illegal Brush (clip hull [%i] has backward face): Entity %i, Brush %i\n",
					hullnum, b->originalentitynum, b->originalbrushnum );
					break;
				}
			}
		}
	}
#endif
}

//============================================================================
void FreeHullFaces( void )
{
	for( int i = 0; i < g_nummapbrushes; i++ )
	{
		brush_t *b = &g_mapbrushes[i];

		for( int j = 0; j < MAX_MAP_HULLS; j++ )
		{
			UnlinkFaces( &b->hull[j].faces );
		}

		// throw sides too
		b->sides.Purge();
	}
}

void SortHullFaces( brushhull_t *h )
{
	int	numfaces;
	bface_t	**faces;
	vec3_t	*normals;
	bool	*isused;
	int	i, j;
	int	*sorted;
	bface_t	*f;

	for( numfaces = 0, f = h->faces; f != NULL; f = f->next )
		numfaces++;

	faces = (bface_t **)Mem_Alloc( numfaces * sizeof( bface_t* ), C_TEMPORARY );
	normals = (vec3_t *)Mem_Alloc( numfaces * sizeof( vec3_t ), C_TEMPORARY );
	isused = (bool *)Mem_Alloc( numfaces * sizeof( bool ), C_TEMPORARY );
	sorted = (int *)Mem_Alloc( numfaces * sizeof( int ), C_TEMPORARY );

	for( i = 0, f = h->faces; f != NULL; i++, f = f->next )
	{
		const plane_t *p = &g_mapplanes[f->planenum];
		VectorCopy( p->normal, normals[i] );
		faces[i] = f;
	}

	for( i = 0; i < numfaces; i++ )
	{
		int	bestaxial = -1;
		int	bestside;

		for( j = 0; j < numfaces; j++)
		{
			if( isused[j] ) continue;

			int axial = AxisFromNormal( normals[j] );

			if( axial > bestaxial )
			{
				bestaxial = axial;
				bestside = j;
			}
		}

		sorted[i] = bestside;
		isused[bestside] = true;
	}

	for( i = -1; i < numfaces; i++ )
	{
		*(i >= 0 ? &faces[sorted[i]]->next: &h->faces) = (i + 1 < numfaces ? faces[sorted[i + 1]] : NULL);
	}

	Mem_Free( faces, C_TEMPORARY );
	Mem_Free( normals, C_TEMPORARY );
	Mem_Free( isused, C_TEMPORARY );
	Mem_Free( sorted, C_TEMPORARY );
}

/*
===========
MakeHullFaces
===========
*/
void MakeHullFaces( brush_t *b, brushhull_t *h, int hullnum )
{
	bface_t		*f, *f2;
	mapent_t		*mapent;
	bool		warned = false;
	vec_t		v, area;
	int		i, j;
	vec3_t		point;
	winding_t		*w;
	plane_t		*p;

	mapent = &g_mapentities[b->entitynum];

	// sorted faces make BSP-splits is more axial than unsorted
	SortHullFaces( h );
restart:
	ClearBounds( h->mins, h->maxs );

	for( f = h->faces; f != NULL; f = f->next )
	{
		w = BaseWindingForPlane( f->plane->normal, f->plane->dist );

		ASSERT( w != NULL );

		for( f2 = h->faces; f2 != NULL; f2 = f2->next )
		{
			if( f == f2 ) continue;

			p = &g_mapplanes[f2->planenum ^ 1];

			if( !ChopWindingInPlace( &w, p->normal, p->dist, NORMAL_EPSILON, false ))
				break; // nothing to chop?
		}

		RemoveColinearPointsEpsilon( w, ON_EPSILON );

		area = WindingArea( w );

		if( area < MICROVOLUME )
		{
			if( w ) FreeWinding( w );

			if( !w && !warned && hullnum == 0 )
			{
				MsgDev( D_WARN, "Illegal Brush (plane doesn't contribute to final shape): Entity %i, Brush %i\n",
				b->originalentitynum, b->originalbrushnum );
				warned = true;
			}

			// remove the face and regenerate the hull
			UnlinkFaces( &h->faces, f );
			goto restart;
		}

		f->contents[0] = CONTENTS_EMPTY;
		f->w = w;	// at this point winging is always valid

		for( i = 0; i < w->numpoints; i++ )
		{
			for( j = 0; j < 3; j++ )
			{
				point[j] = w->p[i][j];
				v = Q_rint( point[j] );

				if( fabs( point[j] - v ) < DIR_EPSILON )
					w->p[i][j] = v;
				else w->p[i][j] = point[j];

				// check for incomplete brushes
				if( w->p[i][j] >= WORLD_MAXS || w->p[i][j] <= WORLD_MINS )
					break;
			}

			// remove this brush
			if( j < 3 )
			{
				UnlinkFaces( &h->faces );
				MsgDev( D_REPORT, "Entity %i, Brush %i: degenerate brush was removed\n",
				b->originalentitynum, b->originalbrushnum );
				return;
			}

			if( !FBitSet( f->flags, FSIDE_SKIP ))
				AddPointToBounds( w->p[i], h->mins, h->maxs );
		}

		// make sure what all faces has valid ws
		CheckWindingEpsilon( w, ON_EPSILON, ( hullnum == 0 ));
	}

	for( i = 0; i < 3; i++ )
	{
		if( h->mins[i] < WORLD_MINS || h->maxs[i] > WORLD_MAXS )
		{
			MsgDev( D_ERROR, "Entity %i, Brush %i: outside world(+/-%d): (%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)\n",
			b->originalentitynum, b->originalbrushnum, BOGUS_RANGE / 2,
			h->mins[0], h->mins[1], h->mins[2], h->maxs[0], h->maxs[1], h->maxs[2] );
			break;
		}
	}

	if( i == 3 && hullnum == 0 )
	{
		// compute total entity bounds
		AddPointToBounds( h->mins, mapent->absmin, mapent->absmax );
		AddPointToBounds( h->maxs, mapent->absmin, mapent->absmax );
	}
}

/*
===========
MakeBrushPlanes
===========
*/
int MakeBrushPlanes( brush_t *b )
{
	bool	skipface;
	int	badsides = 0;
	vec3_t	planepts[3];
	vec3_t	origin;
	bface_t	*f;
	side_t	*s;

	//
	// if the origin key is set (by an origin brush), offset all of the values
	//
	GetVectorForKey( (entity_t *)&g_mapentities[b->entitynum], "origin", origin );

	// convert to mapplanes
	for( int i = 0; i < b->sides.Count(); i++ )
	{
		s = &b->sides[i];
		skipface = false;

		if( b->entitynum )
		{
			for( int k = 0; k < 3; k++ )
				VectorSubtract( s->planepts[k], origin, planepts[k] );
			s->planenum = PlaneFromPoints( planepts[0], planepts[1], planepts[2] );
		}
		else
		{
			// world doesn't required offset by origin
			s->planenum = PlaneFromPoints( s->planepts[0], s->planepts[1], s->planepts[2] );
		}

		if( s->planenum == -1 )
		{
			MsgDev( D_REPORT, "Entity %i, Brush %i: plane with no normal\n", b->originalentitynum, b->originalbrushnum );
			badsides++;
			continue;
		}

		// see if the plane has been used already
		for( f = b->hull[0].faces; f != NULL; f = f->next )
		{
			// g-cont. there is non fatal for us. just reject this face and trying again
			if( f->planenum == s->planenum || f->planenum == ( s->planenum ^ 1 ))
			{
				MsgDev( D_REPORT, "Entity %i, Brush %i, Side %i: has a coplanar plane\n",
				b->originalentitynum, b->originalbrushnum, i );
				skipface = true;
				badsides++;
				break;
			}
		}

		if( skipface ) continue;

		f = AllocFace();
		f->planenum = s->planenum;
		f->plane = &g_mapplanes[s->planenum];
		f->next = b->hull[0].faces;
		b->hull[0].faces = f;
		f->texinfo = g_onlyents ? -1 : TexinfoForSide( f->plane, s, origin );
		f->flags = s->flags;
	}

	return badsides;
}

/*
===========
CreateBrushFaces
===========
*/
void CreateBrushFaces( brush_t *b )
{
	// convert brush sides to planes
	int	badsides = MakeBrushPlanes( b );

	if( badsides > 0 )
	{
		MsgDev( D_WARN, "Entity %i, Brush %i: has %d invalid sides (total %d)\n",
		b->originalentitynum, b->originalbrushnum, badsides, b->sides.Count() );
	}

	MakeHullFaces( b, &b->hull[0], 0 );

	if( b->contents == CONTENTS_EMPTY || b->contents == CONTENTS_ORIGIN )
		return;

	if( !g_noclip && ( FBitSet( b->flags, FBRUSH_CLIPONLY ) || !FBitSet( b->flags, FBRUSH_NOCLIP )))
	{
		for( int h = 1; h < MAX_MAP_HULLS; h++ )
		{
			if( VectorIsNull( g_hull_size[h][0] ) && VectorIsNull( g_hull_size[h][1] ))
				continue;

			if( FBitSet( b->flags, FBRUSH_PRECISIONCLIP ))
				ExpandBrush2( b, h );
			else ExpandBrush( b, h );
			MakeHullFaces( b, &b->hull[h], h );
		}
	}

	// invisible detail brush it's a clipbrush
	if( FBitSet( b->flags, FBRUSH_CLIPONLY ))
	{
		UnlinkFaces( &b->hull[0].faces );
		b->hull[0].faces = NULL;
	}
}

/*
===========
CreateBrush

multi-thread version
===========
*/
void CreateBrush( int brushnum, int threadnum )
{
	CreateBrushFaces( &g_mapbrushes[brushnum] );
}

/*
==================
DeleteBrushFaces

release specified face or purge all chain
==================
*/
void DeleteBrushFaces( brush_t *b )
{
	if( !b ) return;

	for( int i = 0; i < MAX_MAP_HULLS; i++ )
		UnlinkFaces( &b->hull[i].faces );
}

/*
===========
DumpBrushPlanes

NOTE: never alloc planes during threads work because planes order will be 
different for each compile and we gets various BSP'ing result every
time when we compile map again!
===========
*/
void DumpBrushPlanes( void )
{
#if 0
	for( int i = 0; i < g_nummapbrushes; i++ )
	{
		brush_t *b = &g_mapbrushes[i];

		if( b->contents == CONTENTS_ORIGIN )
			continue;

		Msg( "brush #%i\n", i );

		// convert to mapplanes
		for( int j = 0; j < b->sides.Count(); j++ )
		{
			side_t	*s = &b->sides[j];
			plane_t	*mp = &g_mapplanes[s->planenum];

			Msg( "#%i, (%g %g %g) - (%g), %d\n", j, mp->normal[0], mp->normal[1], mp->normal[2], mp->dist, mp->type );
		}
	}
#endif
}

void ProcessAutoOrigins( void )
{
	char		string[32];
	const char	*classname;
	const char	*pclassname;
	const char	*ptarget;
	mapent_t		*parent;
	mapent_t		*mapent;
	vec3_t		origin;
	int		c_origins_processed = 0;
	bool		origin_from_parent;

	MsgDev( D_REPORT, "ProcessAutoOrigins:\n" );

	// skip the world
	for( int i = 1; i < g_mapentities.Count(); i++ )
	{
		origin_from_parent = false;
		mapent = &g_mapentities[i];
		parent = NULL;

		if( !mapent->numbrushes )
			continue;

		// for some reasons entity doesn't have a valid size
		if( BoundsIsCleared( mapent->absmin, mapent->absmax ))
			continue;

		GetVectorForKey( (entity_t *)mapent, "origin", origin );

		// origin was set by level-designer
		if( !VectorIsNull( origin ))
			continue;

		classname = ValueForKey( (entity_t *)mapent, "classname" );

		// g-cont. old-good hack for my entity :-)
		if( !Q_strcmp( classname, "func_traindoor" ))
		{
			if( CheckKey( (entity_t *)mapent, "train" ))
				parent = FindTargetMapEntity( &g_mapentities, ValueForKey( (entity_t *)mapent, "train" ));
		}
		else if( !Q_strncmp( classname, "rotate_", 7 ))
		{
			// Quake1 rotational objects support
			parent = FindTargetMapEntity( &g_mapentities, ValueForKey( (entity_t *)mapent, "target" ));
			origin_from_parent = true;
		}
		else if( !Q_strncmp( classname, "func_portal", 11 ))
		{
			// portal always required origin-brush
			parent = mapent;
		}

		if( !parent && CheckKey( (entity_t *)mapent, "parent" ))
			parent = FindTargetMapEntity( &g_mapentities, ValueForKey( (entity_t *)mapent, "parent" ));

		if( !parent ) continue;

		pclassname = ValueForKey( (entity_t *)parent, "classname" );
		ptarget = ValueForKey( (entity_t *)parent, "targetname" );

		if( origin_from_parent || !Q_strcmp( pclassname, "func_door_rotating" ))
		{
			GetVectorForKey( (entity_t *)parent, "origin", origin );
			Q_snprintf( string, sizeof( string ), "%.1f %.1f %.1f", origin[0], origin[1], origin[2] );
			SetKeyValue( (entity_t *)mapent, "origin", string );
		}
		else
		{
			// now we have:
			// 1. entity with valid absmin\absmax
			// 2. it have valid parent
			// 3. it doesn't have custom origin
			VectorAverage( mapent->absmin, mapent->absmax, origin );
			Q_snprintf( string, sizeof( string ), "%.1f %.1f %.1f", origin[0], origin[1], origin[2] );
			SetKeyValue( (entity_t *)mapent, "origin", string );
		}

		MsgDev( D_REPORT, "%s, parent %s (%s) auto origin %g %g %g\n", classname, pclassname, ptarget, origin[0], origin[1], origin[2] );

		for( int j = 0; j < mapent->numbrushes; j++ )
		{
			brush_t *b = &g_mapbrushes[mapent->firstbrush + j];

			// remove old brush faces
			DeleteBrushFaces( b );

			// new brush with origin-adjusted sides
			CreateBrushFaces( b );
		}		
		c_origins_processed++;
	}

	if( c_origins_processed > 0 )
		MsgDev( D_INFO, "total %i entities was adjusted with auto-origin\n", c_origins_processed );
}

void RestoreModelOrigins( void )
{
	const char	*pclassname;
	const char	*ptarget;
	char		string[32];

	for( int i = 1; i < g_nummodels; i++ )
	{
		mapent_t	*ent = MapEntityForModel( i );
		dmodel_t	*bm = &g_dmodels[i];

		if( VectorIsNull( bm->origin ))
			continue;

		// for some reasons origin already set
		if( CheckKey( (entity_t *)ent, "origin" ))
			continue;

		pclassname = ValueForKey( (entity_t *)ent, "classname" );
		ptarget = ValueForKey( (entity_t *)ent, "targetname" );

		MsgDev( D_REPORT, "%s, (%s) auto origin %g %g %g\n", pclassname, ptarget, bm->origin[0], bm->origin[1], bm->origin[2] ); 
		Q_snprintf( string, sizeof( string ), "%.1f %.1f %.1f", bm->origin[0], bm->origin[1], bm->origin[2] );
		SetKeyValue( (entity_t *)ent, "origin", string );		
	}
}