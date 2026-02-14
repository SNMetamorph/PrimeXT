/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// trace.c

#include "qrad.h"
#include "alias.h"
#include "../../engine/studio.h"
#include "model_trace.h"

typedef struct moveclip_s
{
	vec3_t		boxmins, boxmaxs;	// enclose the test object along entire move
	const float	*start, *end;
	vec3_t		direction;
	vec_t		distance;
	bool		nomodels;
	entity_t		*ignore;
	trace_t		trace;
	tmesh_t		*mesh;		// mesh trace (may be NULL)
} moveclip_t;

typedef struct tnode_s
{
	int		type;
	vec3_t		normal;
	float		dist;
	int		children[2];
	word		firstface;
	word		numfaces;		// counting both sides
} tnode_t;

static tnode_t		*tnode_p;
static aabb_tree_t		entity_tree;
static int		numsolidedicts;
static twface_t		*g_world_faces[MAX_MAP_FACES];	// polygons that turned into triangles

/*
=============
SampleMiptex

fence texture testing
=============
*/
int SampleMiptex( const dface_t *face, const vec3_t point )
{
	vec_t		ds, dt;
	byte		*data;
	int		x, y;
	dtexinfo_t	*tx;
	miptex_t		*mt;

	tx = &g_texinfo[face->texinfo];
	mt = GetTextureByMiptex( tx->miptex );
	if( !mt ) return CONTENTS_EMPTY;

	if( mt->name[0] != '{' )
		return CONTENTS_SOLID;

	ds = DotProduct( point, tx->vecs[0] ) + tx->vecs[0][3];
	dt = DotProduct( point, tx->vecs[1] ) + tx->vecs[1][3];

	// convert ST to real pixels position
	x = fix_coord( ds, mt->width );
	y = fix_coord( dt, mt->height );

	ASSERT( x >= 0 && y >= 0 );

	data = ((byte *)mt) + mt->offsets[0];
	if( data[(mt->width * y) + x] == 255 )
		return CONTENTS_EMPTY;
	return CONTENTS_SOLID;
}

/*
==================
MoveBounds
==================
*/
void MoveBounds( const vec3_t start, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs )
{
	for( int i = 0; i < 3; i++ )
	{
		if( end[i] > start[i] )
		{
			boxmins[i] = start[i] - 1.0f;
			boxmaxs[i] = end[i] + 1.0f;
		}
		else
		{
			boxmins[i] = end[i] - 1.0f;
			boxmaxs[i] = start[i] + 1.0f;
		}
	}
}

/*
==================
CombineTraces
==================
*/
trace_t CombineTraces( trace_t *cliptrace, trace_t *trace )
{
	if( trace->fraction < cliptrace->fraction )
		*cliptrace = *trace;
	return *cliptrace;
}

/*
==============
MakeTnode

Converts the disk node structure into the efficient tracing structure
==============
*/
void MakeTnode( tnode_t *head, int nodenum )
{
	dplane_t	*plane;
	dnode_t 	*node;
	tnode_t	*t;
	
	t = tnode_p++;

	node = g_dnodes + nodenum;
	plane = g_dplanes + node->planenum;

	t->type = plane->type;
	VectorCopy( plane->normal, t->normal );
	t->dist = plane->dist;
	t->firstface = node->firstface;
	t->numfaces = node->numfaces;
	
	for( int i = 0; i < 2; i++ )
	{
		if( node->children[i] < 0 )
		{
			t->children[i] = g_dleafs[-node->children[i] - 1].contents;
		}
		else
		{
			t->children[i] = tnode_p - head;
			MakeTnode( head, node->children[i] );
		}
	}
}

/*
=============
CountTnodes_r

count nodes for a given model
=============
*/
static void CountTnodes_r( int &total, int nodenum )
{
	// leaf?
	if( nodenum < 0 ) return;

	total++;

	CountTnodes_r( total, g_dnodes[nodenum].children[0] );
	CountTnodes_r( total, g_dnodes[nodenum].children[1] );
}

/*
=============
MakeTnodes

Loads the node structure out of a .bsp file to be used for light occlusion
=============
*/
static void MakeTnodes( entity_t *ent, dmodel_t *mod )
{
	int	numnodes = 0;

	CountTnodes_r( numnodes, mod->headnode[0] );
	if( ent->cache ) Mem_Free( ent->cache );

	ent->cache = Mem_Alloc(( numnodes + 1 ) * sizeof( tnode_t ));
	tnode_p = (tnode_t *)ent->cache;

	MakeTnode( tnode_p, mod->headnode[0] );
	ent->modtype = mod_brush;
}

