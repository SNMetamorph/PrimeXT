/*
r_world.cpp - world and submodels rendering code
Copyright (C) 2018 Uncle Mike

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
#include "pm_movevars.h"
#include "xash3d_features.h"
#include "mathlib.h"
#include "r_shader.h"
#include "r_world.h"

static gl_world_t	worlddata;
gl_world_t *world = &worlddata;

/*
==================
Mod_SampleSizeForFace

return the current lightmap resolution per face
==================
*/
int Mod_SampleSizeForFace( msurface_t *surf )
{
	if( !surf || !surf->texinfo )
		return LM_SAMPLE_SIZE;

	// world luxels has more priority
	if( FBitSet( surf->texinfo->flags, TEX_WORLD_LUXELS ))
		return 1;

	if( FBitSet( surf->texinfo->flags, TEX_EXTRA_LIGHTMAP ))
		return LM_SAMPLE_EXTRASIZE;

	if( surf->texinfo->faceinfo )
		return surf->texinfo->faceinfo->texture_step;

	return LM_SAMPLE_SIZE;
}

/*
=============================================================

	WORLD LOADING

=============================================================
*/
/*
========================
Mod_LoadWorldMaterials

build a material for each world texture
========================
*/
void Mod_LoadWorldMaterials( void )
{
	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		texture_t *tx = worldmodel->textures[i];
		char	diffuse[128], luma[128];

		// bad texture? 
		if( !tx || !tx->name[0] )
			continue;

		if( tx->gl_texturenum == tr.defaultTexture )
			continue;	// don't replace default

		// build material names
		Q_snprintf( diffuse, sizeof( diffuse ), "textures/%s", tx->name );

		if( IMAGE_EXISTS( diffuse ))
		{
			int	texture_ext = LOAD_TEXTURE( diffuse, NULL, 0, 0 );
			int	encodeType = RENDER_GET_PARM( PARM_TEX_ENCODE, texture_ext );

			// NOTE: default renderer can't unpack encoded textures
			// so keep lowres copies for this case
			if( encodeType == DXT_ENCODE_DEFAULT )
			{
				FREE_TEXTURE( tx->gl_texturenum ); // release wad-texture
				tx->gl_texturenum = texture_ext;
			}
			else
			{
				// can't use encoded textures
				FREE_TEXTURE( texture_ext );
			}
		}

		// build material names
		Q_snprintf( luma, sizeof( luma ), "textures/%s_luma", tx->name );

		if( IMAGE_EXISTS( luma ))
		{
			int	texture_ext = LOAD_TEXTURE( luma, NULL, 0, 0 );
			int	encodeType = RENDER_GET_PARM( PARM_TEX_ENCODE, texture_ext );

			// NOTE: default renderer can't unpack encoded textures
			// so keep lowres copies for this case
			if( encodeType == DXT_ENCODE_DEFAULT )
			{
				tx->fb_texturenum = texture_ext;
			}
			else
			{
				// can't use encoded textures
				FREE_TEXTURE( texture_ext );
			}
		}

		if( !Q_strncmp( tx->name, "sky", 3 ))
			SetBits( world->features, WORLD_HAS_SKYBOX );
	}
}

/*
=================
Mod_SetParent
=================
*/
static void Mod_SetParent( mworldnode_t *node, mworldnode_t *parent )
{
	node->parent = parent;

	if( node->contents < 0 ) return; // it's leaf

	ASSERT( node->children[0] != NULL );
	ASSERT( node->children[1] != NULL );

	Mod_SetParent( node->children[0], node );
	Mod_SetParent( node->children[1], node );
}

/*
==================
CountClipNodes_r
==================
*/
static void Mod_CountNodes_r( mnode_t *node )
{
	if( node->contents < 0 ) return; // leaf

	world->numnodes++;

	Mod_CountNodes_r( node->children[0] );
	Mod_CountNodes_r( node->children[1] );
}

/*
=================
Mod_SetupWorldLeafs
=================
*/
void Mod_SetupWorldLeafs( void )
{
	mleaf_t 		*in = worldmodel->leafs;
	mworldleaf_t	*out;

	world->numleafs = worldmodel->numleafs + 1; // world leafs + outside common leaf 
	world->leafs = out = (mworldleaf_t *)Mem_Alloc( sizeof( mworldleaf_t ) * world->numleafs );

	for( int i = 0; i < world->numleafs; i++, in++, out++ )
	{
		out->contents = in->contents;
		out->mins = Vector( in->minmaxs + 0 );
		out->maxs = Vector( in->minmaxs + 3 );
		out->efrags = &in->efrags;
		out->firstmarksurface = in->firstmarksurface;
		out->nummarksurfaces = in->nummarksurfaces;
		out->cluster = in->cluster; // helper to acess to uncompressed visdata
		out->fogDensity = in->ambient_sound_level[AMBIENT_SKY];
	}
}

/*
=================
Mod_SetupWorldNodes
=================
*/
void Mod_SetupWorldNodes( void )
{
	mnode_t 		*in = worldmodel->nodes;
	mworldnode_t	*out;
	int		p;

	Mod_CountNodes_r( worldmodel->nodes ); // determine worldnode count
	world->nodes = out = (mworldnode_t *)Mem_Alloc( sizeof( mworldnode_t ) * world->numnodes );

	for( int i = 0; i < world->numnodes; i++, in++, out++ )
	{
		out->mins = Vector( in->minmaxs + 0 );
		out->maxs = Vector( in->minmaxs + 3 );

		out->plane = in->plane;
		out->firstsurface = in->firstsurface;
		out->numsurfaces = in->numsurfaces;

		for( int j = 0; j < 2; j++ )
		{
			if( in->children[j]->contents < 0 )
			{
				p = (mleaf_t *)in->children[j] - worldmodel->leafs;
				ASSERT( p >= 0 && p < world->numleafs );
				out->children[j] = (mworldnode_t *)(world->leafs + p);
			}
			else
			{
				p = (mnode_t *)in->children[j] - worldmodel->nodes;
				ASSERT( p >= 0 && p < world->numnodes );
				out->children[j] = (mworldnode_t *)(world->nodes + p);
			}
			ASSERT( out->children[j] != NULL );
		}
	}

	Mod_SetParent( world->nodes, NULL );
}

/*
=================
Mod_LinkLeafLights
=================
*/
static void Mod_LinkLeafLights( void )
{
	mworldleaf_t	*leaf;
	int		i, j;
	mlightprobe_t	*lp;

	leaf = (mworldleaf_t *)world->leafs;
	lp = world->leaflights;

	for( i = j = 0; i < world->numleafs; i++, leaf++ )
	{
		if( world->numleaflights > 0 && lp->leaf == leaf )
		{
			leaf->ambient_light = lp; // pointer to first light in the array that belong to this leaf

			for( ;( j < world->numleaflights ) && (lp->leaf == leaf); j++, lp++ )
				leaf->num_lightprobes++;
		}
	}	
}

/*
=================
Mod_LoadVertNormals
=================
*/
static void Mod_LoadVertNormals( const byte *base, const dlump_t *l )
{
	dnormallump_t	*nhdr;
	dnormal_t		*in;
	byte		*data;
	int		count;

	if( !l->filelen ) return;

	data = (byte *)(base + l->fileofs);
	nhdr = (dnormallump_t *)data;

	// indexed normals
	if( nhdr->ident == NORMIDENT )
	{
		int table_size = worldmodel->numsurfedges * sizeof( dvertnorm_t );
		int data_size = nhdr->numnormals * sizeof( dnormal_t );
		int total_size = sizeof( dnormallump_t ) + table_size + data_size;

		if( l->filelen != total_size )
			HOST_ERROR( "Mod_LoadVertNormals: funny lump size\n" );

		data += sizeof( dnormallump_t );

		// alloc remap table
		world->surfnormals = (dvertnorm_t *)Mem_Alloc( table_size );
		memcpy( world->surfnormals, data, table_size );
		data += table_size;

		// copy normals data
		world->normals = (dnormal_t *)Mem_Alloc( data_size );
		memcpy( world->normals, data, data_size );
		world->numnormals = nhdr->numnormals;
		ALERT( at_aiconsole, "total %d packed normals\n", world->numnormals );
	}
	else
	{
		// old method
		in = (dnormal_t *)(base + l->fileofs);

		if( l->filelen % sizeof( *in ))
			HOST_ERROR( "Mod_LoadVertNormals: funny lump size\n" );
		count = l->filelen / sizeof( *in );

		// all the other counts are invalid
		if( count == worldmodel->numvertexes )
		{
			world->normals = (dnormal_t *)Mem_Alloc( count * sizeof( dnormal_t ));
			memcpy( world->normals, in, count * sizeof( dnormal_t ));
		}
	}
}

/*
=================
Mod_LoadVertexLighting
=================
*/
static void Mod_LoadVertexLighting( const byte *base, const dlump_t *l )
{
	dvlightlump_t	*vl;

	if( !l->filelen ) return;
	
	vl = (dvlightlump_t *)(base + l->fileofs);

	if( vl->ident != VLIGHTIDENT )
		return; // probably it's LUMP_LEAF_LIGHTING

	if( vl->version != VLIGHT_VERSION )
		return; // old version?

	if( vl->nummodels <= 0 ) return;

	world->vertex_lighting = (dvlightlump_t *)Mem_Alloc( l->filelen );
	memcpy( world->vertex_lighting, vl, l->filelen );
}

/*
=================
Mod_LoadLeafAmbientLighting

and link into leafs
=================
*/
static void Mod_LoadLeafAmbientLighting( const byte *base, const dlump_t *l )
{
	dleafsample_t	*in;
	dvlightlump_t	*vl;
	mlightprobe_t	*out;
	int		i, count;

	if( !l->filelen ) return;

	vl = (dvlightlump_t *)(base + l->fileofs);

	if( vl->ident == VLIGHTIDENT )
	{
		// probably it's LUMP_VERTEX_LIGHTING
		Mod_LoadVertexLighting( base, l );
		return;
	}

	in = (dleafsample_t *)(base + l->fileofs);
	if( l->filelen % sizeof( *in ))
	{
		ALERT( at_warning, "Mod_LoadLeafAmbientLighting: funny lump size in %s\n", world->name );
		return;
	}
	count = l->filelen / sizeof( *in );

	world->leaflights = out = (mlightprobe_t *)Mem_Alloc( count * sizeof( *out ));
	world->numleaflights = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		memcpy( &out->cube, &in->ambient, sizeof( dlightcube_t ));
		out->leaf = &world->leafs[in->leafnum];
		out->origin.x = in->origin[0];
		out->origin.y = in->origin[1];
		out->origin.z = in->origin[2];
	}
}

/*
=================
Mod_SurfaceCompareBuild

sort faces before lightmap building
=================
*/
static int Mod_SurfaceCompareBuild( const unsigned short **a, const unsigned short **b )
{
	msurface_t	*surf1, *surf2;

	surf1 = &worldmodel->surfaces[*(unsigned short *)a];
	surf2 = &worldmodel->surfaces[*(unsigned short *)b];

	if( FBitSet( surf1->flags, SURF_DRAWSKY ) && !FBitSet( surf2->flags, SURF_DRAWSKY ))
		return -1;

	if( !FBitSet( surf1->flags, SURF_DRAWSKY ) && FBitSet( surf2->flags, SURF_DRAWSKY ))
		return 1;

	if( FBitSet( surf1->flags, SURF_DRAWTURB ) && !FBitSet( surf2->flags, SURF_DRAWTURB ))
		return -1;

	if( !FBitSet( surf1->flags, SURF_DRAWTURB ) && FBitSet( surf2->flags, SURF_DRAWTURB ))
		return 1;

	// there faces owned with model in local space, so it *always* have non-identity transform matrix.
	// move them to end of the list
	if( FBitSet( surf1->flags, SURF_LOCAL_SPACE ) && !FBitSet( surf2->flags, SURF_LOCAL_SPACE ))
		return 1;

	if( !FBitSet( surf1->flags, SURF_LOCAL_SPACE ) && FBitSet( surf2->flags, SURF_LOCAL_SPACE ))
		return -1;

	return 0;
}

