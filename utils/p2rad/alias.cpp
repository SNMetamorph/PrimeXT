/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// alias.c

#include "qrad.h"
#include "..\..\engine\alias.h"
#include "model_trace.h"

static trivertex_t	*g_poseverts[MAXALIASFRAMES];
static stvert_t	g_stverts[MAXALIASVERTS];
static dtriangle_t	g_triangles[MAXALIASTRIS];
static int	g_posenum;

void *LoadSingleSkin( daliasskintype_t *pskintype, int size )
{
	return ((byte *)(pskintype + 1) + size);
}

void *LoadGroupSkin( daliasskintype_t *pskintype, int size )
{
	daliasskininterval_t	*pinskinintervals;
	daliasskingroup_t		*pinskingroup;
	int			i;

	// animating skin group.  yuck.
	pskintype++;
	pinskingroup = (daliasskingroup_t *)pskintype;
	pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);
	pskintype = (daliasskintype_t *)(pinskinintervals + pinskingroup->numskins);

	for( i = 0; i < pinskingroup->numskins; i++ )
		pskintype = (daliasskintype_t *)((byte *)(pskintype) + size);
	return pskintype;
}

/*
===============
LoadAllSkins
===============
*/
void *LoadAllSkins( daliashdr_t *phdr, daliasskintype_t *pskintype )
{
	int	i, size;

	size = phdr->skinwidth * phdr->skinheight;

	for( i = 0; i < phdr->numskins; i++ )
	{
		if( pskintype->type == ALIAS_SKIN_SINGLE )
			pskintype = (daliasskintype_t *)LoadSingleSkin( pskintype, size );
		else pskintype = (daliasskintype_t *)LoadGroupSkin( pskintype, size );
	}

	return (void *)pskintype;
}

/*
=================
LoadAliasFrame
=================
*/
void *LoadAliasFrame( daliashdr_t *phdr, void *pin )
{
	daliasframe_t	*pdaliasframe;
	trivertex_t	*pinframe;

	pdaliasframe = (daliasframe_t *)pin;
	pinframe = (trivertex_t *)(pdaliasframe + 1);

	g_poseverts[g_posenum] = (trivertex_t *)pinframe;
	pinframe += phdr->numverts;
	g_posenum++;

	return (void *)pinframe;
}

/*
=================
LoadAliasGroup
=================
*/
void *LoadAliasGroup( daliashdr_t *phdr, void *pin )
{
	daliasgroup_t	*pingroup;
	int		i, numframes;
	daliasinterval_t	*pin_intervals;
	void		*ptemp;

	pingroup = (daliasgroup_t *)pin;
	numframes = pingroup->numframes;

	pin_intervals = (daliasinterval_t *)(pingroup + 1);
	pin_intervals += numframes;
	ptemp = (void *)pin_intervals;

	for( i = 0; i < numframes; i++ )
	{
		g_poseverts[g_posenum] = (trivertex_t *)((daliasframe_t *)ptemp + 1);
		ptemp = (trivertex_t *)((daliasframe_t *)ptemp + 1) + phdr->numverts;
		g_posenum++;
	}

	return ptemp;
}

static void AliasSetupTriangle( tmesh_t *mesh, int index )
{
	tface_t	*face = &mesh->faces[index];

	// calculate mins & maxs
	ClearBounds( face->absmin, face->absmax );
	face->contents = CONTENTS_SOLID;

	// calc bounds
	AddPointToBounds( mesh->verts[face->a].point, face->absmin, face->absmax );
	AddPointToBounds( mesh->verts[face->a].point, mesh->absmin, mesh->absmax );
	AddPointToBounds( mesh->verts[face->b].point, face->absmin, face->absmax );
	AddPointToBounds( mesh->verts[face->b].point, mesh->absmin, mesh->absmax );
	AddPointToBounds( mesh->verts[face->c].point, face->absmin, face->absmax );
	AddPointToBounds( mesh->verts[face->c].point, mesh->absmin, mesh->absmax );
	ExpandBounds( face->absmin, face->absmax, 1.0 );

	// alias always have single texture on a mesh
	face->texture = mesh->textures;
	// setup efficiency intersection data
	face->PrepareIntersectionData( mesh->verts );
}