//==========================================================
/*
==================
ClipMoveToTriangle

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
void ClipMoveToTriangle( tface_t *face, moveclip_t *clip )
{
	// compute plane intersection
	float DDotN = DotProduct( clip->direction, face->normal );

	// while vertex lighting is disabled we should lighting space
	// under model so R_LightVec will working correctly
	if( !FBitSet( clip->mesh->flags, FMESH_VERTEX_LIGHTING|FMESH_MODEL_LIGHTMAPS ))
	{
		if( !FBitSet( face->texture->flags, STUDIO_NF_TWOSIDE ))
		{
			// NOTE: probably we should normalize the n but for speed reasons i don't do it
			if( DDotN >= FLT_EPSILON ) return;
		}
	}

	// mask off zero or near zero (ray parallel to surface)
	bool did_hit = (( DDotN > FLT_EPSILON ) || ( DDotN < -FLT_EPSILON ));
	if( !did_hit ) return; // to prevent division by zero

	float numerator = face->NdotP1 - DotProduct( clip->start, face->normal );
	float isect_t = numerator / DDotN; // fraction
	float frac = isect_t / clip->distance;	// remap to [0..1]

	// now, we have the distance to the plane. lets update our mask
	did_hit = did_hit && ( frac > clip->mesh->backfrac );
	did_hit = did_hit && ( frac < clip->trace.fraction );
	if( !did_hit ) return;

	// now, check 3 edges
	float hitc1 = clip->start[face->pcoord0] + ( isect_t * clip->direction[face->pcoord0] );
	float hitc2 = clip->start[face->pcoord1] + ( isect_t * clip->direction[face->pcoord1] );
					
	// do barycentric coordinate check
	float B0 = face->edge1[0] * hitc1 + face->edge1[1] * hitc2 + face->edge1[2];
	did_hit = did_hit && ( B0 >= 0.0f );

	float B1 = face->edge2[0] * hitc1 + face->edge2[1] * hitc2 + face->edge2[2];
	did_hit = did_hit && ( B1 >= 0.0f );

	float B2 = B0 + B1;
	did_hit = did_hit && ( B2 <= 1.0f );

	if( !did_hit ) return;

	// if the triangle is transparent
	if( face->texture->data )
	{
		float	u = 1.0 - B2;
		float	v = B0;
		float	w = B1;

		// assuming a triangle indexed as v0, v1, v2
		// the projected edge equations are set up such that the vert opposite the first
		// equation is v2, and the vert opposite the second equation is v0
		// Therefore we pass them back in 1, 2, 0 order
		// Also B2 is currently B1 + B0 and needs to be 1 - (B1+B0) in order to be a real
		// barycentric coordinate.  Compute that now and pass it to the callback
		float s = w * clip->mesh->verts[face->a].st[0] + u * clip->mesh->verts[face->b].st[0] + v * clip->mesh->verts[face->c].st[0];
		float t = w * clip->mesh->verts[face->a].st[1] + u * clip->mesh->verts[face->b].st[1] + v * clip->mesh->verts[face->c].st[1];

		// convert ST to real pixels position
		int x = fix_coord( s * face->texture->width, face->texture->width );
		int y = fix_coord( t * face->texture->height, face->texture->height );

		// test pixel
		if( face->texture->data[(face->texture->width * y) + x] == 255 )
			return;
	}

	if( clip->trace.fraction > frac )
	{
		// at this point we hit the opaque pixel
		clip->trace.fraction = bound( 0.0, frac, 1.0 );
		clip->trace.contents = face->contents;
	}
}

//==========================================================
#define HLRAD_TRACE_FACES

/*
==================
TestLine_r

==================
*/
int TestLine_r( tnode_t *head, int node, vec_t p1f, vec_t p2f, const vec3_t start, const vec3_t stop, trace_t *trace )
{
	tnode_t	*tnode;
	float	front, back;
	float	frac, midf;
	int		i, j, r, side;
	vec3_t	mid;
loc0:
	if( node < 0 )
	{
		// water, slime or lava interpret as empty
		if( node == CONTENTS_SOLID )
			return CONTENTS_SOLID;
		if( node == CONTENTS_SKY )
			return CONTENTS_SKY;
		trace->fraction = 1.0f;
		return CONTENTS_EMPTY;
	}

	tnode = &head[node];
	front = PlaneDiff( start, tnode );
	back = PlaneDiff( stop, tnode );
#ifdef HLRAD_TestLine_EDGE_FIX
	if( front > FRAC_EPSILON / 2 && back > FRAC_EPSILON / 2 )
	{
		node = tnode->children[0];
		goto loc0;
	}

	if( front < -FRAC_EPSILON / 2 && back < -FRAC_EPSILON / 2 )
	{
		node = tnode->children[1];
		goto loc0;
	}

	if( fabs( front ) <= FRAC_EPSILON && fabs( back ) <= FRAC_EPSILON )
	{
		int	r1 = TestLine_r( head, tnode->children[0], p1f, p2f, start, stop, trace );

		if( r1 == CONTENTS_SOLID )
		{
			trace->contents = r1;
			return CONTENTS_SOLID;
		}

		int	r2 = TestLine_r( head, tnode->children[1], p1f, p2f, start, stop, trace );

		if( r2 == CONTENTS_SOLID )
		{
			trace->contents = r2;
			return CONTENTS_SOLID;
		}

		if( r1 == CONTENTS_SKY || r2 == CONTENTS_SKY )
		{
			trace->contents = r2;
			return CONTENTS_SKY;
		}

		trace->contents = CONTENTS_EMPTY;
		trace->fraction = 1.0f;
		return CONTENTS_EMPTY;
	}

	side = (front - back) < 0;
	frac = front / (front - back);
	frac = bound( 0.0, frac, 1.0 );
#else
	if( front >= -FRAC_EPSILON && back >= -FRAC_EPSILON )
	{
		node = tnode->children[0];
		goto loc0;
	}

	if( front < FRAC_EPSILON && back < FRAC_EPSILON )
	{
		node = tnode->children[1];
		goto loc0;
	}

	side = (front < 0);
	frac = front / (front - back);
	frac = bound( 0.0, frac, 1.0 );
#endif
	VectorLerp( start, frac, stop, mid );
	midf = p1f + ( p2f - p1f ) * frac;

	r = TestLine_r( head, tnode->children[side], p1f, midf, start, mid, trace );

	// trace back faces (in case point was inside of brush)
	if( r != CONTENTS_EMPTY )
	{
		if( trace->surface == -1 )
			trace->fraction = midf;
		trace->contents = r;
		return r;
	}

#ifdef HLRAD_TRACE_FACES
	// walk through real faces
	for( i = 0; i < tnode->numfaces; i++ )
	{
		twface_t	*wf = g_world_faces[tnode->firstface + i];
		int	contents;
		vec3_t	delta;

		if( !wf || wf->contents == CONTENTS_SKY )
			continue;

		VectorSubtract( mid, wf->origin, delta );
		if( DotProduct( delta, delta ) >= wf->radius )
			continue;	// no intersection

		for( j = 0; j < wf->numedges; j++ )
		{
			if( PlaneDiff( mid, &wf->edges[j] ) > FRAC_EPSILON )
				break; // outside the bounds
		}

		if( j != wf->numedges )
			continue; // we are outside the bounds of the facet

		// hit the surface
		if( FBitSet( wf->flags, TEX_ALPHATEST ))
		{
			contents = SampleMiptex( wf->original, mid );
			if( contents == CONTENTS_EMPTY )
			{
				// traced through fence
				trace->contents = contents;
				trace->fraction = midf;
				return contents;
			}
		}
		else contents = wf->contents; // sky or solid

		if( contents != CONTENTS_EMPTY )
		{
			// fill the trace and out
			trace->surface = tnode->firstface + i;
			trace->contents = contents;
			trace->fraction = midf;
			return contents;
		}
	}
#endif
	return TestLine_r( head, tnode->children[!side], midf, p2f, mid, stop, trace );
}