/*
=================
Mod_SurfaceCompareInGame

sort faces to reduce shader switches
=================
*/
static int Mod_SurfaceCompareInGame( const unsigned short **a, const unsigned short **b )
{
	msurface_t	*surf1, *surf2;
	mextrasurf_t	*esrf1, *esrf2;

	surf1 = &worldmodel->surfaces[*(unsigned short *)a];
	surf2 = &worldmodel->surfaces[*(unsigned short *)b];
	texture_t *tx1 = R_TextureAnimation( surf1 );
	texture_t *tx2 = R_TextureAnimation( surf2 );

	esrf1 = surf1->info;
	esrf2 = surf2->info;

	if( esrf1->shaderNum[0] > esrf2->shaderNum[0] )
		return 1;

	if( esrf1->shaderNum[0] < esrf2->shaderNum[0] )
		return -1;

	if( tx1->gl_texturenum > tx2->gl_texturenum )
		return 1;

	if( tx1->gl_texturenum < tx2->gl_texturenum )
		return -1;

	if( esrf1->lightmaptexturenum > esrf2->lightmaptexturenum )
		return 1;

	if( esrf1->lightmaptexturenum < esrf2->lightmaptexturenum )
		return -1;

	return 0;
}

/*
=================
Mod_SurfaceCompareTrans

compare translucent surfaces
=================
*/
static int Mod_SurfaceCompareTrans( const gl_bmodelface_t *a, const gl_bmodelface_t *b )
{
	Vector org1 = RI->currententity->origin + a->surface->info->origin;
	Vector org2 = RI->currententity->origin + b->surface->info->origin;

	// compare by plane dists
	float len1 = DotProduct( org1, RI->vforward ) - RI->viewplanedist;
	float len2 = DotProduct( org2, RI->vforward ) - RI->viewplanedist;

	if( len1 > len2 )
		return -1;
	if( len1 < len2 )
		return 1;

	return 0;
}


/*
=================
Mod_FinalizeWorld

build representation table
of surfaces sorted by texture
then alloc lightmaps
=================
*/
void Mod_FinalizeWorld( void )
{
	int	i;

	world->sortedfaces = (unsigned short *)Mem_Alloc( worldmodel->numsurfaces * sizeof( unsigned short ));
	world->numsortedfaces = worldmodel->numsurfaces;

	// initial filling
	for( i = 0; i < world->numsortedfaces; i++ )
		world->sortedfaces[i] = i;

	qsort( world->sortedfaces, world->numsortedfaces, sizeof( unsigned short ), (cmpfunc)Mod_SurfaceCompareBuild );

	// alloc surface lightmaps and compute lm coords (for sorted list)
	for( i = 0; i < world->numsortedfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[world->sortedfaces[i]];

		// allocate the lightmap coords, create lightmap textures (empty at this moment)
		GL_AllocLightmapForFace( surf );
	}
}

/*
=================
Mod_PrecacheShaders

precache shaders to reduce freezes in-game
=================
*/
void Mod_PrecacheShaders( void )
{
	msurface_t	*surf;
	int		i;

	// preload shaders for all the world faces (but ignore watery faces)
	for( i = 0; i < world->numsortedfaces; i++ )
	{
		if( world->sortedfaces[i] > worldmodel->nummodelsurfaces )
			continue;	// precache only world shaders

		surf = &worldmodel->surfaces[world->sortedfaces[i]];

		if( !FBitSet( surf->flags, SURF_DRAWSKY ))
			GL_UberShaderForSolidBmodel( surf );
	}

	tr.params_changed = true;
#if 0
	Msg( "sorted faces:\n" );
	for( i = 0; i < world->numsortedfaces; i++ )
	{
		surf = &worldmodel->surfaces[world->sortedfaces[i]];
		Msg( "face %i (local %s), style[1] %i\n", i, FBitSet( surf->flags, SURF_LOCAL_SPACE ) ? "Yes" : "No", surf->styles[1] );
	}
#endif
}

/*
=================
Mod_ResortFaces

if shaders was changed we need to resort them
=================
*/
void Mod_ResortFaces( void )
{
	msurface_t	*surf;
	int		i;

	if( !tr.params_changed ) return;

	// rebuild shaders
	for( i = 0; i < world->numsortedfaces; i++ )
	{
		if( world->sortedfaces[i] > worldmodel->nummodelsurfaces )
			continue;	// precache only world shaders

		surf = &worldmodel->surfaces[world->sortedfaces[i]];

		if( !FBitSet( surf->flags, SURF_DRAWSKY ))
			GL_UberShaderForSolidBmodel( surf );
	}

	// resort faces
	qsort( world->sortedfaces, world->numsortedfaces, sizeof( unsigned short ), (cmpfunc)Mod_SurfaceCompareInGame );
#if 0
	Msg( "resorted faces:\n" );
	for( i = 0; i < world->numsortedfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[world->sortedfaces[i]];
		texture_t *tx = R_TextureAnimation( surf );
		mextrasurf_t *esrf = surf->info;
		Msg( "face %i, shader %i, texture %i lightmap %i\n", i, esrf->shaderNum[0], tx->gl_texturenum, esrf->lightmaptexturenum );
	}
#endif
}

/*
=================
Mod_ComputeVertexNormal

compute normals if missed
=================
*/
static void Mod_ComputeVertexNormal( msurface_t *surf, mextrasurf_t *esrf )
{
	bvert_t	*v0, *v1, *v2;
	Vector	areaNormal;

	if( world->normals ) return;

	// build areaweighted normal
	if( FBitSet( surf->flags, SURF_DRAWTURB ))
	{
		for( int j = 0; j < esrf->numindexes; j += 3 )
		{
			v0 = &world->vertexes[esrf->firstvertex + esrf->indexes[j+0]];
			v1 = &world->vertexes[esrf->firstvertex + esrf->indexes[j+1]];
			v2 = &world->vertexes[esrf->firstvertex + esrf->indexes[j+2]];

			TriangleNormal( v0->vertex, v1->vertex, v2->vertex, areaNormal );

			v0->normal += areaNormal;
			v1->normal += areaNormal;
			v2->normal += areaNormal;
		}
	}
	else
	{
		// fallback: get the normal from plane
		for( int i = 0; i < esrf->numverts; i++ )
		{
			v0 = &world->vertexes[esrf->firstvertex + i];

			// calc unsmoothed tangent space
			if( FBitSet( surf->flags, SURF_PLANEBACK ))
				v0->normal = -surf->plane->normal;
			else v0->normal = surf->plane->normal;
		}
	}
}

/*
=================
Mod_SmoothVertexNormals

trying th smooth the normals
=================
*/
static void Mod_SmoothVertexNormals( void )
{
	float	 	smooth_threshold;
	int		i, j, k, l;
	Vector		faceNormal;
	msurface_t	*s0, *s1;
	bvert_t		*v0, *v1;
	double		start;

	if( world->normals ) return;

	smooth_threshold = cos( DEG2RAD( 50.0f ));
	start = Sys_DoubleTime();

	for( i = 0, s0 = worldmodel->surfaces; i < worldmodel->numsurfaces; i++, s0++ )
	{
		if( FBitSet( s0->flags, SURF_DRAWSKY|SURF_DRAWTURB ))
			continue;

		// clear normals first
		for( j = 0; j < s0->info->numverts; j++ )
		{
			v0 = &world->vertexes[s0->info->firstvertex + j];
			v0->normal = g_vecZero;
		}

		// collect coincident surfaces' TBN
		for( j = 0, s1 = worldmodel->surfaces; j < worldmodel->numsurfaces; j++, s1++ )
		{
			if( FBitSet( s1->flags, SURF_DRAWSKY|SURF_DRAWTURB ))
				continue;

			if( s0->texinfo->texture != s1->texinfo->texture )
				continue;	// don't interact with other textures

			if( DotProduct( s0->plane->normal, s1->plane->normal ) < smooth_threshold )
				continue;

			// calc unsmoothed tangent space
			if( FBitSet( s1->flags, SURF_PLANEBACK ))
				faceNormal = -s1->plane->normal;
			else faceNormal = s1->plane->normal;

			for( k = 0; k < s0->info->numverts; k++ )
			{
				for( l = 0; l < s1->info->numverts; l++ )
				{
					v0 = &world->vertexes[s0->info->firstvertex + k];
					v1 = &world->vertexes[s1->info->firstvertex + l];

					if( !VectorCompareEpsilon( v0->vertex, v1->vertex, ON_EPSILON ))
						continue;

					v0->normal += faceNormal;
				}
			}
		}

		// renormalize them
		for( j = 0; j < s0->info->numverts; j++ )
		{
			v0 = &world->vertexes[s0->info->firstvertex + j];
			v0->normal = v0->normal.Normalize();
		}
	}
	Msg( "smooth normals: elapsed time %g secs\n", Sys_DoubleTime() - start );
}

/*
==================
GetLayerIndexForPoint

this function came from q3map2
==================
*/
static byte Mod_GetLayerIndexForPoint( indexMap_t *im, const Vector &mins, const Vector &maxs, const Vector &point )
{
	Vector	size;

	if( !im->pixels ) return 0;

	for( int i = 0; i < 3; i++ )
		size[i] = ( maxs[i] - mins[i] );

	float s = ( point[0] - mins[0] ) / size[0];
	float t = ( maxs[1] - point[1] ) / size[1];

	int x = s * im->width;
	int y = t * im->height;

	x = bound( 0, x, ( im->width - 1 ));
	y = bound( 0, y, ( im->height - 1 ));

	return im->pixels[y * im->width + x];
}

/*
=================
Mod_LayerNameForPixel

return layer name per pixel
=================
*/
bool Mod_CheckLayerNameForPixel( mfaceinfo_t *land, const Vector &point, const char *checkName )
{
	terrain_t		*terra;
	layerMap_t	*lm;
	indexMap_t	*im;

	if( !land ) return true; // no landscape specified
	terra = land->terrain;
	if( !terra ) return true;

	im = &terra->indexmap;
	lm = &terra->layermap;

	if( !Q_stricmp( checkName, lm->names[Mod_GetLayerIndexForPoint( im, land->mins, land->maxs, point )] ))
		return true;
	return false;
}

/*
=================
Mod_CheckLayerNameForSurf

return layer name per face
=================
*/
bool Mod_CheckLayerNameForSurf( msurface_t *surf, const char *checkName )
{
	mtexinfo_t	*tx = surf->texinfo;
	mfaceinfo_t	*land = tx->faceinfo;
	terrain_t		*terra;
	layerMap_t	*lm;

	if( land != NULL && land->terrain != NULL )
	{
		terra = land->terrain;
		lm = &terra->layermap;

		for( int i = 0; i < terra->numLayers; i++ )
		{
			if( !Q_stricmp( checkName, lm->names[i] ))
				return true;
		}
	}
	else
	{
		const char *texname = surf->texinfo->texture->name;

		if( !Q_stricmp( checkName, texname ))
			return true;
	}

	return false;
}