static void AliasRelinkFace( aabb_tree_t *tree, tface_t *face )
{
	// only shadow faces must be linked
	if( !face->shadow ) return;

	// find the first node that the facet box crosses
	areanode_t	*node = tree->areanodes;

	while( 1 )
	{
		if( node->axis == -1 ) break;
		if( face->absmin[node->axis] > node->dist )
			node = node->children[0];
		else if( face->absmax[node->axis] < node->dist )
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in	
	InsertLinkBefore( &face->area, &node->solid_edicts );
}

void LoadAlias( entity_t *ent, void *extradata, long fileLength, int flags )
{
	const char	*modname = ValueForKey( ent, "model" );
	int		keyframe = IntForKey( ent, "frame" );
	double		start = I_FloatTime();
	dword		modelCRC = 0;
	daliashdr_t	*phdr;
	stvert_t		*pinstverts;
	dtriangle_t	*pintriangles;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	trivertex_t	*poseverts;
	int		i, j;

	// alias doesn't allow these features
	ClearBits( flags, FMESH_VERTEX_LIGHTING|FMESH_MODEL_LIGHTMAPS );

	if( !extradata )
	{
		MsgDev( D_WARN, "LoadAlias: couldn't load %s\n", modname );
		return;
	}

	phdr = (daliashdr_t *)extradata;
	i = phdr->version;

	if( i != ALIAS_VERSION )
	{
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)\n", modname, i, ALIAS_VERSION );
		Mem_Free( extradata, C_FILESYSTEM );
		return;
	}

	if( phdr->numframes < 1 )
	{
		MsgDev( D_ERROR, "%s has invalid # of frames: %d\n", modname, phdr->numframes );
		Mem_Free( extradata, C_FILESYSTEM );
		return;
	}

	if( keyframe < 0 || keyframe >= phdr->numframes )
	{
		MsgDev( D_WARN, "%s specified invalid frame: %d\n", modname, keyframe );
		keyframe = 0;
	}

	if( phdr->numverts <= 0 )
	{
		MsgDev( D_ERROR, "model %s has no vertices\n", modname );
		Mem_Free( extradata, C_FILESYSTEM );
		return;
	}

	if( phdr->numverts > MAXALIASVERTS )
	{
		MsgDev( D_ERROR, "model %s has too many vertices\n", modname );
		Mem_Free( extradata, C_FILESYSTEM );
		return;
	}

	if( phdr->numtris <= 0 )
	{
		MsgDev( D_ERROR, "model %s has no triangles\n", modname );
		Mem_Free( extradata, C_FILESYSTEM );
		return;
	}

	if( phdr->numskins < 1 || phdr->numskins > MAX_SKINS )
	{
		MsgDev( D_ERROR, "model %s has invalid # of skins: %d\n", modname, phdr->numskins );
		Mem_Free( extradata, C_FILESYSTEM );
		return;
	}

	// load the skins
	pskintype = (daliasskintype_t *)&phdr[1];
	pskintype = (daliasskintype_t *)LoadAllSkins( phdr, pskintype );

	// load base s and t vertices
	pinstverts = (stvert_t *)pskintype;

	for( i = 0; i < phdr->numverts; i++ )
	{
		g_stverts[i].onseam = pinstverts[i].onseam;
		g_stverts[i].s = pinstverts[i].s;
		g_stverts[i].t = pinstverts[i].t;
	}

	// load triangle lists
	pintriangles = (dtriangle_t *)&pinstverts[phdr->numverts];

	for( i = 0; i < phdr->numtris; i++ )
	{
		g_triangles[i].facesfront = pintriangles[i].facesfront;

		for( j = 0; j < 3; j++ )
			g_triangles[i].vertindex[j] = pintriangles[i].vertindex[j];
	}

	// load the frames
	pframetype = (daliasframetype_t *)&pintriangles[phdr->numtris];
	g_posenum = 0;

	for( i = 0; i < phdr->numframes; i++ )
	{
		aliasframetype_t	frametype = pframetype->type;

		if( frametype == ALIAS_SINGLE )
			pframetype = (daliasframetype_t *)LoadAliasFrame( phdr, pframetype + 1 );
		else pframetype = (daliasframetype_t *)LoadAliasGroup( phdr, pframetype + 1 );
	}

	size_t	memsize = sizeof( tmesh_t ) + ( sizeof( tface_t ) * phdr->numtris ) + sizeof( timage_t ) + sizeof( tvert_t ) * phdr->numverts;
	byte	*meshdata = (byte *)Mem_Alloc( memsize );
	byte	*meshend = meshdata + memsize;
	tmesh_t	*mesh = (tmesh_t *)meshdata;
	vec3_t	origin, angles, point, normal;
	matrix3x4	transform;
	vec3_t	xform;
	float	scale;

	if( ent->cache ) Mem_Free( ent->cache ); // throw previous instance
	ent->cache = meshdata; // FreeEntities will be automatically free that

	// setup pointers
	meshdata += sizeof( tmesh_t );
	mesh->faces = (tface_t *)meshdata;
	meshdata += sizeof( tface_t ) * phdr->numtris;
	mesh->textures = (timage_t *)meshdata;
	meshdata += sizeof( timage_t );
	mesh->verts = (tvert_t *)meshdata;
	meshdata += sizeof( tvert_t ) * phdr->numverts;
	mesh->numverts = phdr->numverts;

	if( meshdata != meshend )
		Msg( "%s memory corrupted\n", modname );

	// mesh->submodels aren't used because alias model doesn't have the submodels
	for( int l = 0; l < MAXLIGHTMAPS; l++ )
		mesh->styles[l] = 255;

	// select the specified frame
	poseverts = g_poseverts[keyframe];
 	ASSERT( poseverts != NULL );
 
	ClearBounds( mesh->absmin, mesh->absmax );

	// just in case, not needs
	mesh->textures->width = phdr->skinwidth;
	mesh->textures->height = phdr->skinheight;

	// get model properties
	GetVectorForKey( ent, "origin", origin );
	GetVectorForKey( ent, "angles", angles );

	angles[0] = -angles[0]; // Stupid quake bug workaround
	scale = FloatForKey( ent, "scale" );
	GetVectorForKey( ent, "xform", xform );
	if( VectorIsNull( xform ))
		VectorFill( xform, scale );

	// check xform values
	if( xform[0] < 0.01f ) xform[0] = 1.0f;
	if( xform[1] < 0.01f ) xform[1] = 1.0f;
	if( xform[2] < 0.01f ) xform[2] = 1.0f;
	if( xform[0] > 16.0f ) xform[0] = 16.0f;
	if( xform[1] > 16.0f ) xform[1] = 16.0f;
	if( xform[2] > 16.0f ) xform[2] = 16.0f;

	Matrix3x4_CreateFromEntityScale3f( transform, angles, origin, xform );

	// fill the verts
	for( i = 0; i < phdr->numverts; i++ )
	{
		stvert_t		*st = &g_stverts[i];
		trivertex_t	*v = &poseverts[i];
		tvert_t		*tv = &mesh->verts[i];
		float		s, t;

		point[0] = (float)v->v[0] * phdr->scale[0] + phdr->scale_origin[0];
		point[1] = (float)v->v[1] * phdr->scale[1] + phdr->scale_origin[1];
		point[2] = (float)v->v[2] * phdr->scale[2] + phdr->scale_origin[2];
		Matrix3x4_VectorTransform( transform, point, tv->point );

		// compute lightvert normal
		VectorCopy( g_anorms[v->lightnormalindex], point );
		Matrix3x4_VectorRotate( transform, point, normal );
		VectorNormalize2( normal );
		VectorCopy( normal, tv->normal );

		s = st->s;
		t = st->t;

#if 0		// FIXME: unreachable. but who cares?
		if( !tri->facesfront && st->onseam )
			s += phdr->skinwidth / 2;	// on back side
#endif
		tv->st[0] = (s + 0.5f) / phdr->skinwidth;
		tv->st[1] = (t + 0.5f) / phdr->skinheight;
	}

	// fill the triangles
	for( i = 0; i < phdr->numtris; i++ )
	{
		dtriangle_t	*tri = &g_triangles[i];
		tface_t		*tf = &mesh->faces[i];

		tf->a = tri->vertindex[0];
		tf->b = tri->vertindex[1];
		tf->c = tri->vertindex[2];
		tf->shadow = true;	// used for trace
	}

	// do some post-initialization
	for( i = 0; i < phdr->numtris; i++ )
		AliasSetupTriangle( mesh, i );

	// create AABB tree for speedup reasons
	CreateAreaNode( &mesh->face_tree, 0, AREA_MIN_DEPTH, mesh->absmin, mesh->absmax );

	for( i = 0; i < phdr->numtris; i++ )
		AliasRelinkFace( &mesh->face_tree, &mesh->faces[i] );

	Mem_Free( extradata, C_FILESYSTEM );
	ent->modtype = mod_alias; // now our mesh is valid and ready to trace
	mesh->backfrac = FloatForKey( ent, "zhlt_backfrac" );
	mesh->flags = flags; // copy settings
	mesh->modelCRC = modelCRC;

	MsgDev( D_REPORT, "%s: build time %g secs, size %s\n", modname, I_FloatTime() - start, Q_memprint( memsize ));
}

void AliasGetBounds( entity_t *ent, vec3_t mins, vec3_t maxs )
{
	tmesh_t	*mesh = (tmesh_t *)ent->cache;

	// assume point hull
	VectorClear( mins );
	VectorClear( maxs );

	if( !mesh || ent->modtype != mod_alias )
		return;

	VectorSubtract( mesh->absmin, ent->origin, mins );
	VectorSubtract( mesh->absmax, ent->origin, maxs );
}