/*
====================
ClipToTriangles

Mins and maxs enclose the entire area swept by the move
====================
*/
static void ClipToTriangles( areanode_t *node, moveclip_t *clip )
{
	link_t	*l, *next;
	tface_t	*touch;
loc0:
	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;

		touch = TFACE_FROM_AREA( l );

		if( !BoundsIntersect( clip->boxmins, clip->boxmaxs, touch->absmin, touch->absmax ))
			continue;

		// might intersect, so do an exact clip
		if( clip->trace.contents == CONTENTS_SOLID )
			return;

		ClipMoveToTriangle( touch, clip );
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( clip->boxmaxs[node->axis] > node->dist)
	{
		if( clip->boxmins[node->axis] < node->dist )
			ClipToTriangles( node->children[1], clip );
		node = node->children[0];
		goto loc0;
	}
	else if( clip->boxmins[node->axis] < node->dist )
	{
		node = node->children[1];
		goto loc0;
	}
}

/*
===============
UnlinkEdict

just in case for pair
===============
*/
static void UnlinkEdict( entity_t *ent )
{
	// not linked in anywhere
	if( !ent->area.prev ) return;

	RemoveLink( &ent->area );
	ent->area.prev = NULL;
	ent->area.next = NULL;
	numsolidedicts--;
}