/*
=================
Mod_ProcessLandscapes

handle all the landscapes per level
=================
*/
static void Mod_ProcessLandscapes( msurface_t *surf, mextrasurf_t *esrf )
{
	mtexinfo_t	*tx = surf->texinfo;
	mfaceinfo_t	*land = tx->faceinfo;

	if( !land || land->groupid == 0 || !land->landname[0] )
		return; // no landscape specified, just lightmap resolution

	if( !land->terrain )
	{
		land->terrain = R_FindTerrain( land->landname );

		if( !land->terrain )
		{
			// land name was specified in bsp but not declared in script file
			ALERT( at_error, "Mod_ProcessLandscapes: %s missing description\n", land->landname );
			land->landname[0] = '\0'; // clear name to avoid trying to find invalid terrain
			return;
		}

		// prepare new landscape params
		ClearBounds( land->mins, land->maxs );
	}

	// update terrain bounds
	AddPointToBounds( esrf->mins, land->mins, land->maxs );
	AddPointToBounds( esrf->maxs, land->mins, land->maxs );

	for( int j = 0; j < esrf->numverts; j++ )
	{
		bvert_t *v = &world->vertexes[esrf->firstvertex + j];
		AddPointToBounds( v->vertex, land->mins, land->maxs );
	}
}

/*
=================
Mod_MappingLandscapes

now landscape AABB is actual
mappping the surfaces
=================
*/
static void Mod_MappingLandscapes( msurface_t *surf, mextrasurf_t *esrf )
{
	mtexinfo_t	*tx = surf->texinfo;
	mfaceinfo_t	*land = tx->faceinfo;
	float		mappingScale;
	terrain_t		*terra;
	bvert_t		*v;

	if( !land ) return; // no landscape specified
	terra = land->terrain;
	if( !terra ) return; // ooops! something bad happens!

	// now we have landscape info!
	SetBits( surf->flags, SURF_LANDSCAPE );
	mappingScale = terra->texScale;

	// mapping global diffuse texture
	for( int i = 0; i < esrf->numverts; i++ )
	{
		v = &world->vertexes[esrf->firstvertex + i];

		v->stcoord0[0] *= mappingScale;
		v->stcoord0[1] *= mappingScale;
		R_GlobalCoords( surf, v->vertex, v->stcoord0 );
	}
}

/*
=================
Mod_SubdividePolygon
=================
*/
static void Mod_SubdividePolygon_r( msurface_t *warpface, int numverts, Vector verts[], bool firstpass )
{
	Vector		front[SUBDIVIDE_SIZE];
	Vector		back[SUBDIVIDE_SIZE];
	float		dist[SUBDIVIDE_SIZE];
	mextrasurf_t	*es = warpface->info;
	int		i, j, k, f, b;
	Vector		mins, maxs;
	float		m, frac;
	bvert_t		*mv;

	if( numverts > ( SUBDIVIDE_SIZE - 4 ))
		HOST_ERROR( "Mod_SubdividePolygon: too many vertexes on face ( %i )\n", numverts );

	ClearBounds( mins, maxs );
	for( i = 0; i < numverts; i++ )
		AddPointToBounds( verts[i], mins, maxs );

	for( i = 0; i < 3; i++ )
	{
		m = ( mins[i] + maxs[i] ) * 0.5f;
		m = SUBDIVIDE_SIZE * floor( m / SUBDIVIDE_SIZE + 0.5f );
		if( maxs[i] - m < 8.0f ) continue;
		if( m - mins[i] < 8.0f ) continue;

		// cut it
		for( j = 0; j < numverts; j++ )
			dist[j] = verts[j][i] - m;

		// wrap cases
		verts[j] = verts[0];
		dist[j] = dist[0];
		f = b = 0;

		for( j = 0; j < numverts; j++ )
		{
			if( dist[j] >= 0 )
			{
				front[f] = verts[j];
				f++;
			}

			if( dist[j] <= 0 )
			{
				back[b] = verts[j];
				b++;
			}

			if( dist[j] == 0 || dist[j+1] == 0 )
				continue;

			if(( dist[j] > 0 ) != ( dist[j+1] > 0 ))
			{
				// clip point
				frac = dist[j] / ( dist[j] - dist[j+1] );
				for( k = 0; k < 3; k++ )
					front[f][k] = back[b][k] = verts[j][k] + frac * (verts[j+1][k] - verts[j][k]);
				f++;
				b++;
			}
		}

		Mod_SubdividePolygon_r( warpface, f, front, firstpass );
		Mod_SubdividePolygon_r( warpface, b, back, firstpass );
		return;
	}

	if( firstpass )
	{
		es->numindexes += (numverts - 2) * 3;
		es->numverts += numverts;
		return;
	}

	// copy verts
	for( i = 0; i < numverts; i++ )
	{
		mv = &world->vertexes[es->firstvertex + es->numverts + i];
		mv->vertex = verts[i];
		R_TextureCoords( warpface, mv->vertex, mv->stcoord0 );
		R_LightmapCoords( warpface, mv->vertex, mv->lmcoord0, 0 );	// styles 0-1
		R_LightmapCoords( warpface, mv->vertex, mv->lmcoord1, 2 );	// styles 2-3
	}

	// build indices
	unsigned short *indexes = es->indexes + es->numindexes;

	for( i = 0; i < numverts - 2; i++ )
	{
		indexes[i*3+0] = es->numverts;
		indexes[i*3+1] = es->numverts + i + 1;
		indexes[i*3+2] = es->numverts + i + 2;
	}

	es->numindexes += (numverts - 2) * 3;
	es->numverts += numverts;
}