/*
===============
LinkEdict
===============
*/
static void LinkEdict( entity_t *ent, modtype_t modtype, const char *modname, int flags = 0 )
{
	vec3_t		mins, maxs;
	void		*filedata = NULL;
	areanode_t	*node;
	dmodel_t		*bm;

	if( ent->area.prev ) UnlinkEdict( ent );	// unlink from old position
	if( ent == g_entities ) return;		// don't add the world

	// set the origin
	GetVectorForKey( ent, "origin", ent->origin );

	// also check lightorigin for brushmodels
	if( modtype == mod_brush )
	{
		bool	b_light_origin = false;
		bool	b_model_center = false;
		vec3_t	light_origin;
		vec3_t	model_center;
		char	*s;

		// allow models to be lit in an alternate location (pt1)
		if( *( s = ValueForKey( ent, "light_origin" )))
		{
			entity_t	*e = FindTargetEntity( s );

			if( e )
			{
				if( *( s = ValueForKey( e, "origin" )))
				{
					double	v1, v2, v3;

					if( sscanf( s, "%lf %lf %lf", &v1, &v2, &v3 ) == 3 )
					{
						VectorSet( light_origin, v1, v2, v3 );
						b_light_origin = true;
					}
				}
			}
		}

		// allow models to be lit in an alternate location (pt2)
		if( *( s = ValueForKey( ent, "model_center" )))
		{
			double	v1, v2, v3;

			if( sscanf( s, "%lf %lf %lf", &v1, &v2, &v3 ) == 3 )
			{
				VectorSet( model_center, v1, v2, v3 );
				b_model_center = true;
			}
		}

		// allow models to be lit in an alternate location (pt3)
		if( b_light_origin && b_model_center )
		{
			VectorSubtract( light_origin, model_center, ent->origin );
		}
	}

	if (ent->modtype == mod_unknown)
	{
		size_t fileLength = 0;

		// init trace nodes
		switch (modtype)
		{
			case mod_brush:
			{
				bm = ModelForEntity(ent);
				if (!bm) {
					COM_FatalError("LinkEdict: not a inline bmodel!\n");
					return;
				}
				MakeTnodes(ent, bm);
				break;
			}
			default:
			{
				// trying to recognize file format by it's header
				if (!g_nomodelshadow) {
					filedata = FS_LoadFile(modname, &fileLength, false);
				}
				break;
			}
		}

		if (filedata)
		{
			MsgDev(D_NOTE, "Loading model %s\n", modname);
			switch (*(uint32_t*)filedata) // call the apropriate loader
			{
				case IDSTUDIOHEADER:
					LoadStudio(ent, filedata, fileLength, flags);
					break;
				case IDALIASHEADER:
					LoadAlias(ent, filedata, fileLength, flags);
					break;
				default:
				{
					MsgDev(D_ERROR, "%s unknown file format\n", modname);
					Mem_Free(filedata, C_FILESYSTEM);
					break;
				}
			}
		}
		else if (modtype != mod_brush) {
			MsgDev(D_INFO, "^3Warning:^7 Failed to load model %s\n", modname);
		}
	}

	// entity failed to load model
	if( ent->modtype == mod_unknown )
		return;

	if( ent->modtype == mod_brush )
	{
		bm = ModelForEntity( ent );
		// set the abs box
		VectorAdd( ent->origin, bm->mins, ent->absmin );
		VectorAdd( ent->origin, bm->maxs, ent->absmax );
		ExpandBounds( ent->absmin, ent->absmax, 1.0 );
	}
	else if( ent->modtype == mod_studio )
	{
		StudioGetBounds( ent, mins, maxs );
		VectorAdd( ent->origin, mins, ent->absmin );
		VectorAdd( ent->origin, maxs, ent->absmax );
		ExpandBounds( ent->absmin, ent->absmax, 1.0 );
	}
	else if( ent->modtype == mod_alias )
	{
		AliasGetBounds( ent, mins, maxs );
		VectorAdd( ent->origin, mins, ent->absmin );
		VectorAdd( ent->origin, maxs, ent->absmax );
		ExpandBounds( ent->absmin, ent->absmax, 1.0 );
	}
	else
	{
		return;
	}

	// don't link into world if shadow was disabled
	if(( ent->modtype == mod_studio || ent->modtype == mod_alias ) && !FBitSet( flags, FMESH_CAST_SHADOW ) && !FBitSet( flags, FMESH_SELF_SHADOW ))
		return;

#ifdef HLRAD_RAYTRACE
	if( ent->modtype == mod_studio || ent->modtype == mod_alias )
	{
		tmesh_t	*mesh = (tmesh_t *)ent->cache;

		if( !g_studiolegacy )
			mesh->rayBVH.BuildTree( mesh );
		else
		{	
			for( int i = 0; i < mesh->numfaces; i++ )
				mesh->ray.AddTriangle( &mesh->faces[i] );
			mesh->ray.BuildTree( mesh );

			// set backfraction if specified
			mesh->ray.SetBackFraction( FloatForKey( ent, "zhlt_backfrac" ));
		}
	}
#endif
	// find the first node that the ent's box crosses
	node = entity_tree.areanodes;

	while( 1 )
	{
		if( node->axis == -1 ) break;
		if( ent->absmin[node->axis] > node->dist )
			node = node->children[0];
		else if( ent->absmax[node->axis] < node->dist )
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in	
	InsertLinkBefore( &ent->area, &node->solid_edicts );
	numsolidedicts++;
}

dvertex_t	*GetVertexByNumber( int vertexnum )
{
	ASSERT( vertexnum >= 0 && vertexnum < g_numsurfedges );

	int	e = g_dsurfedges[vertexnum];
	dvertex_t	*v;

	if( e >= 0 ) v = &g_dvertexes[g_dedges[e].v[0]];
	else v = &g_dvertexes[g_dedges[-e].v[1]];

	return v;
}

void GetTexCoordsForPoint( const vec3_t point, dtexinfo_t *tex, miptex_t *mt, float stcoord[2] )
{
	float	s, t;

	// texture coordinates
	s = DotProduct( point, tex->vecs[0] ) + tex->vecs[0][3];
	if( mt ) s /= mt->width;

	t = DotProduct( point, tex->vecs[1] ) + tex->vecs[1][3];
	if( mt ) t /= mt->height;

	stcoord[0] = s;
	stcoord[1] = t;
}

/*
===============
BuildWorldFaces

===============
*/
void BuildWorldFaces( void )
{
	// store a list of every face that uses a particular vertex
	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		vec3_t		delta, edgevec;
		const dplane_t	*faceplane;
		byte		*facedata;
		int		contents;
		int		i, size;
		dtexinfo_t	*tx;
		miptex_t		*mt;
		twface_t		*wf;
		const dface_t	*f;

		f = &g_dfaces[facenum];
		tx = &g_texinfo[f->texinfo];
		mt = GetTextureByMiptex( tx->miptex );
		if( mt ) contents = GetFaceContents( mt->name );
		else contents = CONTENTS_SOLID;

		if( IsLiquidContents( contents )/* || contents == CONTENTS_SKY*/ || FBitSet( tx->flags, TEX_NOSHADOW ))
			continue;	// ignore non-lightmapped surfaces

		size = sizeof( twface_t ) + f->numedges * sizeof( dplane_t );
		facedata = (byte *)Mem_Alloc( size );
		wf = (twface_t *)facedata;
		facedata += sizeof( twface_t );
		wf->edges = (dplane_t *)facedata;
		facedata += f->numedges * sizeof( dplane_t );
		g_world_faces[facenum] = wf;
		wf->numedges = f->numedges;
		wf->contents = contents;
		wf->flags = tx->flags;
		wf->original = f;

		if( mt && mt->name[0] == '{' )
			SetBits( wf->flags, TEX_ALPHATEST );

		faceplane = GetPlaneFromFace( facenum );

		// compute face origin and plane edges
		for( i = 0; i < f->numedges; i++ )
		{
			dplane_t	*dest = &wf->edges[i];
			vec3_t	v0, v1;

			VectorCopy( GetVertexByNumber( f->firstedge + i )->point, v0 );
			VectorCopy( GetVertexByNumber( f->firstedge + (i + 1) % f->numedges )->point, v1 );
			VectorSubtract( v1, v0, edgevec );
			CrossProduct( faceplane->normal, edgevec, dest->normal );
			VectorNormalize( dest->normal );
			dest->dist = DotProduct( dest->normal, v0 );
			dest->type = PlaneTypeForNormal( dest->normal );
			VectorAdd( wf->origin, v0, wf->origin );
		}

		VectorScale( wf->origin, 1.0f / f->numedges, wf->origin );

		// compute face radius
		for( i = 0; i < f->numedges; i++ )
		{
			vec3_t	v0;

			VectorCopy( GetVertexByNumber( f->firstedge + i )->point, v0 );
			VectorSubtract( v0, wf->origin, delta );
			vec_t radius = DotProduct( delta, delta );
			wf->radius = Q_max( radius, wf->radius );
		}
	}
}