/*
================
Mod_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
static int Mod_SubdivideSurface( msurface_t *fa, bool firstpass )
{
	Vector		verts[SUBDIVIDE_SIZE];
	mextrasurf_t	*es = fa->info;
	int		numVerts = 0;

	// convert edges back to a normal polygon
	for( int i = 0; i < fa->numedges; i++ )
	{
		int l = worldmodel->surfedges[fa->firstedge + i];
		int vert = worldmodel->edges[abs(l)].v[(l > 0) ? 0 : 1];
		mvertex_t	*dv = &worldmodel->vertexes[vert];
		verts[i] = dv->position;
	}

	// do subdivide
	Mod_SubdividePolygon_r( fa, fa->numedges, verts, firstpass );

	if( firstpass )
	{
		// allocate the indexes array
		es->indexes = (unsigned short *)Mem_Alloc( es->numindexes * sizeof( short ));
		numVerts = es->numverts;
		es->numindexes = 0;
		es->numverts = 0;

		return numVerts;
	}

	return es->numverts;
}

/*
=================
Mod_CreateBufferObject
=================
*/
static void Mod_CreateBufferObject( void )
{
	if( world->vertex_buffer_object )
		return; // already created

	// calculate number of used faces and vertexes
	msurface_t *surf = worldmodel->surfaces;
	int i, j, curVert = 0;
	mvertex_t *dv;
	bvert_t *mv;

	world->numvertexes = 0;

	// compute totalvertex count for VBO but ignore sky polys
	for( i = 0; i < worldmodel->numsurfaces; i++, surf++ )
	{
		if( FBitSet( surf->flags, SURF_DRAWSKY ))
			continue;
		if( FBitSet( surf->flags, SURF_DRAWTURB ))
			world->numvertexes += Mod_SubdivideSurface( surf, true );
		else world->numvertexes += surf->numedges;
	}

	// temporary array will be removed at end of this function
	// g-cont. i'm leave local copy of vertexes for some debug purpoces
	world->vertexes = (bvert_t *)Mem_Alloc( sizeof( bvert_t ) * world->numvertexes );
	surf = worldmodel->surfaces;

	// create VBO-optimized vertex array (single for world and all brush-models)
	for( i = 0; i < worldmodel->numsurfaces; i++, surf++ )
	{
		Vector t, b, n;

		if( FBitSet( surf->flags, SURF_DRAWSKY ))
			continue;	// ignore sky polys it was never be drawed

		if( FBitSet( surf->flags, SURF_DRAWTURB ))
		{
			surf->info->firstvertex = curVert;
			Mod_SubdivideSurface( surf, false );
			curVert += surf->info->numverts;
		}
		else
		{
			mv = &world->vertexes[curVert];

			// NOTE: all polygons stored as source (no tesselation anyway)
			for( j = 0; j < surf->numedges; j++, mv++ )
			{
				int l = worldmodel->surfedges[surf->firstedge + j];
				int vert = worldmodel->edges[abs(l)].v[(l > 0) ? 0 : 1];
				memcpy( mv->styles, surf->styles, sizeof( surf->styles ));
				dv = &worldmodel->vertexes[vert];
				mv->vertex = dv->position;

				if( world->surfnormals != NULL && world->normals != NULL )
				{
					l = world->surfnormals[surf->firstedge + j];
					if( l >= 0 || l < world->numnormals )
						mv->normal = Vector( world->normals[l].normal );
					else ALERT( at_error, "normal index %d out of range (max %d)\n", l, world->numnormals );
				}
				else if( world->normals != NULL )
					mv->normal = Vector( world->normals[vert].normal );

				R_TextureCoords( surf, mv->vertex, mv->stcoord0 );
				R_LightmapCoords( surf, mv->vertex, mv->lmcoord0, 0 );	// styles 0-1
				R_LightmapCoords( surf, mv->vertex, mv->lmcoord1, 2 );	// styles 2-3
			}

			// NOTE: now firstvertex are handled in world->vertexes[] array, not in world->tbn_vectors[] !!!
			surf->info->firstvertex = curVert;
			surf->info->numverts = surf->numedges;
			curVert += surf->numedges;
		}

		Mod_ComputeVertexNormal( surf, surf->info );
	}
#if 0
	// it's works for a very long time
	Mod_SmoothVertexNormals();
#endif
	// time to prepare landscapes
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		Mod_ProcessLandscapes( surf, surf->info );
	}

	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		Mod_MappingLandscapes( surf, surf->info );
	}

	if( world->surfnormals != NULL )
		Mem_Free( world->surfnormals );
	world->surfnormals = NULL;

	// now normals are merged into single array world->vertexes[]
	if( world->normals != NULL )
		Mem_Free( world->normals );
	world->normals = NULL;

	// create GPU static buffer
	pglGenBuffersARB( 1, &world->vertex_buffer_object );
	pglGenVertexArrays( 1, &world->vertex_array_object );

	// create vertex array object
	pglBindVertexArray( world->vertex_array_object );

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, world->vertex_buffer_object );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, world->numvertexes * sizeof( bvert_t ), &world->vertexes[0], GL_STATIC_DRAW_ARB );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, 0, sizeof( bvert_t ), (void *)offsetof( bvert_t, vertex ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, 0, sizeof( bvert_t ), (void *)offsetof( bvert_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, 0, sizeof( bvert_t ), (void *)offsetof( bvert_t, stcoord0 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, 0, sizeof( bvert_t ), (void *)offsetof( bvert_t, lmcoord0 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, 0, sizeof( bvert_t ), (void *)offsetof( bvert_t, lmcoord1 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, 0, sizeof( bvert_t ), (void *)offsetof( bvert_t, styles ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );

	// don't forget to unbind them
	pglBindVertexArray( GL_FALSE );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

/*
=================
Mod_DeleteBufferObject
=================
*/
static void Mod_DeleteBufferObject( void )
{
	if( world->vertex_array_object ) pglDeleteVertexArrays( 1, &world->vertex_array_object );
	if( world->vertex_buffer_object ) pglDeleteBuffersARB( 1, &world->vertex_buffer_object );

	world->vertex_array_object = world->vertex_buffer_object = 0;
}

/*
=================
Mod_PrepareModelInstances

throw all the instances before
loading the new map
=================
*/
void Mod_PrepareModelInstances( void )
{
	// invalidate model handles
	for( int i = 1; i < RENDER_GET_PARM( PARM_MAX_ENTITIES, 0 ); i++ )
	{
		cl_entity_t *e = GET_ENTITY( i );
		if( !e ) break;
		e->modelhandle = INVALID_HANDLE;
	}

	GET_VIEWMODEL()->modelhandle = INVALID_HANDLE;
}

/*
=================
Mod_ThrowModelInstances

throw all the instances before
loading the new map
=================
*/
void Mod_ThrowModelInstances( void )
{
	// engine already released entity array so we can't release
	// model instance for each entity personally 
	g_StudioRenderer.DestroyAllModelInstances();

	if( g_fRenderInitialized )
	{
		// may caused by Host_Error, clear gl state
		pglBindVertexArray( GL_FALSE );
		pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
		GL_BindShader( GL_NONE );
		GL_BindFBO( FBO_MAIN );
	}
}

/*
=================
Mod_LoadWorld


=================
*/
void Mod_LoadWorld( model_t *mod, const byte *buf )
{
	dheader_t	*header;
	dextrahdr_t *extrahdr;
	int i;

	header = (dheader_t *)buf;
	extrahdr = (dextrahdr_t *)((byte *)buf + sizeof( dheader_t ));

	if( RENDER_GET_PARM( PARM_MAP_HAS_DELUXE, 0 ))
		SetBits( world->features, WORLD_HAS_DELUXEMAP );

	if( RENDER_GET_PARM( PARM_WATER_ALPHA, 0 ))
		SetBits( world->features, WORLD_WATERALPHA );

	COM_FileBase( worldmodel->name, world->name );
#if 0
	Msg( "Mod_LoadWorld: %s\n", world->name );
	Msg( "sizeof( bvert_t ) == %d bytes\n", sizeof( bvert_t ));
	Msg( "sizeof( svert_t ) == %d bytes\n", sizeof( svert_t ));
#endif
	R_CheckChanges(); // catch all the cvar changes
	tr.glsl_valid_sequence = 1;
	tr.params_changed = false;

	// precache and upload cinematics
	R_InitCinematics();

	// prepare visibility and setup leaf extradata
	Mod_SetupWorldLeafs();
	Mod_SetupWorldNodes();

	int visclusters = mod->submodels[0].visleafs;
	tr.pvssize = (visclusters + 7) >> 3;

	// all the old lightmaps are freed
	GL_BeginBuildingLightmaps();

	// process landscapes first
	R_LoadLandscapes( world->name );

	// load material textures
	Mod_LoadWorldMaterials();

	// mark submodel faces
	for( i = worldmodel->submodels[0].numfaces; i < worldmodel->numsurfaces; i++ )
		SetBits( worldmodel->surfaces[i].flags, SURF_OF_SUBMODEL );

	// detect surfs in local space
	for( i = 0; i < worldmodel->numsubmodels; i++ )
	{
		dmodel_t *bm = &worldmodel->submodels[i];
		if( bm->origin == g_vecZero )
			continue; // abs space

		// mark surfs in local space
		msurface_t *surf = worldmodel->surfaces + bm->firstface;
		for( int j = 0; j < bm->numfaces; j++, surf++ )
			SetBits( surf->flags, SURF_LOCAL_SPACE );
	}

	if( extrahdr->id == IDEXTRAHEADER && extrahdr->version == EXTRA_VERSION )
	{
		// new Xash3D extended format
		Mod_LoadVertNormals( buf, &extrahdr->lumps[LUMP_VERTNORMALS] );
		Mod_LoadLeafAmbientLighting( buf, &extrahdr->lumps[LUMP_LEAF_LIGHTING] );
		Mod_LoadVertexLighting( buf, &extrahdr->lumps[LUMP_VERTEX_LIGHT] );

		Mod_LinkLeafLights();
	}

	// mark surfaces for world features
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		texture_t *tex = surf->texinfo->texture;

		if( FBitSet( surf->flags, SURF_DRAWSKY ))
			SetBits( world->features, WORLD_HAS_SKYBOX );

		if( !Q_strncmp( tex->name, "movie", 5 ))
		{
			SetBits( world->features, WORLD_HAS_MOVIES );
			SetBits( surf->flags, SURF_MOVIE );
		}

		if( !Q_strcmp( tex->name, "reflect1" ) || !Q_strncmp( tex->name, "!reflect", 8 ))
		{
			SetBits( world->features, WORLD_HAS_MIRRORS );
			SetBits( surf->flags, SURF_REFLECT );
		}

		if( !Q_strncmp( tex->name, "portal", 6 ))
		{
			SetBits( world->features, WORLD_HAS_PORTALS );
			SetBits( surf->flags, SURF_PORTAL );
		}

		if( !Q_strncmp( tex->name, "monitor", 7 ))
		{
			SetBits( world->features, WORLD_HAS_SCREENS );
			SetBits( surf->flags, SURF_SCREEN );
		}

		if( !Q_strncmp( tex->name, "chrome", 6 ))
		{
			SetBits( surf->flags, SURF_CHROME );
		}
	}

	Mod_FinalizeWorld();

	// create lightmap pages (empty at this moment)
	GL_EndBuildingLightmaps( (worldmodel->lightdata != NULL), FBitSet( world->features, WORLD_HAS_DELUXEMAP ) ? true : false );
	Mod_CreateBufferObject();
	tr.fResetVis = true;

	// time to place grass
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		// place to initialize our grass
		R_GrassInitForSurface( &worldmodel->surfaces[i] );
	}

	// precache world shaders
	Mod_PrecacheShaders();
	Mod_ResortFaces();
}

void Mod_FreeWorld( model_t *mod )
{
//	Msg( "Mod_FreeWorld: %s\n", world->name );

	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		texture_t *tx = worldmodel->textures[i];

		// bad texture? 
		if( !tx || !tx->name[0] ) continue;

		FREE_TEXTURE( tx->fb_texturenum );
	}

	if( world->leafs )
		Mem_Free( world->leafs );
	world->leafs = NULL;

	if( world->vertexes )
		Mem_Free( world->vertexes );
	world->vertexes = NULL;

	if( world->vertex_lighting )
		Mem_Free( world->vertex_lighting );
	world->vertex_lighting = NULL;

	// destroy VBO & VAO
	Mod_DeleteBufferObject();

	// free old cinematics
	R_FreeCinematics();

	// free landscapes
	R_FreeLandscapes();

	if( FBitSet( world->features, WORLD_HAS_GRASS ))
	{
		// throw grass vbo's
		for( int i = 0; i < worldmodel->numsurfaces; i++ )
			R_RemoveGrassForSurface( worldmodel->surfaces[i].info );
	}

	memset( world, 0, sizeof( gl_world_t ));
	tr.grass_total_size = 0;
}

/*
==================
Mod_SetOrthoBounds

setup ortho min\maxs for draw overview
==================
*/
void Mod_SetOrthoBounds( const float *mins, const float *maxs )
{
	if( !g_fRenderInitialized ) return;

	world->orthocenter.x = ((maxs[0] + mins[0]) * 0.5f);
	world->orthocenter.y = ((maxs[1] + mins[1]) * 0.5f);
	world->orthohalf.x = maxs[0] - world->orthocenter.x;
	world->orthohalf.y = maxs[1] - world->orthocenter.y;
}

/*
==================
R_ProcessWorldData

resource management
==================
*/
void R_ProcessWorldData( model_t *mod, qboolean create, const byte *buffer )
{
	RI->currententity = NULL;
	RI->currentmodel = NULL;
	worldmodel = mod;

	if( create ) Mod_LoadWorld( mod, buffer );
	else Mod_FreeWorld( mod );
}

/*
=============================================================================

  WORLD RENDERING

=============================================================================
*/
static unsigned int tempElems[MAX_MAP_ELEMS];
static unsigned int numTempElems;

// accumulate the indices
#define R_DrawSurface( es )					\
for( int vert = 0; vert < es->numverts - 2; vert++ )		\
{							\
	ASSERT( numTempElems < ( MAX_MAP_ELEMS - 3 ));		\
	tempElems[numTempElems++] = es->firstvertex;		\
	tempElems[numTempElems++] = es->firstvertex + vert + 1;	\
	tempElems[numTempElems++] = es->firstvertex + vert + 2;	\
}

// accumulate the indices
#define R_DrawIndexedSurface( es )					\
for( int elem = 0; elem < es->numindexes; elem++ )			\
{								\
	ASSERT( numTempElems < ( MAX_MAP_ELEMS - 3 ));			\
	tempElems[numTempElems++] = es->firstvertex + es->indexes[elem];	\
}

word R_ChooseBmodelProgram( msurface_t *surf, cl_entity_t *e, bool lightpass )
{
	bool translucent;

	switch( e->curstate.rendermode )
	{
	case kRenderTransTexture:
	case kRenderTransColor:
	case kRenderTransAdd:
		translucent = true;
		break;
	default:
		translucent = false;
		break;
	}

	if( FBitSet( RI->params, RP_SHADOWVIEW )) // shadow pass
		return (glsl.bmodelDepthFill - glsl_programs);

	if( lightpass )
		return GL_UberShaderForBmodelDlight( RI->currentlight, surf, translucent );
	return GL_UberShaderForSolidBmodel( surf, translucent );
}

/*
=================
R_AddSurfaceToDrawList

add specified face into sorted drawlist
=================
*/
bool R_AddSurfaceToDrawList( msurface_t *surf, bool lightpass )
{
	cl_entity_t	*e = RI->currententity;
	word		hProgram;
	gl_bmodelface_t	*entry;

	if( FBitSet( RI->params, RP_SHADOWVIEW ))
		lightpass = false;

	if( lightpass )
	{
		if( tr.num_light_surfaces >= MAX_MAP_FACES )
			return false;
	}
	else
	{
		if( tr.num_draw_surfaces >= MAX_MAP_FACES )
			return false;
	}

	if( !( hProgram = R_ChooseBmodelProgram( surf, e, lightpass )))
		return false; // failed to build shader, don't draw this surface

	if( lightpass ) entry = &tr.light_surfaces[tr.num_light_surfaces++];
	else entry = &tr.draw_surfaces[tr.num_draw_surfaces++];

	// surface has passed all visibility checks
	// and can be update some data (lightmaps, mirror matrix, etc)
	R_UpdateSurfaceParams( surf );

	entry->hProgram = hProgram;
	entry->surface = surf;

	if( !FBitSet( RI->params, RP_SHADOWVIEW ))
	{
		if( surf->pdecals && tr.num_draw_decals < MAX_DECAL_SURFS )
			tr.draw_decals[tr.num_draw_decals++] = surf;
	}

	return true;
}

void R_SetRenderColor( cl_entity_t *e )
{
	float r = e->curstate.rendercolor.r / 255.0f;
	float g = e->curstate.rendercolor.g / 255.0f;
	float b = e->curstate.rendercolor.b / 255.0f;
	float a = tr.blend;//e->curstate.renderamt / 255.0f;

	switch( e->curstate.rendermode )
	{
	case kRenderNormal:
		pglUniform4fARB( RI->currentshader->u_RenderColor, 1.0f, 1.0f, 1.0f, 1.0f );
		break;
	case kRenderTransColor:
		pglUniform4fARB( RI->currentshader->u_RenderColor, r, g, b, a );
		break;
	case kRenderTransAdd:
		pglUniform4fARB( RI->currentshader->u_RenderColor, a, a, a, 1.0f );
		break;
	case kRenderTransAlpha:
		pglUniform4fARB( RI->currentshader->u_RenderColor, 1.0f, 1.0f, 1.0f, 1.0f );
		break;
	default:
		pglUniform4fARB( RI->currentshader->u_RenderColor, 1.0f, 1.0f, 1.0f, a );
		break;
	}
}

/*
================
R_RenderDynLightList

================
*/
void R_BuildFaceListForLight( plight_t *pl )
{
	cl_entity_t *e = RI->currententity;

	R_GrassPrepareLight();

	tr.num_light_surfaces = 0;
	tr.modelorg = pl->origin;

	// only visible polys passed through the light list
	for( int i = 0; i < tr.num_draw_surfaces; i++ )
	{
		gl_bmodelface_t *entry = &tr.draw_surfaces[i];
		mextrasurf_t *es = entry->surface->info;
		gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];
		tr.modelorg = glm->transform.VectorITransform( pl->origin );

		if( es->subtexture[glState.stack_position] )
			continue;	// don't light the mirrors, portals etc

		R_AddGrassToChain( entry->surface, &pl->frustum, true );

		if( R_CullSurfaceExt( entry->surface, &pl->frustum ))
			continue;

		// move from main list into light list
		R_AddSurfaceToDrawList( entry->surface, true );
	}
}

/*
================
R_DrawLightForSurfList

setup light projection for each 
================
*/
void R_DrawLightForSurfList( plight_t *pl )
{
	texture_t		*cached_texture = NULL;
	qboolean		flush_buffer = false;
	cl_entity_t	*e = RI->currententity;
	gl_state_t	*glm = &tr.cached_state[e->hCachedMatrix];
	GLfloat		gl_lightViewProjMatrix[16];
	int		startv, endv;

	if( e->curstate.rendermode == kRenderTransAlpha )
		pglEnable( GL_ALPHA_TEST );
	pglBlendFunc( GL_ONE, GL_ONE );
	startv = MAX_MAP_ELEMS;
	numTempElems = 0;
	endv = 0;

	float y2 = (float)RI->viewport[3] - pl->h - pl->y;
	pglScissor( pl->x, y2, pl->w, pl->h );

	Vector lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
	pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );

	for( int i = 0; i < tr.num_light_surfaces; i++ )
	{
		gl_bmodelface_t *entry = &tr.light_surfaces[i];
		mextrasurf_t *es = entry->surface->info;
		msurface_t *s = entry->surface;

		texture_t *tex = R_TextureAnimation( s );

		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->hProgram] ))
			flush_buffer = true;

		if( cached_texture != tex )
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.c_total_tris += (numTempElems / 3);
				r_stats.num_flushes++;
				startv = MAX_MAP_ELEMS;
				numTempElems = 0;
				endv = 0;
			}
			flush_buffer = false;
		}

		// begin draw the sorted list
		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->hProgram] ))
		{
			Vector lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;

			GL_BindShader( &glsl_programs[entry->hProgram] );			

			ASSERT( RI->currentshader != NULL );

			// write constants
			pglUniformMatrix4fvARB( RI->currentshader->u_LightViewProjectionMatrix, 1, GL_FALSE, &gl_lightViewProjMatrix[0] );
			float shadowWidth = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_WIDTH, pl->shadowTexture );
			float shadowHeight = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_HEIGHT, pl->shadowTexture );

			// depth scale and bias and shadowmap resolution
			pglUniform4fARB( RI->currentshader->u_LightDir, lightdir.x, lightdir.y, lightdir.z, pl->fov );
			pglUniform4fARB( RI->currentshader->u_LightDiffuse, pl->color.r / 255.0f, pl->color.g / 255.0f, pl->color.b / 255.0f, pl->lightFalloff );
			pglUniform4fARB( RI->currentshader->u_ShadowParams, shadowWidth, shadowHeight, -pl->projectionMatrix[2][2], pl->projectionMatrix[3][2] );
			pglUniform4fARB( RI->currentshader->u_LightOrigin, pl->origin.x, pl->origin.y, pl->origin.z, ( 1.0f / pl->radius ));
			pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			pglUniformMatrix4fvARB( RI->currentshader->u_ModelMatrix, 1, GL_FALSE, &glm->modelMatrix[0] );
			pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			R_SetRenderColor( RI->currententity );

			GL_Bind( GL_TEXTURE2, pl->projectionTexture );
			GL_Bind( GL_TEXTURE3, pl->shadowTexture );

			// reset cache
			cached_texture = NULL;
		}

		if( cached_texture != tex )
		{
			mtexinfo_t	*tx = s->texinfo;
			mfaceinfo_t	*land = tx->faceinfo;
			float		xScale, yScale, waveHeight;

			// set the current waveheight
			if( s->polys->verts[0][2] >= RI->vieworg[2] )
				waveHeight = -RI->currententity->curstate.scale;
			else waveHeight = RI->currententity->curstate.scale;

			if( FBitSet( s->flags, SURF_MOVIE ) && RI->currententity->curstate.body )
			{ 
				GL_Bind( GL_TEXTURE0, tr.cinTextures[es->cintexturenum-1] );
				GL_LoadIdentityTexMatrix();
			}
			else if( CVAR_TO_BOOL( r_lightmap ))
			{
				GL_Bind( GL_TEXTURE0, tr.whiteTexture );
				GL_LoadIdentityTexMatrix();
			}
			else if( FBitSet( s->flags, SURF_LANDSCAPE ) && land && land->terrain )
			{
				if( land->terrain->layermap.gl_diffuse_id )
					GL_Bind( GL_TEXTURE0, land->terrain->layermap.gl_diffuse_id );
				else if( land->terrain->indexmap.gl_diffuse_id && CVAR_TO_BOOL( r_detailtextures ))
					GL_Bind( GL_TEXTURE0, tr.grayTexture );
				else GL_Bind( GL_TEXTURE0, tex->gl_texturenum );
				GL_LoadIdentityTexMatrix();
			}
			else
			{
				GL_Bind( GL_TEXTURE0, tex->gl_texturenum );
				GL_LoadIdentityTexMatrix();
			}

			if( land && land->terrain && land->terrain->indexmap.gl_diffuse_id != 0 )
				GL_Bind( GL_TEXTURE1, land->terrain->indexmap.gl_diffuse_id );
			else GL_Bind( GL_TEXTURE1, tex->dt_texturenum );
			GET_DETAIL_SCALE( tex->gl_texturenum, &xScale, &yScale );
			pglUniform3fARB( RI->currentshader->u_DetailScale, xScale, yScale, waveHeight );
			pglUniform3fARB( RI->currentshader->u_TexOffset, es->texofs[0], es->texofs[1], tr.time );

			if( ScreenCopyRequired( RI->currentshader ))
				GL_Bind( GL_TEXTURE4, tr.screen_color );
			else GL_Bind( GL_TEXTURE4, tr.whiteTexture );

			if( FBitSet( s->flags, SURF_LANDSCAPE ) && land && land->terrain )
				GL_Bind( GL_TEXTURE5, land->terrain->indexmap.gl_heightmap_id );

			cached_texture = tex;
		}

		if( es->firstvertex < startv )
			startv = es->firstvertex;

		if(( es->firstvertex + es->numverts ) > endv )
			endv = es->firstvertex + es->numverts;

		// accumulate the indices
		if( FBitSet( s->flags, SURF_DRAWTURB ))
		{
			R_DrawIndexedSurface( es );
		}
		else
		{
			R_DrawSurface( es );
		}
	}

	if( numTempElems )
	{
		pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.c_total_tris += (numTempElems / 3);
		r_stats.num_flushes++;
		startv = MAX_MAP_ELEMS;
		numTempElems = 0;
		endv = 0;
	}

	if( R_GrassUseBufferObject( ))
		R_DrawLightForGrass( pl );
	else R_DrawGrassLight( pl );
}