void FreeWorldFaces( void )
{
	// store a list of every face that uses a particular vertex
	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		Mem_Free( g_world_faces[facenum] );
		g_world_faces[facenum] = NULL;
	}
}

/*
===============
InitWorldTrace

===============
*/
void InitWorldTrace( void )
{
	int	i, j;

	memset( &entity_tree, 0, sizeof( entity_tree ));

	CreateAreaNode( &entity_tree, 0, AREA_MIN_DEPTH, g_dmodels[0].mins, g_dmodels[0].maxs );
	BuildWorldFaces();			// init world faces
	MakeTnodes( g_entities, g_dmodels );	// init world nodes

	// link entities into world
	for( i = 1; i < g_numentities; i++ )
	{
		entity_t	*e = &g_entities[i];
		const char *c = ValueForKey( e, "classname" );
		const char *t = ValueForKey( e, "model" );
		int	flags = 0;

		if( !*t ) continue;

		// drop to floor env_static's
		if( !Q_strcmp( c, "env_static" ))
		{
			int	spawnflags = IntForKey( e, "spawnflags" );

			// drop static to the floor
			if( FBitSet( spawnflags, BIT( 1 )))
			{
				vec3_t	end;

				// set the origin
				GetVectorForKey( e, "origin", e->origin );
				VectorCopy( e->origin, end );

				if( PointInLeaf( end )->contents != CONTENTS_SOLID )
				{
					// delicate drop-to-floor
					for( j = 0; j < 256; j++ )
					{
						if( PointInLeaf( end )->contents == CONTENTS_SOLID )
							break; // we hit the floor
						end[2] -= 1.0f;
					}

					if( j != 256 )
					{
						SetVectorForKey( e, "origin", end, true );
						MsgDev( D_REPORT, "%s origin adjusted to -%g units\n", t, e->origin[2] - end[2] );
						ClearBits( spawnflags, BIT( 1 ));
						SetKeyValue( e, "spawnflags", va( "%i", spawnflags ));
					}
				}
			}
		}
	}
#ifdef HLRAD_RAYTRACE
	int dispatch = 0, total = 0;
	double start = I_FloatTime();
	if( g_studiolegacy )
		Msg( "Build KD-Trees:\n" );
	else
		Msg( "Build BVH-Trees:\n" );
	StartPacifier();

	for( i = 1; i < g_numentities; i++ )
	{
		entity_t	*e = &g_entities[i];
		const char *c = ValueForKey( e, "classname" );
		const char *t = ValueForKey( e, "model" );
		int	flags = 0;

		if( !*t ) continue;

		// both support for quake and half-life
		if( !Q_strcmp( c, "env_static" ) || !Q_strcmp( c, "misc_model" ))
		{
			int	spawnflags = IntForKey( e, "spawnflags" );

			if( !FBitSet( spawnflags, BIT( 2 )))
				total++;
		}
		else if( BoolForKey( e, "zhlt_modelshadow" ) || BoolForKey( e, "zhlt_vertexlight" ))
		{
			if( BoolForKey( e, "zhlt_modelshadow" ))
				total++;
		}
	}
#endif
	// link entities into world
	for( i = 1; i < g_numentities; i++ )
	{
		entity_t	*e = &g_entities[i];
		const char *c = ValueForKey( e, "classname" );
		const char *t = ValueForKey( e, "model" );
		int	flags = 0;

		if( !*t ) continue;

		// both support for quake and half-life
		if( !Q_strcmp( c, "env_static" ) || !Q_strcmp( c, "misc_model" ))
		{
			int	spawnflags = IntForKey( e, "spawnflags" );

			if( !FBitSet( spawnflags, BIT( 2 )))		// spawnflag 4 - disable the shadows
				SetBits( flags, FMESH_CAST_SHADOW );
			if( BoolForKey( e, "zhlt_modellightmap" ))
				SetBits( flags, FMESH_MODEL_LIGHTMAPS );
			else if( !FBitSet( spawnflags, BIT( 3 )))	// spawnflag 8 - disable the vertex lighting
				SetBits( flags, FMESH_VERTEX_LIGHTING );
			if( BoolForKey( e, "zhlt_selfshadow" ))
				SetBits( flags, FMESH_SELF_SHADOW );
			if( BoolForKey( e, "zhlt_nosmooth" ))
				SetBits( flags, FMESH_DONT_SMOOTH );
			LinkEdict( e, mod_unknown, t, flags );
		}
		else if( BoolForKey( e, "zhlt_modelshadow" ) || BoolForKey( e, "zhlt_vertexlight" ))
		{
			// NOTE: compatibility case for GoldSrc
			if( BoolForKey( e, "zhlt_modellightmap" ))
				SetBits( flags, FMESH_MODEL_LIGHTMAPS );
			else if( BoolForKey( e, "zhlt_vertexlight" ))
				SetBits( flags, FMESH_VERTEX_LIGHTING );
			if( BoolForKey( e, "zhlt_modelshadow" ))
				SetBits( flags, FMESH_CAST_SHADOW );
			if( BoolForKey( e, "zhlt_selfshadow" ))
				SetBits( flags, FMESH_SELF_SHADOW );
			if( BoolForKey( e, "zhlt_nosmooth" ))
				SetBits( flags, FMESH_DONT_SMOOTH );
			LinkEdict( e, mod_unknown, t, flags );
		}
		else if( *t == '*' )
		{
			int	flags = IntForKey( e, "zhlt_lightflags" );
			int	shadow = IntForKey( e, "_shadow" );

			if( FBitSet( flags, BIT( 1 )) || shadow == 1 )
				LinkEdict( e, mod_brush, t );
		}
#ifdef HLRAD_RAYTRACE
		if(( e->modtype == mod_studio || e->modtype == mod_alias ) && FBitSet( flags, FMESH_CAST_SHADOW ))
		{
			dispatch++;
			UpdatePacifier( (float)dispatch / total );
		}
#endif
	}
#ifdef HLRAD_RAYTRACE
	double end = I_FloatTime();
	EndPacifier( end - start );
#endif
}

void FreeWorldTrace( void )
{
	FreeWorldFaces();
}

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/
/*
==================
ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
void ClipMoveToEntity( entity_t *ent, const vec3_t start, const vec3_t end, trace_t *trace, bool stop_on_first_solid = false )
{
	trace->contents = CONTENTS_EMPTY;
	trace->fraction = 1.0f;
	trace->surface = -1;

	if( ent->modtype == mod_studio || ent->modtype == mod_alias )
	{
		moveclip_t	clip;

		clip.mesh = (tmesh_t *)ent->cache;
		clip.trace.contents = CONTENTS_EMPTY;
		clip.trace.fraction = 1.0f;
		clip.trace.surface = -1;
		clip.start = start;
		clip.end = end;

		MoveBounds( start, end, clip.boxmins, clip.boxmaxs );
		VectorSubtract( end, start, clip.direction );
		clip.distance = VectorNormalize( clip.direction );

		// run through studio triangles
#ifdef HLRAD_RAYTRACE
		if( !g_studiolegacy )
			clip.mesh->rayBVH.TraceRay( start, end, &clip.trace, stop_on_first_solid );
		else
			clip.mesh->ray.TraceRay( start, end, &clip.trace );
#else
		ClipToTriangles( clip.mesh->face_tree.areanodes, &clip );
#endif
		trace->contents = clip.trace.contents;
		trace->fraction = clip.trace.fraction;

		//studio gi
		if( clip.trace.surface == STUDIO_SURFACE_HIT )
		{	
			trace->surface = clip.trace.surface;
			for( int i = 0; i < MAXLIGHTMAPS; i++ )
			{
				trace->styles[i] = clip.trace.styles[i];
				VectorCopy( clip.trace.light[i], trace->light[i] );
			}
		}

	}
	else if( ent->modtype == mod_brush )
	{
		vec3_t	start_l, end_l;

		VectorSubtract( start, ent->origin, start_l );
		VectorSubtract( end, ent->origin, end_l );
		trace->fraction = 0.0f;

		TestLine_r((tnode_t *)ent->cache, 0, 0.0, 1.0, start_l, end_l, trace );
	}
	else
	{
		COM_FatalError( "ClipMoveToEntity: unknown model type\n" );
	}
}

/*
====================
ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
static void ClipToLinks( areanode_t *node, moveclip_t *clip, bool stop_on_first_solid = true )
{
	link_t	*l, *next;
	entity_t	*touch;
	trace_t	trace;
loc0:
	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;

		touch = ENTITY_FROM_AREA( l );

		//hack for self shadowing without casting shadows
		if( touch->modtype == mod_studio || touch->modtype == mod_alias )
		{
			tmesh_t	*mesh = (tmesh_t *)touch->cache;

			if( touch == clip->ignore )
			{
				if( !FBitSet( mesh->flags, FMESH_SELF_SHADOW ) )
					continue;
			}
			else
				if( !FBitSet( mesh->flags, FMESH_CAST_SHADOW ) )
					continue;			
		}
		else
			if( touch == clip->ignore )
				continue;

		if( !BoundsIntersect( clip->boxmins, clip->boxmaxs, touch->absmin, touch->absmax ))
			continue;

		// might intersect, so do an exact clip
		if(( clip->trace.contents == CONTENTS_SOLID )&&(stop_on_first_solid))
			return;

		if( clip->nomodels && ( touch->modtype == mod_studio || touch->modtype == mod_alias ))
			continue;

		ClipMoveToEntity( touch, clip->start, clip->end, &trace, stop_on_first_solid );

		clip->trace = CombineTraces( &clip->trace, &trace );
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( clip->boxmaxs[node->axis] > node->dist)
	{
		if( clip->boxmins[node->axis] < node->dist )
			ClipToLinks( node->children[1], clip, stop_on_first_solid );
		node = node->children[0];
		goto loc0;
	}
	else if( clip->boxmins[node->axis] < node->dist )
	{
		node = node->children[1];
		goto loc0;
	}
}

/*
==================
TraceLine
==================
*/
int TestLine( int threadnum, const vec3_t start, const vec3_t end, bool nomodels, entity_t *ignoreent )
{
	moveclip_t	clip;

	clip.trace.contents = CONTENTS_EMPTY;
	clip.trace.fraction = 0.0f;
	clip.trace.surface = -1;

	// trace world first
	TestLine_r( (tnode_t *)g_entities->cache, 0, 0.0f, 1.0f, start, end, &clip.trace );

	// run through entities (bmodels, studiomodels)
	if(( numsolidedicts > 0 ) && ( clip.trace.fraction != 0.0f ))
	{
		float	trace_fraction;
		vec3_t	trace_endpos;

		VectorLerp( start, clip.trace.fraction, end, trace_endpos );
		trace_fraction = clip.trace.fraction;
		clip.trace.fraction = 1.0f;
		clip.nomodels = nomodels;
		clip.start = start;
		clip.end = trace_endpos;
		clip.ignore = ignoreent;

		MoveBounds( start, trace_endpos, clip.boxmins, clip.boxmaxs );
		ClipToLinks( entity_tree.areanodes, &clip );

		clip.trace.fraction *= trace_fraction;
	}

	return clip.trace.contents;
}