/*
================
R_RenderDynLightList

draw dynamic lights for world and bmodels
================
*/
void R_RenderDynLightList( void )
{
	if( FBitSet( RI->params, RP_ENVVIEW ))
		return;

	if( R_FullBright( )) return;

	if( !R_CountPlights( ))
		return;

	GL_Blend( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	pglEnable( GL_SCISSOR_TEST );
	pglBindVertexArray( world->vertex_array_object );

	plight_t *pl = cl_plights;

	for( int i = 0; i < MAX_PLIGHTS; i++, pl++ )
	{
		if( pl->die < tr.time || !pl->radius || pl->culled )
			continue;

		RI->currentlight = pl;

		if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ))
			continue;

		if( R_CullBox( pl->absmin, pl->absmax ))
			continue;

		// draw world from light position
		R_BuildFaceListForLight( pl );

		if( !tr.num_light_surfaces && !tr.num_light_grass )
			continue;	// no interaction with this light?

		R_DrawLightForSurfList( pl );
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	pglDisable( GL_SCISSOR_TEST );
	GL_CleanUpTextureUnits( 0 );
	RI->currentlight = NULL;
}

/*
================
R_DrawShadowBrushList

================
*/
void R_DrawShadowBrushList( void )
{
	int		cached_texture = -1;
	qboolean		flush_buffer = false;
	plight_t		*pl = RI->currentlight;
	cl_entity_t	*e = RI->currententity;
	int		startv, endv;

	if( !tr.num_draw_surfaces )
		return;

	RI->currentmodel = e->model;
	R_LoadIdentity();
	startv = MAX_MAP_ELEMS;
	numTempElems = 0;
	endv = 0;

	pglBindVertexArray( world->vertex_array_object );
	pglAlphaFunc( GL_GREATER, 0.25f );
	GL_TextureTarget( GL_NONE );

	for( int i = 0; i < tr.num_draw_surfaces; i++ )
	{
		gl_bmodelface_t *entry = &tr.draw_surfaces[i];
		mextrasurf_t *es = entry->surface->info;
		msurface_t *s = entry->surface;

		if( !entry->hProgram ) continue;

		int curtex = tr.whiteTexture;
		texture_t *tex = R_TextureAnimation( s );
		if( FBitSet( s->flags, SURF_TRANSPARENT ))
			curtex = tex->gl_texturenum;

		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->hProgram] ))
			flush_buffer = true;

		if( cached_texture != curtex )
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.c_total_tris += (numTempElems / 3);
				r_stats.num_flushes++;
				startv = MAX_MAP_ELEMS;
				numTempElems = 0;
				endv = 0;
			}
			flush_buffer = false;
		}

		// begin draw the sorted list
		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->hProgram] ))
		{
			GL_BindShader( &glsl_programs[entry->hProgram] );			

			ASSERT( RI->currentshader != NULL );

			gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];

			pglUniformMatrix4fvARB( RI->currentshader->u_ModelMatrix, 1, GL_FALSE, &glm->modelMatrix[0] );
			pglUniform2fARB( RI->currentshader->u_TexOffset, es->texofs[0], es->texofs[1] );

			// reset cache
			cached_texture = -1;
		}

		// begin draw the sorted list
		if( cached_texture != curtex )
		{
			if( curtex != tr.whiteTexture )
				GL_AlphaTest( GL_TRUE );
			else GL_AlphaTest( GL_FALSE );

			GL_Bind( GL_TEXTURE0, curtex );
			cached_texture = curtex;
		}

		if( es->firstvertex < startv )
			startv = es->firstvertex;

		if(( es->firstvertex + es->numverts ) > endv )
			endv = es->firstvertex + es->numverts;

		// accumulate the indices
		for( int j = 0; j < es->numverts - 2; j++ )
		{
			ASSERT( numTempElems < ( MAX_MAP_ELEMS - 3 ));

			tempElems[numTempElems++] = es->firstvertex;
			tempElems[numTempElems++] = es->firstvertex + j + 1;
			tempElems[numTempElems++] = es->firstvertex + j + 2;
		}
	}

	if( numTempElems )
	{
		pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.c_total_tris += (numTempElems / 3);
		r_stats.num_flushes++;
		startv = MAX_MAP_ELEMS;
		numTempElems = 0;
		endv = 0;
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	pglBindVertexArray( GL_FALSE );
	GL_BindShader( GL_FALSE );
	tr.num_draw_surfaces = 0;
	GL_Cull( GL_FRONT );

	if( R_GrassUseBufferObject( ))
		R_RenderShadowGrassOnList();
	else R_DrawGrass();
}