/*
==================
TestLine

trace world only
==================
*/
void TestLine( int threadnum, const vec3_t start, const vec3_t stop, trace_t *trace, bool nomodels, entity_t *ignoreent )
{
	trace->contents = CONTENTS_EMPTY;
	trace->fraction = 0.0f;
	trace->surface = -1;

	// trace world first
	TestLine_r( (tnode_t *)g_entities->cache, 0, 0.0f, 1.0f, start, stop, trace );
	
	moveclip_t	clip;
	if(( numsolidedicts > 0 ) && ( trace->fraction != 0.0f ))
	{
		vec3_t	trace_endpos;

		VectorLerp( start, trace->fraction, stop, trace_endpos );
		clip.trace.contents = CONTENTS_EMPTY;
		clip.trace.surface = trace->surface;
		clip.trace.fraction = 1.0f;
		clip.nomodels = nomodels;
		clip.start = start;
		clip.end = trace_endpos;
		clip.ignore = ignoreent;

		MoveBounds( start, trace_endpos, clip.boxmins, clip.boxmaxs );
		ClipToLinks( entity_tree.areanodes, &clip, false );

		if( clip.trace.contents == CONTENTS_EMPTY )
			return;

		trace->contents = clip.trace.contents;
		trace->fraction *= clip.trace.fraction;
		trace->surface = clip.trace.surface;

		//studio gi
		if( clip.trace.surface == STUDIO_SURFACE_HIT )
			for( int i = 0; i < MAXLIGHTMAPS; i++ )
			{
				trace->styles[i] = clip.trace.styles[i];
				VectorCopy( clip.trace.light[i], trace->light[i] );
			}
	}	
}