/*
================
R_DrawBrushList
================
*/
void R_DrawBrushList( void )
{
	int		cached_mirror = -1;
	int		cached_lightmap = -1;
	int		cached_texture = -1;
	qboolean		flush_buffer = false;
	cl_entity_t	*e = RI->currententity;
	float		cached_texofs[2] = { -1.0f, -1.0f };
	int		startv, endv;
	int i;

	if( !tr.num_draw_surfaces )
		return;

	RI->currentmodel = e->model;
	R_LoadIdentity();
	startv = MAX_MAP_ELEMS;
	numTempElems = 0;
	endv = 0;

	pglBindVertexArray( world->vertex_array_object );
	r_stats.c_world_polys += tr.num_draw_surfaces;

	for( i = 0; i < tr.num_draw_surfaces; i++ )
	{
		gl_bmodelface_t *entry = &tr.draw_surfaces[i];
		mextrasurf_t *es = entry->surface->info;
		msurface_t *s = entry->surface;

		if( !entry->hProgram ) continue;

		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->hProgram] ))
			flush_buffer = true;

		if( cached_lightmap != es->lightmaptexturenum )
			flush_buffer = true;

		if( cached_mirror != es->subtexture[glState.stack_position] )
			flush_buffer = true;

		if( cached_texture != es->gl_texturenum )
			flush_buffer = true;

		if( cached_texofs[0] != es->texofs[0] || cached_texofs[1] != es->texofs[1] )
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.c_total_tris += (numTempElems / 3);
				r_stats.num_flushes++;
				startv = MAX_MAP_ELEMS;
				numTempElems = 0;
				endv = 0;
			}
			flush_buffer = false;
		}

		// begin draw the sorted list
		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->hProgram] ))
		{
			GL_BindShader( &glsl_programs[entry->hProgram] );			

			ASSERT( RI->currentshader != NULL );

			gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];

			// write constants
			pglUniform1fvARB( RI->currentshader->u_LightStyleValues, MAX_LIGHTSTYLES, &tr.lightstyles[0] );
			pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			pglUniformMatrix4fvARB( RI->currentshader->u_ModelMatrix, 1, GL_FALSE, &glm->modelMatrix[0] );
			pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			pglUniform1fARB( RI->currentshader->u_zFar, -RI->farClip );
			R_SetRenderColor( RI->currententity );

			// reset cache
			cached_texofs[0] = -1.0f;
			cached_texofs[1] = -1.0f;
			cached_texture = NULL;
			cached_lightmap = -1;
			cached_mirror = -1;
		}

		if( ScreenCopyRequired( RI->currentshader ))
		{
			Vector	absmin = e->origin + es->mins;
			Vector	absmax = e->origin + es->maxs;
			float	x, y, w, h;

			if( R_ScissorForAABB( absmin, absmax, &x, &y, &w, &h ))
			{
				// keep screencopy an actual
				float y2 = (float)RI->viewport[3] - h - y;
				GL_Bind( GL_TEXTURE0, tr.screen_color );
				pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, x, y2, x, y2, w, h );
				r_stats.c_screen_copy++;
				cached_texture = NULL;
				flush_buffer = true;
			}
		}

		if(( cached_mirror != es->subtexture[glState.stack_position] ) || ( cached_texture != es->gl_texturenum ))
		{
			mtexinfo_t	*tx = s->texinfo;
			mfaceinfo_t	*land = tx->faceinfo;
			float		xScale, yScale, waveHeight;

			// set the current waveheight
			if( s->polys->verts[0][2] >= RI->vieworg[2] )
				waveHeight = -RI->currententity->curstate.scale;
			else waveHeight = RI->currententity->curstate.scale;

			if( FBitSet( s->flags, SURF_REFLECT|SURF_PORTAL|SURF_SCREEN ) && es->subtexture[glState.stack_position] )
			{
				int handle = es->subtexture[glState.stack_position];
				GL_Bind( GL_TEXTURE0, tr.subviewTextures[handle-1].texturenum );
				GL_LoadTexMatrix( tr.subviewTextures[handle-1].matrix );
			}
			else
			{
				GL_Bind( GL_TEXTURE0, es->gl_texturenum );
				GL_LoadIdentityTexMatrix();
			}

			GL_Bind( GL_TEXTURE1, es->dt_texturenum );
			GET_DETAIL_SCALE( es->gl_texturenum, &xScale, &yScale );
			pglUniform3fARB( RI->currentshader->u_DetailScale, xScale, yScale, waveHeight );

			if( ScreenCopyRequired( RI->currentshader ))
				GL_Bind( GL_TEXTURE3, tr.screen_color );
			else if( FBitSet( s->flags, SURF_REFLECT ) && FBitSet( s->flags, SURF_DRAWTURB ))
				GL_Bind( GL_TEXTURE3, es->gl_texturenum ); // mix turbulency texture and reflection
			else GL_Bind( GL_TEXTURE3, tr.whiteTexture );

			if( FBitSet( s->flags, SURF_LANDSCAPE ) && land && land->terrain )
				GL_Bind( GL_TEXTURE4, land->terrain->indexmap.gl_heightmap_id );

			GL_Bind( GL_TEXTURE5, tx->texture->fb_texturenum );
			cached_mirror = es->subtexture[glState.stack_position];
			cached_texture = es->gl_texturenum;
			cached_texofs[0] = -1.0f;
			cached_texofs[1] = -1.0f;
		}

		if( cached_lightmap != es->lightmaptexturenum )
		{
			if( R_FullBright( ))
			{
				// bind stubs (helper to reduce conditions in shader)
				GL_Bind( GL_TEXTURE2, tr.whiteTexture );
			}
			else
			{
				// bind real data
				GL_Bind( GL_TEXTURE2, tr.lightmaps[es->lightmaptexturenum].lightmap );
			}
			cached_lightmap = es->lightmaptexturenum;
		}

		if( tr.viewparams.waterlevel >= 3 && RP_NORMALPASS() && FBitSet( s->flags, SURF_DRAWTURB ))
			GL_Cull( GL_BACK );
		else GL_Cull( GL_FRONT );

		if( cached_texofs[0] != es->texofs[0] || cached_texofs[1] != es->texofs[1] )
		{
			pglUniform3fARB( RI->currentshader->u_TexOffset, es->texofs[0], es->texofs[1], tr.time );
			cached_texofs[0] = es->texofs[0];
			cached_texofs[1] = es->texofs[1];
		}

		if( es->firstvertex < startv )
			startv = es->firstvertex;

		if(( es->firstvertex + es->numverts ) > endv )
			endv = es->firstvertex + es->numverts;

		if( FBitSet( s->flags, SURF_DRAWTURB ))
		{
			R_DrawIndexedSurface( es );
		}
		else
		{
			R_DrawSurface( es );
		}
	}

	if( numTempElems )
	{
		pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.c_total_tris += (numTempElems / 3);
		r_stats.num_flushes++;
		numTempElems = 0;
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	pglBindVertexArray( GL_FALSE );
	GL_BindShader( NULL );
	GL_Cull( GL_FRONT );

	// draw grass on visible surfaces
	if( R_GrassUseBufferObject( ))
		R_RenderGrassOnList();
	else R_DrawGrass();

	// draw dynamic lighting for world and bmodels
	R_RenderDynLightList ();

	pglBindVertexArray( GL_FALSE );
	GL_BindShader( NULL );

	DrawWireFrame();

	// clear the subview pointers after normalpass
	if( RP_NORMALPASS( ))
	{
		for( i = 0; i < tr.num_draw_surfaces; i++ )
			memset( tr.draw_surfaces[i].surface->info->subtexture, 0, sizeof( short[8] ));
	}
	tr.num_draw_surfaces = 0;

	// render all decals on world and opaque bmodels
	DrawDecalsBatch();
}

/*
================
R_DrawWorldList
================
*/
void R_DrawWorldList( void )
{
	int		cached_mirror = -1;
	int		cached_lightmap = -1;
	texture_t		*cached_texture = NULL;
	qboolean		flush_buffer = false;
	cl_entity_t	*e = RI->currententity;
	float		cached_texofs[2] = { 0.0f, 0.0f };
	int		startv, endv;

	if( !tr.num_draw_surfaces )
		return;

	RI->currentmodel = e->model;
	R_LoadIdentity();
	startv = MAX_MAP_ELEMS;
	numTempElems = 0;
	endv = 0;

	pglBindVertexArray( world->vertex_array_object );
	r_stats.c_world_polys += tr.num_draw_surfaces;

	for( int i = 0; i < world->numsortedfaces; i++ )
	{
		int j = world->sortedfaces[i];

		if( j >= worldmodel->nummodelsurfaces )
			continue;	// not a world face

		if( !CHECKVISBIT( RI->visfaces, j ))
			continue;

		msurface_t *s = worldmodel->surfaces + j;
		mextrasurf_t *es = s->info;
		bool mirror = false;
		word hProgram = 0;

		if( !FBitSet( s->flags, SURF_SCREEN ) && es->subtexture[glState.stack_position] )
			mirror = true;

		hProgram = es->shaderNum[mirror];

		if( !hProgram ) continue;

		texture_t *tex = R_TextureAnimation( s );

		if(( RI->currentshader != &glsl_programs[hProgram] ))
			flush_buffer = true;

		if( cached_lightmap != es->lightmaptexturenum )
			flush_buffer = true;

		if( cached_mirror != es->subtexture[glState.stack_position] )
			flush_buffer = true;

		if( cached_texture != tex )
			flush_buffer = true;

		if( cached_texofs[0] != es->texofs[0] || cached_texofs[1] != es->texofs[1] )
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.c_total_tris += (numTempElems / 3);
				r_stats.num_flushes++;
				startv = MAX_MAP_ELEMS;
				numTempElems = 0;
				endv = 0;
			}
			flush_buffer = false;
		}

		// begin draw the sorted list
		if(( RI->currentshader != &glsl_programs[hProgram] ))
		{
			GL_BindShader( &glsl_programs[hProgram] );			

			ASSERT( RI->currentshader != NULL );

			gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];

			// write constants
			pglUniform1fvARB( RI->currentshader->u_LightStyleValues, MAX_LIGHTSTYLES, &tr.lightstyles[0] );
			pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			pglUniformMatrix4fvARB( RI->currentshader->u_ModelMatrix, 1, GL_FALSE, &glm->modelMatrix[0] );
			pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			pglUniform1fARB( RI->currentshader->u_zFar, -RI->farClip );
			R_SetRenderColor( RI->currententity );

			// reset cache
			cached_texture = NULL;
			cached_lightmap = -1;
			cached_mirror = -1;
		}

		if( ScreenCopyRequired( RI->currentshader ))
		{
			Vector	absmin = e->origin + es->mins;
			Vector	absmax = e->origin + es->maxs;
			float	x, y, w, h;

			if( R_ScissorForAABB( absmin, absmax, &x, &y, &w, &h ))
			{
				// keep screencopy an actual
				float y2 = (float)RI->viewport[3] - h - y;
				GL_Bind( GL_TEXTURE0, tr.screen_color );
				pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, x, y2, x, y2, w, h );
				r_stats.c_screen_copy++;
				cached_texture = NULL;
				flush_buffer = true;
			}
		}

		if(( cached_mirror != es->subtexture[glState.stack_position] ) || ( cached_texture != tex ))
		{
			mtexinfo_t	*tx = s->texinfo;
			mfaceinfo_t	*land = tx->faceinfo;
			float		xScale, yScale, waveHeight;

			// set the current waveheight
			if( s->polys->verts[0][2] >= RI->vieworg[2] )
				waveHeight = -RI->currententity->curstate.scale;
			else waveHeight = RI->currententity->curstate.scale;

			if( FBitSet( s->flags, SURF_REFLECT|SURF_PORTAL|SURF_SCREEN ) && es->subtexture[glState.stack_position] )
			{
				int handle = es->subtexture[glState.stack_position];
				GL_Bind( GL_TEXTURE0, tr.subviewTextures[handle-1].texturenum );
				GL_LoadTexMatrix( tr.subviewTextures[handle-1].matrix );
			}
			else if( FBitSet( s->flags, SURF_MOVIE ) && RI->currententity->curstate.body )
			{ 
				GL_Bind( GL_TEXTURE0, tr.cinTextures[es->cintexturenum-1] );
				GL_LoadIdentityTexMatrix();
			}
			else if( CVAR_TO_BOOL( r_lightmap ) || e->curstate.rendermode == kRenderTransColor )
			{
				GL_Bind( GL_TEXTURE0, tr.whiteTexture );
				GL_LoadIdentityTexMatrix();
			}
			else if( FBitSet( s->flags, SURF_LANDSCAPE ) && land && land->terrain )
			{
				if( land->terrain->layermap.gl_diffuse_id )
					GL_Bind( GL_TEXTURE0, land->terrain->layermap.gl_diffuse_id );
				else if( land->terrain->indexmap.gl_diffuse_id && CVAR_TO_BOOL( r_detailtextures ))
					GL_Bind( GL_TEXTURE0, tr.grayTexture );
				else GL_Bind( GL_TEXTURE0, tex->gl_texturenum );
				GL_LoadIdentityTexMatrix();
			}
			else
			{
				GL_Bind( GL_TEXTURE0, tex->gl_texturenum );
				GL_LoadIdentityTexMatrix();
			}

			if( land && land->terrain && land->terrain->indexmap.gl_diffuse_id != 0 )
				GL_Bind( GL_TEXTURE1, land->terrain->indexmap.gl_diffuse_id );
			else GL_Bind( GL_TEXTURE1, tex->dt_texturenum );
			GET_DETAIL_SCALE( tex->gl_texturenum, &xScale, &yScale );
			pglUniform3fARB( RI->currentshader->u_DetailScale, xScale, yScale, waveHeight );
			pglUniform3fARB( RI->currentshader->u_TexOffset, es->texofs[0], es->texofs[1], tr.time );

			if( ScreenCopyRequired( RI->currentshader ))
				GL_Bind( GL_TEXTURE3, tr.screen_color );
			else GL_Bind( GL_TEXTURE3, tr.whiteTexture );

			if( FBitSet( s->flags, SURF_LANDSCAPE ) && land && land->terrain )
				GL_Bind( GL_TEXTURE4, land->terrain->indexmap.gl_heightmap_id );

			cached_mirror = es->subtexture[glState.stack_position];
			cached_texture = tex;
		}

		if( cached_lightmap != es->lightmaptexturenum )
		{
			if( R_FullBright( ))
			{
				// bind stubs (helper to reduce conditions in shader)
				GL_Bind( GL_TEXTURE2, tr.whiteTexture );
			}
			else
			{
				// bind real data
				GL_Bind( GL_TEXTURE2, tr.lightmaps[es->lightmaptexturenum].lightmap );
			}
			cached_lightmap = es->lightmaptexturenum;
		}

		if( FBitSet( s->flags, SURF_DRAWTURB ))
			GL_Cull( GL_NONE );
		else GL_Cull( GL_FRONT );

		cached_texofs[0] = es->texofs[0];
		cached_texofs[1] = es->texofs[1];

		if( es->firstvertex < startv )
			startv = es->firstvertex;

		if(( es->firstvertex + es->numverts ) > endv )
			endv = es->firstvertex + es->numverts;

		if( FBitSet( s->flags, SURF_DRAWTURB ))
		{
			R_DrawIndexedSurface( es );
		}
		else
		{
			R_DrawSurface( es );
		}
	}

	if( numTempElems )
	{
		pglDrawRangeElementsEXT( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.c_total_tris += (numTempElems / 3);
		r_stats.num_flushes++;
		numTempElems = 0;
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	pglBindVertexArray( GL_FALSE );
	GL_BindShader( NULL );
	GL_Cull( GL_FRONT );

	// draw grass on visible surfaces
//	R_DrawGrass();

	// draw dynamic lighting for world and bmodels
//	R_RenderDynLightList ();

	pglBindVertexArray( GL_FALSE );
	GL_BindShader( NULL );

//	DrawWireFrame();

	// clear the subview pointers after normalpass
	if( RP_NORMALPASS( ))
	{
//		for( i = 0; i < tr.num_draw_surfaces; i++ )
//			memset( tr.draw_surfaces[i].surface->info->subtexture, 0, sizeof( short[8] ));
	}
	tr.num_draw_surfaces = 0;

	// render all decals on world and opaque bmodels
//	DrawDecalsBatch();
}

/*
=================
R_SurfaceCompare

compare translucent surfaces
=================
*/
static int R_SurfaceCompare( const gl_bmodelface_t *a, const gl_bmodelface_t *b )
{
	msurface_t	*surf1, *surf2;

	surf1 = (msurface_t *)a->surface;
	surf2 = (msurface_t *)b->surface;

	Vector org1 = RI->currententity->origin + surf1->info->origin;
	Vector org2 = RI->currententity->origin + surf2->info->origin;

	// compare by plane dists
	float len1 = DotProduct( org1, RI->vforward ) - RI->viewplanedist;
	float len2 = DotProduct( org2, RI->vforward ) - RI->viewplanedist;

	if( len1 > len2 )
		return -1;
	if( len1 < len2 )
		return 1;

	return 0;
}

void R_SetRenderMode( cl_entity_t *e )
{
	pglDisable( GL_ALPHA_TEST );
	pglDepthMask( GL_TRUE );

	switch( e->curstate.rendermode )
	{
	case kRenderNormal:
		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		pglDisable( GL_BLEND );
		break;
	case kRenderTransColor:
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglColor4ub( e->curstate.rendercolor.r, e->curstate.rendercolor.g, e->curstate.rendercolor.b, e->curstate.renderamt );
		pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglDisable( GL_TEXTURE_2D );
		pglEnable( GL_BLEND );
		break;
	case kRenderTransAdd:
		pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglColor4f( tr.blend, tr.blend, tr.blend, 1.0f );
		pglBlendFunc( GL_ONE, GL_ONE );
		pglDepthMask( GL_FALSE );
		pglEnable( GL_BLEND );
		break;
	case kRenderTransAlpha:
		pglEnable( GL_ALPHA_TEST );
		pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		pglDisable( GL_BLEND );
		pglAlphaFunc( GL_GREATER, 0.25f );
		break;
	default:
		pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglColor4f( 1.0f, 1.0f, 1.0f, tr.blend );
		pglDepthMask( GL_FALSE );
		pglEnable( GL_BLEND );
		break;
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel( cl_entity_t *e )
{
	Vector		absmin, absmax;
	msurface_t	*psurf;
	model_t		*clmodel;
	mplane_t		plane;

	clmodel = e->model;

	gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];

	if( e->angles != g_vecZero )
	{
		TransformAABB( glm->transform, clmodel->mins, clmodel->maxs, absmin, absmax );
	}
	else
	{
		absmin = e->origin + clmodel->mins;
		absmax = e->origin + clmodel->maxs;
	}

	if( !Mod_CheckBoxVisible( absmin, absmax ))
		return;

	if( R_CullModel( e, absmin, absmax ))
		return;

	tr.num_draw_surfaces = 0;

	if( e->angles != g_vecZero )
		tr.modelorg = glm->transform.VectorITransform( RI->vieworg );
	else tr.modelorg = RI->vieworg - e->origin;
	R_GrassPrepareFrame();

	// accumulate surfaces, build the lightmaps
	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( FBitSet( psurf->flags, SURF_DRAWTURB ))
		{
			if( FBitSet( psurf->flags, SURF_PLANEBACK ))
				SetPlane( &plane, -psurf->plane->normal, -psurf->plane->dist );
			else SetPlane( &plane, psurf->plane->normal, psurf->plane->dist );

			if( e->hCachedMatrix != WORLD_MATRIX )
				glm->transform.TransformPositivePlane( plane, plane );

			if( psurf->plane->type != PLANE_Z && !FBitSet( e->curstate.effects, EF_WATERSIDES ))
				continue;

			if( absmin[2] + 1.0 >= plane.dist )
				continue;
		}

		// in some cases surface is invisible but grass is visible
		bool force = R_AddGrassToChain( psurf, &RI->frustum );

		int cull_type = R_CullSurface( psurf ); // ignore frustum for bmodels

		if( !force && cull_type >= CULL_FRUSTUM )
			continue;

		if( cull_type == CULL_BACKSIDE )
		{
			if( !FBitSet( psurf->flags, SURF_DRAWTURB ) && !( psurf->pdecals && e->curstate.rendermode == kRenderTransTexture ))
				continue;
		}

		// FIXME: store the cull type
		R_AddSurfaceToDrawList( psurf, false );
	}

	if( e->curstate.rendermode == kRenderTransTexture && !FBitSet( clmodel->flags, MODEL_LIQUID ))
		qsort( tr.draw_surfaces, tr.num_draw_surfaces, sizeof( gl_bmodelface_t ), (cmpfunc)R_SurfaceCompare );

	R_SetRenderMode( e );
	R_DrawBrushList();

	R_LoadIdentity();	// restore worldmatrix
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModelShadow( cl_entity_t *e )
{
	Vector		absmin, absmax;
	msurface_t	*psurf;
	model_t		*clmodel;

	clmodel = e->model;

	gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];

	if( e->angles != g_vecZero )
	{
		TransformAABB( glm->transform, clmodel->mins, clmodel->maxs, absmin, absmax );
	}
	else
	{
		absmin = e->origin + clmodel->mins;
		absmax = e->origin + clmodel->maxs;
	}

	if( !Mod_CheckBoxVisible( absmin, absmax ))
		return;

	if( R_CullModel( e, absmin, absmax ))
		return;

	tr.num_draw_surfaces = 0;

	if( e->angles != g_vecZero )
		tr.modelorg = glm->transform.VectorITransform( RI->vieworg );
	else tr.modelorg = RI->vieworg - e->origin;
	R_GrassPrepareFrame();

	// accumulate surfaces, build the lightmaps
	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		// in some cases surface is invisible but grass is visible
		bool force = R_AddGrassToChain( psurf, &RI->frustum );

		int cull_type = R_CullSurface( psurf ); // ignore frustum for bmodels

		if( !force && cull_type >= CULL_FRUSTUM )
			continue;

		if( cull_type == CULL_BACKSIDE )
			continue;

		R_AddSurfaceToDrawList( psurf, false );
	}

	R_DrawShadowBrushList();
	R_LoadIdentity();	// restore worldmatrix
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/
void R_UpdateEngineVisibility( void )
{
	// just update shared PVS because it's reset each frame
	for( int j = 0; j < tr.pvssize; j++ )
		SetBits( tr.visbytes[j], RI->visbytes[j] );
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current leaf
===============
*/
void R_MarkLeaves( void )
{
	bool		novis = false;
	mworldnode_t	*node;

	if( FBitSet( r_novis->flags, FCVAR_CHANGED ) || tr.fResetVis )
	{
		// force recalc viewleaf
		ClearBits( r_novis->flags, FCVAR_CHANGED );
		tr.fResetVis = false;
		RI->viewleaf = NULL;
	}

	if( RI->viewleaf == RI->oldviewleaf && RI->viewleaf != NULL )
	{
		R_UpdateEngineVisibility();
		return;
	}

	// development aid to let you run around
	// and see exactly where the pvs ends
	if( r_lockpvs->value )
	{
		R_UpdateEngineVisibility();
		return;
	}

	// cache current values
	RI->oldviewleaf = RI->viewleaf;

	if( r_novis->value || FBitSet( RI->params, RP_OVERVIEW ) || !worldmodel->visdata )
		novis = true;

	ENGINE_SET_PVS( RI->pvsorigin, REFPVS_RADIUS, RI->visbytes, FBitSet( RI->params, RP_MERGEVISIBILITY ), novis );
	R_UpdateEngineVisibility();
	RI->visframecount++;

	int stack = glState.stack_position;

	for( int i = 0; i < world->numleafs - 1; i++ )
	{
		if( CHECKVISBIT( RI->visbytes, i ))
		{
			node = (mworldnode_t *)&world->leafs[i+1];
			do
			{
				if( node->visframe[stack] == RI->visframecount )
					break;
				node->visframe[stack] = RI->visframecount;
				node = node->parent;
			} while( node );
		}
	}
}

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode( mworldnode_t *node, unsigned int clipflags )
{
	int		stack = glState.stack_position;
	msurface_t	*surf, **mark;
	mworldleaf_t	*pleaf;
	int		c, side;
	float		dot;
loc0:
	if( node->contents == CONTENTS_SOLID )
		return; // hit a solid leaf

	if( node->visframe[stack] != RI->visframecount )
		return;

	if( clipflags )
	{
		for( int i = 0; i < FRUSTUM_PLANES; i++ )
		{
			if( !FBitSet( clipflags, BIT( i )))
				continue;

			const mplane_t *p = RI->frustum.GetPlane( i );
			int clipped = BoxOnPlaneSide( node->mins, node->maxs, p );
			if( clipped == 2 ) return;
			if( clipped == 1 ) ClearBits( clipflags, BIT( i ));
		}
	}

	// if a leaf node, draw stuff
	if( node->contents < 0 )
	{
		pleaf = (mworldleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if( c )
		{
			do
			{
				surf = *mark;

				// update leaf bounds with grass bounds
				if( CVAR_TO_BOOL( r_grass ))
					R_AddGrassToChain( surf, NULL, false, pleaf );
				SETVISBIT( RI->visfaces, surf - worldmodel->surfaces );
				mark++;
			} while( --c );
		}

		// deal with model fragments in this leaf
		if( pleaf->efrags )
			STORE_EFRAGS( pleaf->efrags, tr.realframecount );
		r_stats.c_world_leafs++;
		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	dot = PlaneDiff( tr.modelorg, node->plane );
	side = (dot >= 0.0f) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode( node->children[side], clipflags );

	// draw stuff
	for( c = node->numsurfaces, surf = worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		// in some cases surface is invisible but grass is not
		if( CVAR_TO_BOOL( r_grass ))
			R_AddGrassToChain( surf, &RI->frustum );

		if( !CHECKVISBIT( RI->visfaces, surf - worldmodel->surfaces ))
			continue;	// not visible

		bool backplane = FBitSet( surf->flags, SURF_PLANEBACK ) ? true : false;

		if( FBitSet( RI->params, RP_OVERVIEW ))
			dot = surf->plane->normal[2];
		else dot = PlaneDiff( tr.modelorg, surf->plane );

		if(( backplane && dot >= -BACKFACE_EPSILON ) || ( !backplane && dot <= BACKFACE_EPSILON ))
			continue; // wrong side

		if( RI->frustum.CullBox( surf->info->mins, surf->info->maxs ))
			continue;

		if( FBitSet( surf->flags, SURF_DRAWSKY ))
		{
			if( FBitSet( RI->params, RP_SHADOWVIEW ))
				continue;

			if( tr.fIgnoreSkybox )
				continue;

			R_AddSkyBoxSurface( surf );
		}
		else
		{
			R_AddSurfaceToDrawList( surf, false );
		}
	}

	// recurse down the back side
	node = node->children[!side];
	goto loc0;
}

void R_WorldMarkVisibleFaces( void )
{
	msurface_t	*surf, **mark;
	mextrasurf_t	*esrf;
	mworldleaf_t	*leaf;
	int		i, j;

	// always skip the leaf 0, because is outside leaf
	for( i = 1, leaf = &world->leafs[1]; i < world->numleafs; i++, leaf++ )
	{
		if( CHECKVISBIT( RI->visbytes, leaf->cluster ) && ( leaf->efrags || leaf->nummarksurfaces ))
		{
			if( RI->frustum.CullBox( leaf->mins, leaf->maxs ))
				continue;

			// do additional culling in dev_overview mode
			if( FBitSet( RI->params, RP_OVERVIEW ) && R_CullNodeTopView( (mworldnode_t *)leaf ))
				continue;

			// deal with model fragments in this leaf
			if( leaf->efrags )
				STORE_EFRAGS( leaf->efrags, tr.realframecount );
			r_stats.c_world_leafs++;

			if( leaf->nummarksurfaces <= 0 )
				continue;

			for( j = 0, mark = leaf->firstmarksurface; j < leaf->nummarksurfaces; j++, mark++ )
			{
				int facenum = *mark - worldmodel->surfaces;
				bool force = false, backplane;
				float dist;

				if( CHECKVISBIT( RI->visfaces, facenum ))
					continue;	// already checked
				SETVISBIT( RI->visfaces, facenum ); // don't bother check it again

				surf = worldmodel->surfaces + facenum;
				esrf = surf->info;

				if( CVAR_TO_BOOL( r_grass ))
					R_AddGrassToChain( surf, NULL, false, leaf ); // update leaf bounds

				// in some cases surface is invisible but grass is visible
				if( CVAR_TO_BOOL( r_grass ))
					force = R_AddGrassToChain( surf, &RI->frustum );
#if 1
				if( !force )
				{
					backplane = FBitSet( surf->flags, SURF_PLANEBACK ) ? true : false;
					if( FBitSet( RI->params, RP_OVERVIEW ))
						dist = surf->plane->normal[2];
					else dist = PlaneDiff( tr.modelorg, surf->plane );

					if(( backplane && dist >= -BACKFACE_EPSILON ) || ( !backplane && dist <= BACKFACE_EPSILON ))
						continue; // wrong side

					if( RI->frustum.CullBox( surf->info->mins, surf->info->maxs ))
						continue;
				}
#else
				if( !force && R_CullSurface( surf ))
					continue;
#endif
				if( FBitSet( surf->flags, SURF_DRAWSKY ))
				{
					if( FBitSet( RI->params, RP_SHADOWVIEW ))
						continue;

					if( tr.fIgnoreSkybox )
						continue;

					R_AddSkyBoxSurface( surf );
				}
				else
				{
					R_AddSurfaceToDrawList( surf, false );
				}

			}
		}
	}
#if 0
	// create drawlist for faces, do additional culling for world faces
	for( i = 0; i < world->numsortedfaces; i++ )
	{
		j = world->sortedfaces[i];

		if( j >= worldmodel->nummodelsurfaces )
			continue;	// not a world face

		if( CHECKVISBIT( RI->visfaces, j ))
		{
			surf = worldmodel->surfaces + j;
			esrf = surf->info;

			RI->currententity = GET_ENTITY( 0 );
			RI->currentmodel = RI->currententity->model;

			// in some cases surface is invisible but grass is visible
			bool force = R_AddGrassToChain( surf, &RI->frustum );

			if( !force && R_CullSurface( surf ))
			{
				CLEARVISBIT( RI->visfaces, j ); // not visible
				continue;
			}

			if( FBitSet( surf->flags, SURF_DRAWSKY ))
			{
				if( FBitSet( RI->params, RP_SHADOWVIEW ))
					continue;

				if( tr.fIgnoreSkybox )
					continue;

				R_AddSkyBoxSurface( surf );
			}
			else
			{
				R_AddSurfaceToDrawList( surf, false );
			}
		}
	}
#endif
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	double	start, end;

	RI->currententity = GET_ENTITY( 0 );
	RI->currentmodel = RI->currententity->model;
	memset( RI->visfaces, 0x00, ( world->numsortedfaces + 7) >> 3 );

	tr.modelorg = RI->vieworg;

	R_SetRenderMode( RI->currententity );
	R_GrassPrepareFrame();
	R_LoadIdentity();
	R_ClearSkyBox ();

	R_MarkLeaves();

	start = Sys_DoubleTime();
	if( CVAR_TO_BOOL( r_recursive_world_node ))
		R_RecursiveWorldNode( world->nodes, RI->frustum.GetClipFlags( ));
	else R_WorldMarkVisibleFaces();
	end = Sys_DoubleTime();

	r_stats.t_world_node = end - start;

	start = Sys_DoubleTime();
//	if( CVAR_TO_BOOL( r_test ))
//		R_DrawWorldList();
//	else
		R_DrawBrushList();
	end = Sys_DoubleTime();

	r_stats.t_world_draw = end - start;

	if( FBitSet( RI->params, RP_SKYVISIBLE ))
		R_DrawSkyBox();
}

void R_DrawWorldShadowPass( void )
{
	RI->currententity = GET_ENTITY( 0 );
	RI->currentmodel = RI->currententity->model;
	tr.modelorg = RI->vieworg;
	R_GrassPrepareFrame();
	R_LoadIdentity();

	R_MarkLeaves();
	R_WorldMarkVisibleFaces();
	R_DrawShadowBrushList();
}