void BuildDiffuseNormals( void )
{
	//fibonacci spiral
	g_numskynormals[0] = 0;
	g_skynormals[0] = NULL; //don't use this

	int numpoints = 1;
	vec_t goldenRatio = (1.0 + sqrt(5.0)) / 2.0;

	for( int skylevel = 1; skylevel <= SKYLEVELMAX; skylevel++ )
	{
		numpoints *= 4;
		g_numskynormals[skylevel] = numpoints;
		g_skynormals[skylevel] = (vec3_t *)Mem_Alloc( numpoints * sizeof( vec3_t ));

		for( int i = 0; i < numpoints; i++ )
		{
			vec_t theta = 2.0f * M_PI * (float)i / goldenRatio;
 			vec_t phi = AcosFast(1.0f - 2.0f * ((float)i + 0.5f) / (float)numpoints);

			g_skynormals[skylevel][i][0] = cos(theta) * sin(phi);
			g_skynormals[skylevel][i][1] = sin(theta) * sin(phi);
			g_skynormals[skylevel][i][2] = cos(phi);
		}
	}	
}

void FreeDiffuseNormals( void )
{
	for( int i = 0; i <= SKYLEVELMAX; i++ )
	{
		Mem_Free( g_skynormals[i] );
		g_skynormals[i] = NULL;
	}
}
