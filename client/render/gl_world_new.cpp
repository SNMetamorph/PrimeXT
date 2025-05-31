/*
gl_world.cpp - world and bmodel rendering
Copyright (C) 2016 Uncle Mike

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
#include "gl_local.h"
#include "pm_defs.h"
#include "event_api.h"
#include <stringlib.h>
#include "gl_studio.h"
#include "gl_shader.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_occlusion.h"
#include "gl_cvars.h"
#include "vertex_fmt.h"
#include "brush_material.h"

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

gl_texbuffer_t *Surf_GetSubview( mextrasurf_t *es )
{
	ASSERT( glState.stack_position >= 0 && glState.stack_position < MAX_REF_STACK );
	int handle = es->subtexture[glState.stack_position];

	if( handle > 0 && handle <= MAX_SUBVIEW_TEXTURES )
		return &tr.subviewTextures[handle-1];
	return NULL;
}

bool Surf_CheckSubview( mextrasurf_t *es, bool puddle )
{
	ASSERT( glState.stack_position >= 0 && glState.stack_position < MAX_REF_STACK );
	int handle = es->subtexture[glState.stack_position];
	if( !handle ) return false;

	ASSERT( handle > 0 && handle <= MAX_SUBVIEW_TEXTURES );

	// we can't directly compare here
	if(( tr.realframecount - tr.subviewTextures[handle-1].texframe ) <= 1 )
	{
		if (puddle)
		{
			if (FBitSet(es->surf->flags, SURF_REFLECT_PUDDLE))
				return true;
		}
		else 
		{
			if (FBitSet(es->surf->flags, SURF_REFLECT | SURF_PORTAL | SURF_SCREEN))
				return true;
		}
	}
	return false;
}

/*
=============================================================

	WORLD LOADING

=============================================================
*/

/*
========================
Mod_CopyMaterialDesc

copy params from description
to real material struct
========================
*/
static void Mod_CopyMaterialDesc( material_t *mat, const matdesc_t *desc )
{
	mat->impl->smoothness = desc->smoothness;
	mat->impl->detailScale[0] = desc->detailScale[0];
	mat->impl->detailScale[1] = desc->detailScale[1];
	mat->impl->reflectScale = desc->reflectScale;
	mat->impl->refractScale = desc->refractScale;
	mat->impl->aberrationScale = desc->aberrationScale;
	mat->impl->reliefScale = desc->reliefScale;
	mat->effects = desc->effects;
}

/*
========================
Mod_LoadWorldMaterials

build a material for each world texture
========================
*/
static void Mod_LoadWorldMaterials( void )
{
	char	diffuse[128], bumpmap[128];
	char	glossmap[128], glowmap[128];
	char	heightmap[128];

	world->materials = (material_t *)Mem_Alloc( sizeof( material_t ) * worldmodel->numtextures );

	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		texture_t *tx = worldmodel->textures[i];
		material_t *mat = &world->materials[i];

		// bad texture? 
		if( !tx || !tx->name[0] ) 
			continue;

		// make cross-links for consistency
		tx->material = mat;
		mat->pSource = tx;
		mat->impl = std::make_shared<mbrushmaterial_t>();

		// setup material constants
		matdesc_t *desc = CL_FindMaterial(tx->name);
		Mod_CopyMaterialDesc(mat, desc);

		// build material names
		if (Q_strlen(desc->diffusemap) && IMAGE_EXISTS(desc->diffusemap))
			Q_strncpy(diffuse, desc->diffusemap, sizeof(diffuse));
		else
			Q_snprintf(diffuse, sizeof(diffuse), "textures/%s", tx->name);

		if (Q_strlen(desc->normalmap) && IMAGE_EXISTS(desc->normalmap))
			Q_strncpy(bumpmap, desc->normalmap, sizeof(bumpmap));
		else
			Q_snprintf(bumpmap, sizeof(bumpmap), "textures/%s_norm", tx->name);

		if (Q_strlen(desc->glossmap) && IMAGE_EXISTS(desc->glossmap))
			Q_strncpy(glossmap, desc->glossmap, sizeof(glossmap));
		else
			Q_snprintf(glossmap, sizeof(glossmap), "textures/%s_gloss", tx->name);

		Q_snprintf( glowmap, sizeof( glowmap ), "textures/%s_luma", tx->name );
		Q_snprintf( heightmap, sizeof( heightmap ), "textures/%s_hmap", tx->name );

		// albedo/diffuse map loading
		TextureHandle wadTexture = tx;
		if( IMAGE_EXISTS( diffuse ))
		{
			mat->impl->gl_diffuse_id = LOAD_TEXTURE( diffuse, NULL, 0, 0 );
			if (wadTexture != tr.defaultTexture) {
				FREE_TEXTURE(wadTexture); // release wad-texture
			}

			// so engine can be draw HQ image for gl_renderer 0
			// FIXME: what about detail texture scales ?
			tx->gl_texturenum = mat->impl->gl_diffuse_id.ToInt();

			if (RENDER_GET_PARM(PARM_TEX_FLAGS, tx->gl_texturenum) & TF_HAS_ALPHA) {
				mat->flags |= BRUSH_HAS_ALPHA;
			}
		}
		else
		{
			// use texture from wad
			mat->impl->gl_diffuse_id = wadTexture;
		}

		// normal map loading
		if( IMAGE_EXISTS( bumpmap ))
		{
			mat->impl->gl_normalmap_id = LOAD_TEXTURE( bumpmap, NULL, 0, TF_NORMALMAP );
		}
		else
		{
			// try alternate suffix
			Q_snprintf( bumpmap, sizeof( bumpmap ), "textures/%s_local", tx->name );
			if( IMAGE_EXISTS( bumpmap ))
				mat->impl->gl_normalmap_id = LOAD_TEXTURE( bumpmap, NULL, 0, TF_NORMALMAP );
			else mat->impl->gl_normalmap_id = tr.normalmapTexture; // blank bumpy
        }

		// gloss/PBR map loading
		if( IMAGE_EXISTS( glossmap ))
		{
			mat->impl->gl_specular_id = LOAD_TEXTURE( glossmap, NULL, 0, 0 );
		}
		else
		{
			// try alternate suffix
			Q_snprintf( glossmap, sizeof( glossmap ), "textures/%s_spec", tx->name );
			if( IMAGE_EXISTS( glossmap ))
				mat->impl->gl_specular_id = LOAD_TEXTURE( glossmap, NULL, 0, 0 );
			else mat->impl->gl_specular_id = tr.blackTexture;
		}

		// height map loading
		if( IMAGE_EXISTS( heightmap ))
		{
			mat->impl->gl_heightmap_id = LOAD_TEXTURE( heightmap, NULL, 0, 0 );
		}
		else
		{
			// try alternate suffix
			Q_snprintf( heightmap, sizeof( heightmap ), "textures/%s_bump", tx->name );
			if( IMAGE_EXISTS( heightmap ))
				mat->impl->gl_heightmap_id = LOAD_TEXTURE( heightmap, NULL, 0, 0 );
			else mat->impl->gl_heightmap_id = tr.blackTexture;
		}

		// luma/emission map loading
		if (IMAGE_EXISTS(glowmap)) {
			mat->impl->gl_glowmap_id = LOAD_TEXTURE(glowmap, NULL, 0, 0);
		}
		else {
			mat->impl->gl_glowmap_id = tr.blackTexture;
		}

		// detail map loading
		if (IMAGE_EXISTS(desc->detailmap)) {
			mat->impl->gl_detailmap_id = LOAD_TEXTURE(desc->detailmap, NULL, 0, TF_FORCE_COLOR);
		}
		else {
			mat->impl->gl_detailmap_id = tr.grayTexture;
		}

		// setup material flags
		if( mat->impl->gl_normalmap_id.Initialized() && mat->impl->gl_normalmap_id != tr.normalmapTexture)
			SetBits( mat->flags, BRUSH_HAS_BUMP );

		if( mat->impl->gl_specular_id.Initialized() && mat->impl->gl_specular_id != tr.blackTexture )
			SetBits( mat->flags, BRUSH_HAS_SPECULAR );

		if( mat->impl->gl_glowmap_id.Initialized() && mat->impl->gl_glowmap_id != tr.blackTexture )
			SetBits( mat->flags, BRUSH_HAS_LUMA );

		if( mat->impl->gl_heightmap_id.Initialized() && mat->impl->gl_heightmap_id != tr.blackTexture )
			SetBits( mat->flags, BRUSH_HAS_HEIGHTMAP );

		if( mat->impl->gl_detailmap_id.Initialized() && mat->impl->gl_detailmap_id != tr.grayTexture )
			SetBits( mat->flags, BRUSH_HAS_DETAIL );

		if( tx->name[0] == '{' )
			SetBits( mat->flags, BRUSH_TRANSPARENT );

		if( !Q_strnicmp( tx->name, "scroll", 6 ))
			SetBits( mat->flags, BRUSH_CONVEYOR );

		if( !Q_strnicmp( tx->name, "{scroll", 7 ))
			SetBits( mat->flags, BRUSH_CONVEYOR|BRUSH_TRANSPARENT );

		if( !Q_strncmp( tx->name, "mirror", 6 ) || !Q_strncmp( tx->name, "reflect", 7 ))
			SetBits( mat->flags, BRUSH_REFLECT );

		if( !Q_strncmp( tx->name, "movie", 5 ))
			SetBits( mat->flags, BRUSH_FULLBRIGHT );

		bool liquidScrollFlag = !Q_strncmp(tx->name, "!scroll", 7);
		if (tx->name[0] == '!' || !Q_strncmp(tx->name, "water", 5) || liquidScrollFlag)
		{
			// liquid surface should be smooth and reflective
			SetBits(mat->flags, BRUSH_REFLECT | BRUSH_LIQUID);
			mat->impl->smoothness = 1.0f;
			mat->impl->reflectScale = 1.0f; 
			mat->impl->refractScale = 2.5f;
			
			if (tr.waterTextures[0].Initialized()) {
				SetBits(mat->flags, BRUSH_HAS_BUMP);
			}

			if (liquidScrollFlag) {
				SetBits( mat->flags, BRUSH_CONVEYOR );
			}
		}

		if( !Q_strncmp( tx->name, "sky", 3 ))
			SetBits( world->features, WORLD_HAS_SKYBOX );
	}
}

static void Mod_SetupLeafExtradata( const dlump_t *l, const dlump_t *vis, const byte *buf )
{
	dleaf_t 		*in = (dleaf_t *)(buf + l->fileofs);
	mextraleaf_t	*out;

	world->numleafs = worldmodel->numleafs + 1; // world leafs + outside common leaf 
	world->leafs = out = (mextraleaf_t *)Mem_Alloc( sizeof( mextraleaf_t ) * world->numleafs );
	world->totalleafs = l->filelen / sizeof( *in ); // keep the total leaf counting

	for( int i = 0; i < world->numleafs; i++, in++, out++ )
	{
		VectorCopy( in->mins, out->mins );
		VectorCopy( in->maxs, out->maxs );
	}
}

/*
=================
Mod_SetupLeafLights
=================
*/
static void Mod_SetupLeafLights( void )
{
	mextraleaf_t	*out;
	int		i, j, k;
	mleaf_t		*leaf;
	mworldlight_t	*wl;
	mlightprobe_t	*lp;

	out = (mextraleaf_t *)world->leafs;
	wl = world->worldlights;
	lp = world->leaflights;
	j = k = 0;

	for( i = 0; i < world->numleafs; i++, out++ )
	{
		leaf = INFO_LEAF( out, worldmodel );

		// NOTE: lights already sorted by leafs
		if( world->numworldlights > 0 && wl->leaf == leaf )
		{
			out->direct_lights = wl; // pointer to first light in the array that belong to this leaf

			for( ;( j < world->numworldlights ) && (wl->leaf == leaf); j++, wl++ )
				out->num_directlights++;
		}

		if( world->numleaflights > 0 && lp->leaf == leaf )
		{
			out->ambient_light = lp; // pointer to first light in the array that belong to this leaf

			for( ;( k < world->numleaflights ) && (lp->leaf == leaf); k++, lp++ )
				out->num_lightprobes++;
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
	byte		*data;

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
	}
	else
	{
		// old method...
		int	count;
		dnormal_t	*in;

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
=============
BuildVisForDLight

create visibility cache for dlight
=============
*/
static int Mod_BuildVisForDLight( mworldlight_t *wl )
{
	int	leafnum;

	if( wl->emittype == emit_skylight )
	{
		// all leafs that contain skyface should be added to sun visibility
		for( leafnum = 0; leafnum < worldmodel->numleafs; leafnum++ )
		{
			msurface_t **mark = worldmodel->leafs[leafnum + 1].firstmarksurface;

			for( int markface = 0; markface < worldmodel->leafs[leafnum + 1].nummarksurfaces; markface++, mark++ )
			{
				msurface_t *surf = *mark;

				if( FBitSet( surf->flags, SURF_DRAWSKY ))
				{
					MergeDLightVis( wl, leafnum + 1 );
					break; // no reason to check all faces, go to next leaf
				}
			}
		}

		// technically light_environment is outside of world
		return -1;
	}
	else
	{
		leafnum = Mod_PointInLeaf( wl->origin, worldmodel->nodes ) - worldmodel->leafs;
		SetDLightVis( wl, leafnum );

		return leafnum;
	}
}

/*
=================
Mod_LoadWorldLights
=================
*/
static void Mod_LoadWorldLights( const byte *base, const dlump_t *l )
{
	dworldlight_t	*in;
	mworldlight_t	*out, *out2;
	int		i, count, dup = 0;
	int		total = 0;

	if( !l->filelen ) return;
	
	in = (dworldlight_t *)(base + l->fileofs);
	if( l->filelen % sizeof( *in ))
	{
		ALERT( at_error, "Mod_LoadWorldLights: funny lump size in %s\n", world->name );
		return;
	}
	count = l->filelen / sizeof( *in );

	world->worldlights = out = (mworldlight_t *)Mem_Alloc( count * sizeof( *out ));
	world->numworldlights = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->emittype = (emittype_t)in->emittype;
		out->style = in->style;

		VectorCopy( in->origin, out->origin );
		VectorCopy( in->intensity, out->intensity );
		VectorCopy( in->normal, out->normal );

		out->stopdot = in->stopdot;
		out->stopdot2 = in->stopdot2;
		out->fade = in->fade;
		out->leaf = &worldmodel->leafs[in->leafnum];
		out->radius = in->radius;
		out->falloff = in->falloff;

		if( out->emittype == emit_surface )
			out->surface = worldmodel->surfaces + in->facenum;
		out->modelnum = in->modelnumber;
		out->lightnum = i; // !!!
		out->shadow_x = 0xFFFF;
		out->shadow_y = 0xFFFF;

		if( out->emittype == emit_skylight )
			out->stopdot2 = -1.0f;
	}

	out = world->worldlights;

	for( i = dup = 0; i < count; i++, out++ )
	{
		if( out->intensity == g_vecZero )
			out->emittype = emit_ignored;

		if( out->emittype != emit_surface )
			continue;

		VectorMA( out->origin, 1.0f, out->normal, out->origin );
		SetBits( out->surface->flags, SURF_FULLBRIGHT );	// emit faces is always fullbright
		SetBits( out->surface->texinfo->texture->material->flags, BRUSH_FULLBRIGHT );

		out2 = world->worldlights;

		for( int j = 0; j < count; j++, out2++ )
		{
			if( out == out2 )
				continue;	// himself

			if( out2->emittype != emit_surface )
				continue;

			if( out->surface == out2->surface )
			{
				out2->emittype = emit_ignored;
				dup++;
			}
		}
	}

	out = world->worldlights;

	for( i = 0; i < count; i++, out++ )
	{
		if( out->emittype == emit_ignored )
			continue;

		Mod_BuildVisForDLight( out );
		total++;
	}

	// alloc shadow occlusion buffers
	world->shadowzbuffers = (lightzbuffer_t *)Mem_Alloc( count * sizeof( lightzbuffer_t ));
	ALERT( at_console, "%d world lights\n", total );
}

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

static void Mod_LoadSurfaceLighting( const byte *base, const dlump_t *l )
{
	dvlightlump_t	*vl;

	if( !l->filelen ) return;

	vl = (dvlightlump_t *)(base + l->fileofs);

	if( vl->ident != FLIGHTIDENT )
		return; // probably it's LUMP_LEAF_LIGHTING

	if( vl->version != FLIGHT_VERSION )
		return; // old version?

	if( vl->nummodels <= 0 ) return;

	world->surface_lighting = (dvlightlump_t *)Mem_Alloc( l->filelen );
	memcpy( world->surface_lighting, vl, l->filelen );
}

/*
=================
Mod_LoadVisLightData

worldlights visibility per face
=================
*/
static void Mod_LoadVisLightData( const byte *base, const dlump_t *l )
{
	if( !l->filelen ) return;

	world->vislightdata = (byte *)Mem_Alloc( l->filelen );
	memcpy( world->vislightdata, (byte *)(base + l->fileofs), l->filelen );
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
	short		curleaf = -1;
	mextraleaf_t	*leaf = NULL;

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
		out->leaf = &worldmodel->leafs[in->leafnum];
		VectorCopy( in->origin, out->origin );
	}
}

/*
=================
Mod_SurfaceCompareBuild

sort faces before lightmap building
=================
*/
static int Mod_SurfaceCompareBuild( const unsigned short *a, const unsigned short *b )
{
	msurface_t	*surf1, *surf2;

	surf1 = &worldmodel->surfaces[*a];
	surf2 = &worldmodel->surfaces[*b];

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
static int Mod_SurfaceCompareInGame( const unsigned short *a, const unsigned short *b )
{
	msurface_t	*surf1, *surf2;
	mextrasurf_t	*esrf1, *esrf2;

	surf1 = &worldmodel->surfaces[*a];
	surf2 = &worldmodel->surfaces[*b];

	esrf1 = surf1->info;
	esrf2 = surf2->info;

	if( esrf1->forwardScene[0].GetHandle() > esrf2->forwardScene[0].GetHandle() )
		return 1;

	if( esrf1->forwardScene[0].GetHandle() < esrf2->forwardScene[0].GetHandle() )
		return -1;

	if( surf1->texinfo->texture->gl_texturenum > surf2->texinfo->texture->gl_texturenum )
		return 1;

	if( surf1->texinfo->texture->gl_texturenum < surf2->texinfo->texture->gl_texturenum )
		return -1;

	if( esrf1->lightmaptexturenum > esrf2->lightmaptexturenum )
		return 1;

	if( esrf1->lightmaptexturenum < esrf2->lightmaptexturenum )
		return -1;

	if( !esrf1->parent || !esrf2->parent )
		return 0;

	if( esrf1->parent->hCachedMatrix > esrf2->parent->hCachedMatrix )
		return 1;

	if( esrf1->parent->hCachedMatrix < esrf2->parent->hCachedMatrix )
		return -1;

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
static void Mod_FinalizeWorld( void )
{
	world->sortedfaces = (unsigned short *)Mem_Alloc( worldmodel->numsurfaces * sizeof( unsigned short ));
	world->numsortedfaces = worldmodel->numsurfaces;

	// initial filling
	for (int i = 0; i < worldmodel->numsurfaces; i++)
		world->sortedfaces[i] = i;

	qsort(world->sortedfaces, worldmodel->numsurfaces, sizeof( unsigned short ), (cmpfunc)Mod_SurfaceCompareBuild);

	// alloc surface lightmaps and compute lm coords (for sorted list)
	for (int i = 0; i < worldmodel->numsurfaces; i++)
	{
		msurface_t *surf = &worldmodel->surfaces[world->sortedfaces[i]];

		// allocate the lightmap coords, create lightmap textures (empty at this moment)
		GL_AllocLightmapForFace( surf );
	}
}

/*
=================
Mod_ShaderSceneForward

compute albedo with static lighting
=================
*/
static word Mod_ShaderSceneForward( msurface_t *s )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	mextrasurf_t *es = s->info;
	cl_entity_t *e = es->parent ? es->parent : GET_ENTITY( 0 );

	// don't cache shader for skyfaces!
	if( FBitSet( s->flags, SURF_DRAWSKY ))
		return 0;

	// mirror is actual only if we has actual screen texture!
	bool surf_monitor = FBitSet(s->flags, SURF_SCREEN);
	bool surf_portal = FBitSet(s->flags, SURF_PORTAL);
	bool surf_movie = FBitSet(s->flags, SURF_MOVIE);
	bool mirror = Surf_CheckSubview( s->info ) && !surf_monitor && !surf_portal && !surf_movie;

	if( es->forwardScene[mirror].IsValid() && es->lastRenderMode == e->curstate.rendermode )
		return es->forwardScene[mirror].GetHandle(); // valid

	Q_strncpy( glname, "forward/scene_bmodel", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	mfaceinfo_t *landscape = landscape = s->texinfo->faceinfo;
	material_t *mat = R_TextureAnimation( s )->material;
	bool shader_use_screencopy = false;
	bool shader_additive = false;
	bool using_cubemaps = false;
	bool using_normalmap = false;
	bool fullbright = false;
	bool cubemaps_available = (world->num_cubemaps > 0) && CVAR_TO_BOOL(r_cubemap);
	bool using_refractions = mat->impl->refractScale > 0.0f;
	bool using_aberrations = mat->impl->aberrationScale > 0.0f;
	bool surf_transparent = false;
	
	if ((FBitSet(mat->flags, BRUSH_FULLBRIGHT) || R_FullBright() || mirror) && !FBitSet(mat->flags, BRUSH_LIQUID)) {
		fullbright = true;
	}

	if( e->curstate.rendermode == kRenderTransAdd )
	{
		shader_additive = true;
		fullbright = true;
	}

	if( fullbright )	
	{
		GL_AddShaderDirective( options, "LIGHTING_FULLBRIGHT" );
	}
	else
	{
		// process lightstyles
		for( int i = 0; i < MAXLIGHTMAPS && s->styles[i] != LS_NONE; i++ )
		{
			if (!R_UseSkyLightstyle(s->styles[i]))
				continue;	// skip the sunlight due realtime sun is enabled
			GL_AddShaderDirective( options, va( "APPLY_STYLE%i", i ));
		}

		if (CVAR_TO_BOOL(cv_brdf))
		{
			GL_AddShaderDirective(options, "APPLY_PBS");
			using_cubemaps = true;
		}

		// NOTE: deluxemap and normalmap are separate because some modes may using
		// normalmap directly e.g. for mirror distorsion
		if( es->normals )
		{
			GL_AddShaderDirective( options, "HAS_DELUXEMAP" );
			GL_AddShaderDirective( options, "COMPUTE_TBN" );
		}

		if( r_lightmap->value > 0.0f && r_lightmap->value <= 2.0f )
		{
			if( r_lightmap->value == 1.0f && worldmodel && worldmodel->lightdata )
				GL_AddShaderDirective( options, "LIGHTMAP_DEBUG" );
			else if( r_lightmap->value == 2.0f && FBitSet( world->features, WORLD_HAS_DELUXEMAP ))
				GL_AddShaderDirective( options, "LIGHTVEC_DEBUG" );
		}

		if( !RP_CUBEPASS() && ( CVAR_TO_BOOL( cv_specular ) && FBitSet( mat->flags, BRUSH_HAS_SPECULAR )))
			GL_AddShaderDirective( options, "HAS_GLOSSMAP" );

		if( FBitSet( mat->flags, BRUSH_HAS_LUMA ))
			GL_AddShaderDirective( options, "HAS_LUMA" );
	}

	if (surf_monitor) 
	{
		GL_AddShaderDirective(options, "MONITOR_BRUSH");
		if (FBitSet(e->curstate.iuser1, CF_MONOCHROME))
			GL_AddShaderDirective(options, "MONOCHROME");
	}
	else if (surf_movie)
	{
		if (FBitSet(e->curstate.iuser1, CF_MONOCHROME))
			GL_AddShaderDirective(options, "MONOCHROME");
	}

	if( FBitSet( mat->flags, BRUSH_MULTI_LAYERS ) && landscape && landscape->terrain )
	{
		GL_AddShaderDirective( options, va( "TERRAIN_NUM_LAYERS %i", landscape->terrain->numLayers ));
		GL_AddShaderDirective( options, "APPLY_TERRAIN" );

		if( landscape->terrain->indexmap.gl_diffuse_id.Initialized() && CVAR_TO_BOOL(r_detailtextures))
			GL_AddShaderDirective( options, "HAS_DETAIL" );
	}
	else
	{
		if( FBitSet( mat->flags, BRUSH_HAS_DETAIL ) && CVAR_TO_BOOL( r_detailtextures ))
			GL_AddShaderDirective( options, "HAS_DETAIL" );
	}

	if( !RP_CUBEPASS() && ( FBitSet( mat->flags, BRUSH_HAS_BUMP ) && (CVAR_TO_BOOL( cv_bump ) || FBitSet( mat->flags, BRUSH_LIQUID ))))
	{
		// FIXME: all the waternormals should be encoded as first frame
		if( FBitSet( mat->flags, BRUSH_LIQUID ))
			GL_EncodeNormal( options, tr.waterTextures[0] );
		else 
			GL_EncodeNormal( options, mat->impl->gl_normalmap_id );

		GL_AddShaderDirective( options, "HAS_NORMALMAP" );
		using_normalmap = true;
	}

	if (FBitSet(mat->flags, BRUSH_HAS_HEIGHTMAP) && mat->impl->reliefScale > 0.0f)
	{
		if ((mat->impl->gl_heightmap_id != tr.blackTexture) && (cv_parallax->value > 0.0f))
		{
			if (cv_parallax->value == 1.0f)
				GL_AddShaderDirective(options, "PARALLAX_SIMPLE");
			else if (cv_parallax->value >= 2.0f)
				GL_AddShaderDirective(options, "PARALLAX_OCCLUSION");
		}
	} 

	// and finally select the render-mode
	if( FBitSet( mat->flags, BRUSH_LIQUID ))
	{
		surf_transparent = true; // obviously, liquids always transparent
		GL_AddShaderDirective( options, "LIQUID_SURFACE" );
		if( tr.waterlevel >= 3 )
			GL_AddShaderDirective( options, "LIQUID_UNDERWATER" );

		// world watery with compiler feature
		//if (!FBitSet(s->flags, SURF_OF_SUBMODEL) && FBitSet(world->features, WORLD_WATERALPHA))
		//	shader_use_screencopy = true;

		// apply cubemap reflections for water
		if (cubemaps_available && !RP_CUBEPASS())
		{
			GL_AddShaderDirective(options, "REFLECTION_CUBEMAP");
			using_cubemaps = true;
		}
	}
	else if (cubemaps_available && (mat->impl->reflectScale > 0.0f) && !RP_CUBEPASS( ))
	{
		if( !FBitSet( mat->flags, BRUSH_REFLECT ))
		{
			GL_AddShaderDirective( options, "REFLECTION_CUBEMAP" );
			using_cubemaps = true;
		}
	}

	if (using_aberrations || using_refractions) {
		shader_use_screencopy = true;
	}

	if (FBitSet(mat->flags, BRUSH_TRANSPARENT) || e->curstate.rendermode == kRenderTransTexture) 
	{
		// don't need alpha blending when using screen copy
		if (e->curstate.rendermode == kRenderTransTexture && !shader_use_screencopy) {
			GL_AddShaderDirective(options, "ALPHA_BLENDING");
		}
		else if (e->curstate.rendermode == kRenderTransAlpha)
		{
			// case when using textures with prefix '{'
			if (GL_UsingAlphaToCoverage() && GL_Support(R_A2C_DITHER_CONTROL))
			{
				// enable macro only when we're disabled dithering for A2C
				GL_AddShaderDirective(options, "ALPHA_TO_COVERAGE");
			}
		}
		surf_transparent = true;
	}

	if (shader_use_screencopy && surf_transparent) {
		GL_AddShaderDirective(options, "USING_SCREENCOPY");
	}

	if (mirror) {
		GL_AddShaderDirective(options, "PLANAR_REFLECTION");
	}
	else if (surf_portal) {
		GL_AddShaderDirective(options, "PORTAL_SURFACE");
	}

	if (using_normalmap && surf_transparent)
	{
		if (using_refractions) {
			GL_AddShaderDirective(options, "APPLY_REFRACTION");
		}
		if (using_aberrations) {
			GL_AddShaderDirective(options, "APPLY_ABERRATION");
		}
	}

	if (tr.fogEnabled && !RP_CUBEPASS()) {
		GL_AddShaderDirective(options, "APPLY_FOG_EXP");
	}

	word shaderNum = GL_FindUberShader( glname, options );

	if( !shaderNum )
	{
		tr.fClearScreen = true; // to avoid ugly blur
		SetBits( s->flags, SURF_NODRAW );
		return 0; // something bad happens
	}

	if( shader_use_screencopy )
		GL_AddShaderFeature( shaderNum, SHADER_USE_SCREENCOPY );
	if( shader_additive )
		GL_AddShaderFeature( shaderNum, SHADER_ADDITIVE );

	if( using_cubemaps )
		GL_AddShaderFeature( shaderNum, SHADER_USE_CUBEMAPS );

	es->lastRenderMode = e->curstate.rendermode;
	ClearBits( s->flags, SURF_NODRAW );
	es->forwardScene[mirror].SetShader( shaderNum );
	
	return shaderNum;
}

/*
=================
Mod_ShaderLightForward

compute dynamic lighting
=================
*/
static word Mod_ShaderLightForward( CDynLight *dl, msurface_t *s )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	mfaceinfo_t *landscape = NULL;
	mextrasurf_t *es = s->info;
	int32_t lightShadowType = !FBitSet(dl->flags, DLF_NOSHADOWS) ? 0 : 1;

	switch( dl->type )
	{
		case LIGHT_SPOT:
			if (es->forwardLightSpot[lightShadowType].IsValid()) {
				return es->forwardLightSpot[lightShadowType].GetHandle(); // valid
			}
			break;
		case LIGHT_OMNI:
			if (es->forwardLightOmni[lightShadowType].IsValid()) {
				return es->forwardLightOmni[lightShadowType].GetHandle(); // valid
			}
			break;
		case LIGHT_DIRECTIONAL:
			if (es->forwardLightProj.IsValid()) {
				return es->forwardLightProj.GetHandle(); // valid
			}
			break;
	}

	Q_strncpy( glname, "forward/light_bmodel", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	switch( dl->type )
	{
	case LIGHT_SPOT:
		GL_AddShaderDirective( options, "LIGHT_SPOT" );
		break;
	case LIGHT_OMNI:
		GL_AddShaderDirective( options, "LIGHT_OMNI" );
		break;
	case LIGHT_DIRECTIONAL:
		GL_AddShaderDirective( options, "LIGHT_PROJ" );
		break;
	}

	// mirror is actual only if we has actual screen texture!
	bool mirror = Surf_CheckSubview( s->info );
	material_t *mat = R_TextureAnimation( s )->material;
	landscape = s->texinfo->faceinfo;

	if( CVAR_TO_BOOL( cv_brdf ))
		GL_AddShaderDirective( options, "APPLY_PBS" );

	if( CVAR_TO_BOOL( cv_bump ) || FBitSet( mat->flags, BRUSH_LIQUID ))
	{
		if( FBitSet( mat->flags, BRUSH_HAS_BUMP ) && !FBitSet( dl->flags, DLF_NOBUMP ))
		{
			// FIXME: all the waternormals should be encoded as first frame
			if( FBitSet( mat->flags, BRUSH_LIQUID ))
				GL_EncodeNormal( options, tr.waterTextures[0] );
			else GL_EncodeNormal( options, mat->impl->gl_normalmap_id );
			GL_AddShaderDirective( options, "HAS_NORMALMAP" );
			GL_AddShaderDirective( options, "COMPUTE_TBN" );
		}
	}

	if (CVAR_TO_BOOL(cv_specular) && FBitSet(mat->flags, BRUSH_HAS_SPECULAR))
	{
		GL_AddShaderDirective(options, "HAS_GLOSSMAP");
	}

	if (FBitSet(mat->flags, BRUSH_HAS_HEIGHTMAP) && mat->impl->reliefScale > 0.0f)
	{
		if ((mat->impl->gl_heightmap_id != tr.blackTexture) && (cv_parallax->value > 0.0f))
		{
			if (cv_parallax->value == 1.0f)
				GL_AddShaderDirective(options, "PARALLAX_SIMPLE");
			else if (cv_parallax->value >= 2.0f)
				GL_AddShaderDirective(options, "PARALLAX_OCCLUSION");
		}
	}

	if( FBitSet( mat->flags, BRUSH_MULTI_LAYERS ) && landscape && landscape->terrain )
	{
		GL_AddShaderDirective( options, va( "TERRAIN_NUM_LAYERS %i", landscape->terrain->numLayers ));
		GL_AddShaderDirective( options, "APPLY_TERRAIN" );

		if( landscape->terrain->indexmap.gl_diffuse_id.Initialized() && CVAR_TO_BOOL(r_detailtextures) && glConfig.max_varying_floats > 48)
			GL_AddShaderDirective( options, "HAS_DETAIL" );
	}
	else
	{
		if( FBitSet( mat->flags, BRUSH_HAS_DETAIL ) && CVAR_TO_BOOL( r_detailtextures ) && glConfig.max_varying_floats > 48 )
			GL_AddShaderDirective( options, "HAS_DETAIL" );
	}

	if( mirror && glConfig.max_varying_floats > 48 )
		GL_AddShaderDirective( options, "PLANAR_REFLECTION" );

	// debug visualization
	if( r_lightmap->value > 0.0f && r_lightmap->value <= 2.0f )
	{
		if( r_lightmap->value == 1.0f && worldmodel->lightdata )
			GL_AddShaderDirective( options, "LIGHTMAP_DEBUG" );
		else if( r_lightmap->value == 2.0f && FBitSet( world->features, WORLD_HAS_DELUXEMAP ))
			GL_AddShaderDirective( options, "LIGHTVEC_DEBUG" );
	}

	// and finally select the render-mode
	if( FBitSet( mat->flags, BRUSH_LIQUID ))
	{
		GL_AddShaderDirective( options, "LIQUID_SURFACE" );
		if( tr.waterlevel >= 3 )
			GL_AddShaderDirective( options, "LIQUID_UNDERWATER" );
	}

	if( CVAR_TO_BOOL( r_shadows ) && !FBitSet( dl->flags, DLF_NOSHADOWS ))
	{
		int shadow_smooth_type = static_cast<int>(r_shadows->value);
		// shadow cubemaps only support if GL_EXT_gpu_shader4 is support
		if( dl->type == LIGHT_DIRECTIONAL && CVAR_TO_BOOL( r_sunshadows ))
		{
			GL_AddShaderDirective( options, "APPLY_SHADOW" );
			if (shadow_smooth_type == 4)
				GL_AddShaderDirective(options, "SHADOW_VOGEL_DISK");
		}
		else
		{
			GL_AddShaderDirective( options, "APPLY_SHADOW" );
			if (shadow_smooth_type == 2)
				GL_AddShaderDirective(options, "SHADOW_PCF2X2");
			else if (shadow_smooth_type == 3)
				GL_AddShaderDirective(options, "SHADOW_PCF3X3");
			else if (shadow_smooth_type == 4)
				GL_AddShaderDirective(options, "SHADOW_VOGEL_DISK");
		}
	}

	word shaderNum = GL_FindUberShader( glname, options );

	if( !shaderNum )
	{
		if( dl->type == LIGHT_DIRECTIONAL )
			SetBits( s->flags, SURF_NOSUNLIGHT );
		else SetBits( s->flags, SURF_NODLIGHT );

		return 0; // something bad happens
	}

	// done
	switch( dl->type )
	{
	case LIGHT_SPOT:
		es->forwardLightSpot[lightShadowType].SetShader( shaderNum );
		ClearBits( s->flags, SURF_NODLIGHT );
		break;
	case LIGHT_OMNI:
		es->forwardLightOmni[lightShadowType].SetShader( shaderNum );
		ClearBits( s->flags, SURF_NODLIGHT );
		break;
	case LIGHT_DIRECTIONAL:
		es->forwardLightProj.SetShader( shaderNum );
		ClearBits( s->flags, SURF_NOSUNLIGHT );
		break;
	}

	return shaderNum;
}

/*
=================
Mod_ShaderSceneDepth

return bmodel depth-shader
=================
*/
static word Mod_ShaderSceneDepth( msurface_t *s )
{
	mextrasurf_t *es = s->info;

	if( es->forwardDepth.IsValid( ))
		return es->forwardDepth.GetHandle();

	word shaderNum = GL_FindUberShader( "forward/depth_bmodel" );
	es->forwardDepth.SetShader( shaderNum );

	return shaderNum;
}

/*
=================
Mod_PrecacheShaders

precache shaders to reduce freezes in-game
=================
*/
static void Mod_PrecacheShaders( void )
{
	msurface_t	*surf;
	int		i;

	// preload shaders for all the world faces (but ignore watery faces)
	for( i = 0; i < worldmodel->submodels[0].numfaces; i++ )
	{
		surf = &worldmodel->surfaces[world->sortedfaces[i]];

		if( !FBitSet( surf->flags, SURF_DRAWTURB|SURF_DRAWSKY ))
		{
			Mod_ShaderSceneForward( surf );

			// also precache a default light shaders
			Mod_ShaderLightForward( &tr.defaultlightSpot, surf );
			Mod_ShaderLightForward( &tr.defaultlightOmni, surf );
			Mod_ShaderLightForward( &tr.defaultlightProj, surf );
		}
	}

	tr.params_changed = true;
#if 0
	Msg( "sorted faces:\n" );
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		surf = &worldmodel->surfaces[world->sortedfaces[i]];
		mextrasurf_t *esrf = SURF_INFO( surf, worldmodel );
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
	ZoneScoped;

	int	i;

	if( !tr.params_changed ) return;

	// rebuild shaders
	for( i = 0; i < worldmodel->submodels[0].numfaces; i++ )
		Mod_ShaderSceneForward( &worldmodel->surfaces[i] );

	// resort faces
	qsort( world->sortedfaces, worldmodel->numsurfaces, sizeof( unsigned short ), (cmpfunc)Mod_SurfaceCompareInGame );
#if 0
	Msg( "resorted faces:\n" );
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[world->sortedfaces[i]];
		mextrasurf_t *esrf = SURF_INFO( surf, worldmodel );
		Msg( "face %i (submodel %s) (local %s), shader %i, lightmap %i, style[1] %i\n",
		i, FBitSet( surf->flags, SURF_OF_SUBMODEL ) ? "Yes" : "No", FBitSet( surf->flags, SURF_LOCAL_SPACE ) ? "Yes" : "No",
		esrf->shaderNum, esrf->lightmaptexturenum, surf->styles[1] );
	}
#endif
}

/*
=================
Mod_ComputeFaceTBN

compute smooth TBN with baked normals
=================
*/
static void Mod_ComputeFaceTBN( msurface_t *surf, mextrasurf_t *esrf )
{
	Vector	texdirections[2];
	Vector	directionnormals[2];
	Vector	faceNormal;
	Vector	vertNormal = g_vecZero;
	int	side;

	for( int i = 0; i < esrf->numverts; i++ )
	{
		bvert_t *v = &world->vertexes[esrf->firstvertex + i];
		int l = worldmodel->surfedges[surf->firstedge + i];
		int vert = worldmodel->edges[abs(l)].v[(l > 0) ? 0 : 1];

		if( world->surfnormals != NULL && world->normals != NULL )
		{
			l = world->surfnormals[surf->firstedge + i];
			if( l >= 0 || l < world->numnormals )
				vertNormal = Vector( world->normals[l].normal );
			else ALERT( at_error, "normal index %d out of range (max %d)\n", l, world->numnormals );
		}
		else if( world->normals != NULL )
			vertNormal = Vector( world->normals[vert].normal );

		// calc unsmoothed tangent space
		if( FBitSet( surf->flags, SURF_PLANEBACK ))
			faceNormal = -surf->plane->normal;
		else 
			faceNormal = surf->plane->normal;

		// fallback
		if (vertNormal.IsEqual(g_vecZero, 0.0001f))
			vertNormal = faceNormal;

		vertNormal = vertNormal.Normalize();
		for( side = 0; side < 2; side++ )
		{
			texdirections[side] = CrossProduct( faceNormal, surf->info->lmvecs[!side] ).Normalize();
			if( DotProduct( texdirections[side], surf->info->lmvecs[side] ) < 0.0f )
				texdirections[side] = -texdirections[side];
		}

		for( side = 0; side < 2; side++ )
		{
			float dot = DotProduct( texdirections[side], vertNormal );
			VectorMA( texdirections[side], -dot, vertNormal, directionnormals[side] );
			directionnormals[side] = directionnormals[side].Normalize();
		}

		v->tangent = directionnormals[0];
		v->binormal = -directionnormals[1]; // makes TBN right-side coordinate system for accordance with deluxmap
		v->normal = vertNormal;
	}
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

		// setup shared pointers
		for( int i = 0; i < land->terrain->numLayers; i++ )
			land->effects[i] = land->terrain->layermap.material[i]->effects;

		land->heightmap = land->terrain->indexmap.pixels;
		land->heightmap_width = land->terrain->indexmap.width;
		land->heightmap_height = land->terrain->indexmap.height;
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

	// setup layers here
	if( surf->texinfo && surf->texinfo->texture && surf->texinfo->texture->material )
	{
		material_t *mat = surf->texinfo->texture->material;

		ASSERT( mat != NULL );

		mat->impl->gl_diffuse_id = terra->layermap.gl_diffuse_id;
		mat->impl->gl_normalmap_id = terra->layermap.gl_normalmap_id;
		mat->impl->gl_specular_id = terra->layermap.gl_specular_id;
		mat->impl->gl_glowmap_id = tr.blackTexture;
		mat->flags |= BRUSH_MULTI_LAYERS;

		if( mat->impl->gl_normalmap_id.Initialized() && mat->impl->gl_normalmap_id != tr.normalmapTexture )
			mat->flags |= BRUSH_HAS_BUMP;

		if( mat->impl->gl_specular_id.Initialized() && mat->impl->gl_specular_id != tr.blackTexture )
			mat->flags |= BRUSH_HAS_SPECULAR;

		if( mat->impl->gl_glowmap_id.Initialized() && mat->impl->gl_glowmap_id != tr.blackTexture )
			mat->flags |= BRUSH_HAS_LUMA;

		if( mat->impl->gl_specular_id.GetFlags() & TF_HAS_ALPHA)
			mat->flags |= BRUSH_GLOSSPOWER;

		// refresh material constants
		matdesc_t *desc = CL_FindMaterial( terra->indexmap.diffuse );
		Mod_CopyMaterialDesc( mat, desc );
	}

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
Mod_FindStaticLights

find a mark lights that affected to this face
=================
*/
void Mod_FindStaticLights( byte *vislight, byte lights[MAXDYNLIGHTS], const Vector &origin )
{
	mworldlight_t	*wl = world->worldlights;
	int		indexes[32];
	int		i, count;

	memset( lights, 255, sizeof( byte ) * MAXDYNLIGHTS );
	count = 0;

	// failed to vislightdata...
	if( !vislight ) return;

	// mark all the lights that can lit this face
	for( i = 0; i < world->numworldlights; i++, wl++ )
	{
		if( wl->emittype == emit_ignored )
			continue;	// bad light?

		// this face is invisible for this light
		if( !CHECKVISBIT( vislight, i ))
			continue;

		if( count >= ARRAYSIZE( indexes ))
		{
			ALERT( at_aiconsole, "too many lights on a face\n" );
			break;
		}

		indexes[count++] = i;// member this light
	}

get_next_light:
	float maxPhotons = 0.0;
	int ignored = -1;
	int light = 255;

	for( i = 0; i < count; i++ )
	{
		if( indexes[i] == -1 )
			continue;

		wl = world->worldlights + indexes[i];
		Vector delta = (wl->origin - origin);
		float dist = Q_max( delta.Length(), 1.0 );
		delta = delta.Normalize();
		float ratio = 1.0 / (dist * dist);
		Vector add = wl->intensity * ratio;
		float photons = add.MaxCoord();

		// skylight has a maximum priority
		if( photons > maxPhotons )
		{
			maxPhotons = photons;
			light = indexes[i];
			ignored = i;
		}
	}

	if( ignored == -1 ) {
		return;
	}

	for( i = 0; i < MAXDYNLIGHTS && lights[i] != 255; i++ );
	if( i < MAXDYNLIGHTS ) {
		lights[i] = light;	// nearest light for surf
	}

	indexes[ignored] = -1;		// this light is handled

//	if( count > (int)cv_deferred_maxlights->value && i == (int)cv_deferred_maxlights->value )
//		Msg( "skipped light %i intensity %g, type %d\n", light, maxPhotons, world->worldlights[light].emittype );
	goto get_next_light;
}

static void CreateBufferBaseGL21( bvert_t *arrayxvert )
{
	std::vector<bvert_v0_gl21_t> arraybvert;
	arraybvert.resize(world->numvertexes);

	// convert to GLSL-compacted array
	for( int i = 0; i < world->numvertexes; i++ )
	{
		arraybvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraybvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraybvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraybvert[i].tangent[0] = arrayxvert[i].tangent[0];
		arraybvert[i].tangent[1] = arrayxvert[i].tangent[1];
		arraybvert[i].tangent[2] = arrayxvert[i].tangent[2];
		arraybvert[i].binormal[0] = arrayxvert[i].binormal[0];
		arraybvert[i].binormal[1] = arrayxvert[i].binormal[1];
		arraybvert[i].binormal[2] = arrayxvert[i].binormal[2];
		arraybvert[i].normal[0] = arrayxvert[i].normal[0];
		arraybvert[i].normal[1] = arrayxvert[i].normal[1];
		arraybvert[i].normal[2] = arrayxvert[i].normal[2];
		arraybvert[i].stcoord0[0] = arrayxvert[i].stcoord0[0];
		arraybvert[i].stcoord0[1] = arrayxvert[i].stcoord0[1];
		arraybvert[i].stcoord0[2] = arrayxvert[i].stcoord0[2];
		arraybvert[i].stcoord0[3] = arrayxvert[i].stcoord0[3];
		arraybvert[i].lmcoord0[0] = arrayxvert[i].lmcoord0[0];
		arraybvert[i].lmcoord0[1] = arrayxvert[i].lmcoord0[1];
		arraybvert[i].lmcoord0[2] = arrayxvert[i].lmcoord0[2];
		arraybvert[i].lmcoord0[3] = arrayxvert[i].lmcoord0[3];
		arraybvert[i].lmcoord1[0] = arrayxvert[i].lmcoord1[0];
		arraybvert[i].lmcoord1[1] = arrayxvert[i].lmcoord1[1];
		arraybvert[i].lmcoord1[2] = arrayxvert[i].lmcoord1[2];
		arraybvert[i].lmcoord1[3] = arrayxvert[i].lmcoord1[3];
		memcpy( arraybvert[i].styles, arrayxvert[i].styles, MAXLIGHTMAPS );
	}

	world->cacheSize = world->numvertexes * sizeof( bvert_v0_gl21_t );

	// create world vertex buffer
	pglGenBuffersARB( 1, &world->vertex_buffer_object );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, world->vertex_buffer_object );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, world->cacheSize, &arraybvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

static void BindBufferBaseGL21( void )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, world->vertex_buffer_object );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, 0, sizeof( bvert_v0_gl21_t ), (void *)offsetof( bvert_v0_gl21_t, vertex ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_FLOAT, 0, sizeof( bvert_v0_gl21_t ), (void *)offsetof( bvert_v0_gl21_t, tangent ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );

	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_FLOAT, 0, sizeof( bvert_v0_gl21_t ), (void *)offsetof( bvert_v0_gl21_t, binormal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, 0, sizeof( bvert_v0_gl21_t ), (void *)offsetof( bvert_v0_gl21_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, 0, sizeof( bvert_v0_gl21_t ), (void *)offsetof( bvert_v0_gl21_t, stcoord0 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, 0, sizeof( bvert_v0_gl21_t ), (void *)offsetof( bvert_v0_gl21_t, lmcoord0 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, 0, sizeof( bvert_v0_gl21_t ), (void *)offsetof( bvert_v0_gl21_t, lmcoord1 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, 0, sizeof( bvert_v0_gl21_t ), (void *)offsetof( bvert_v0_gl21_t, styles ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
}

static void CreateBufferBaseGL30( bvert_t *arrayxvert )
{
	std::vector<bvert_v0_gl30_t> arraybvert;
	arraybvert.resize(world->numvertexes);

	// convert to GLSL-compacted array
	for( int i = 0; i < world->numvertexes; i++ )
	{
		arraybvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraybvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraybvert[i].vertex[2] = arrayxvert[i].vertex[2];
		CompressNormalizedVector( arraybvert[i].normal, arrayxvert[i].normal );
		CompressNormalizedVector( arraybvert[i].tangent, arrayxvert[i].tangent );
		CompressNormalizedVector( arraybvert[i].binormal, arrayxvert[i].binormal );
		arraybvert[i].stcoord0[0] = arrayxvert[i].stcoord0[0];
		arraybvert[i].stcoord0[1] = arrayxvert[i].stcoord0[1];
		arraybvert[i].stcoord0[2] = arrayxvert[i].stcoord0[2];
		arraybvert[i].stcoord0[3] = arrayxvert[i].stcoord0[3];
		arraybvert[i].lmcoord0[0] = arrayxvert[i].lmcoord0[0];
		arraybvert[i].lmcoord0[1] = arrayxvert[i].lmcoord0[1];
		arraybvert[i].lmcoord0[2] = arrayxvert[i].lmcoord0[2];
		arraybvert[i].lmcoord0[3] = arrayxvert[i].lmcoord0[3];
		arraybvert[i].lmcoord1[0] = arrayxvert[i].lmcoord1[0];
		arraybvert[i].lmcoord1[1] = arrayxvert[i].lmcoord1[1];
		arraybvert[i].lmcoord1[2] = arrayxvert[i].lmcoord1[2];
		arraybvert[i].lmcoord1[3] = arrayxvert[i].lmcoord1[3];
		memcpy( arraybvert[i].styles, arrayxvert[i].styles, MAXLIGHTMAPS );
		memcpy( arraybvert[i].lights0, arrayxvert[i].lights0, MAXLIGHTMAPS );
		memcpy( arraybvert[i].lights1, arrayxvert[i].lights1, MAXLIGHTMAPS );
	}

	world->cacheSize = world->numvertexes * sizeof( bvert_v0_gl30_t );

	// create world vertex buffer
	pglGenBuffersARB( 1, &world->vertex_buffer_object );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, world->vertex_buffer_object );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, world->cacheSize, &arraybvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

static void BindBufferBaseGL30( void )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, world->vertex_buffer_object );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, vertex ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_BYTE, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, tangent ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );

	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_BYTE, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, binormal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, stcoord0 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, lmcoord0 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, lmcoord1 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, styles ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_NUMS0, 4, GL_UNSIGNED_BYTE, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, lights0 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_NUMS0 );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_NUMS1, 4, GL_UNSIGNED_BYTE, 0, sizeof( bvert_v0_gl30_t ), (void *)offsetof( bvert_v0_gl30_t, lights1 ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_NUMS1 );
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
	byte *vislight = NULL;
	mvertex_t *dv;
	bvert_t *currVertex;
	int currVertexIndex = 0;
	world->numvertexes = 0;

	// compute totalvertex count for VBO but ignore sky polys
	for (int i = 0; i < worldmodel->numsurfaces; i++, surf++)
	{
		if( FBitSet( surf->flags, SURF_DRAWSKY ))
			continue;
		world->numvertexes += surf->numedges;
	}

	// temporary array will be removed at end of this function
	// g-cont. i'm leave local copy of vertexes for some debug purpoces
	world->vertexes = (bvert_t *)Mem_Alloc( sizeof( bvert_t ) * world->numvertexes );
	surf = worldmodel->surfaces;

	// create VBO-optimized vertex array (single for world and all brush-models)
	for (int i = 0; i < worldmodel->numsurfaces; i++, surf++ )
	{
		Vector t, b, n;

		if( FBitSet( surf->flags, SURF_DRAWSKY ))
			continue;	// ignore sky polys it was never be drawed

		currVertex = &world->vertexes[currVertexIndex];

		// request vislightdata for this surface
		if (world->vislightdata) 
			vislight = world->vislightdata + i * ((world->numworldlights + 7) / 8);
		Mod_FindStaticLights( vislight, surf->info->lights, surf->info->origin );

		// NOTE: all polygons stored as source (no tesselation anyway)
		for (int j = 0; j < surf->numedges; j++, currVertex++)
		{
			int l = worldmodel->surfedges[surf->firstedge + j];
			int vert = worldmodel->edges[abs(l)].v[(l > 0) ? 0 : 1];
			memcpy( currVertex->styles, surf->styles, sizeof( surf->styles ));
			memcpy( currVertex->lights0, surf->info->lights, sizeof( surf->info->lights ));
			dv = &worldmodel->vertexes[vert];
			currVertex->vertex = dv->position;

			R_TextureCoords( surf, currVertex->vertex, currVertex->stcoord0 );
			R_LightmapCoords( surf, currVertex->vertex, currVertex->lmcoord0, 0 );	// styles 0-1
			R_LightmapCoords( surf, currVertex->vertex, currVertex->lmcoord1, 2 );	// styles 2-3
		}

		// NOTE: now firstvertex are handled in world->vertexes[] array, not in world->tbn_vectors[] !!!
		surf->info->firstvertex = currVertexIndex;
		surf->info->numverts = surf->numedges;
		currVertexIndex += surf->numedges;

		Mod_ComputeFaceTBN( surf, surf->info );
	}

	// compute water global coords
	for (int i = 1; i < worldmodel->numsubmodels; i++ )
	{
		Vector absmin, absmax;

		ClearBounds( absmin, absmax );

		// first iteration - compute water bbox
		for (int j = 0; j < worldmodel->submodels[i].numfaces; j++ )
		{
			surf = &worldmodel->surfaces[worldmodel->submodels[i].firstface + j];
			if( !FBitSet( surf->flags, SURF_DRAWTURB ))
				continue;

			AddPointToBounds( surf->info->mins, absmin, absmax );
			AddPointToBounds( surf->info->maxs, absmin, absmax );
		}

		if( BoundsIsCleared( absmin, absmax ))
			continue;	// not a water

		float scale = sqrt( (absmax - absmin).Average()) * 0.3f;	// FIXME: tune this constant?

		// second iteration - mapping global coords
		for (int j = 0; j < worldmodel->submodels[i].numfaces; j++)
		{
			surf = &worldmodel->surfaces[worldmodel->submodels[i].firstface + j];
			if( !FBitSet( surf->flags, SURF_DRAWTURB ))
				continue;

			for( int k = 0; k < surf->info->numverts; k++ )
			{
				currVertex = &world->vertexes[surf->info->firstvertex + k];
				R_GlobalCoords( surf, currVertex->vertex, absmin, absmax, scale, currVertex->stcoord0 );
			}
		}
	}

	// time to prepare landscapes
	for (int i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		Mod_ProcessLandscapes( surf, surf->info );
	}

	for (int i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		Mod_MappingLandscapes( surf, surf->info );
	}

	// now normals are merged into single array world->vertexes[]
	if( world->normals != NULL )
		Mem_Free( world->normals );
	world->normals = NULL;

	GL_CheckVertexArrayBinding();

	// create world vertex buffer
	if( glConfig.version < ACTUAL_GL_VERSION )
		CreateBufferBaseGL21( world->vertexes );
	else CreateBufferBaseGL30( world->vertexes );

	// create vertex array object
	pglGenVertexArrays( 1, &world->vertex_array_object );
	pglBindVertexArray( world->vertex_array_object );

	if( glConfig.version < ACTUAL_GL_VERSION )
		BindBufferBaseGL21();
	else BindBufferBaseGL30();

	pglBindVertexArray( GL_FALSE );

	// update stats
	tr.total_vbo_memory += world->cacheSize;
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

	tr.total_vbo_memory -= world->cacheSize;
	world->vertex_array_object = world->vertex_buffer_object = 0;
	world->cacheSize = 0;
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
	for (int i = 1; i < RENDER_GET_PARM(PARM_MAX_ENTITIES, 0); i++)
	{
		cl_entity_t *e = GET_ENTITY( i );
		if (!e) {
			break;
		}
		e->modelhandle = INVALID_HANDLE;
		e->hCachedMatrix = WORLD_MATRIX;
	}

	GET_VIEWMODEL()->modelhandle = INVALID_HANDLE;
	memset( tr.draw_entities, 0, sizeof( tr.draw_entities ));
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

	// may caused by Host_Error, so clear gl state
	if( g_fRenderInitialized )
		GL_SetDefaultState();
}

/*
=================
Mod_LoadWorld

=================
*/
static void Mod_LoadWorld( model_t *mod, const byte *buf )
{
	dheader_t	*header;
	dextrahdr_t *extrahdr;
	int i;

	header = (dheader_t *)buf;
	extrahdr = (dextrahdr_t *)((byte *)buf + sizeof( dheader_t ));

	if( RENDER_GET_PARM( PARM_FEATURES, 0 ) & ENGINE_LARGE_LIGHTMAPS )
		glConfig.block_size = GL_BLOCK_SIZE_MAX;
	else glConfig.block_size = GL_BLOCK_SIZE_DEFAULT;

	if( RENDER_GET_PARM( PARM_MAP_HAS_DELUXE, 0 ))
		SetBits( world->features, WORLD_HAS_DELUXEMAP );

	if( RENDER_GET_PARM( PARM_WATER_ALPHA, 0 ))
		SetBits( world->features, WORLD_WATERALPHA );

	COM_FileBase( worldmodel->name, world->name );
	ALERT( at_aiconsole, "Mod_LoadWorld: %s\n", world->name );

	R_CheckChanges(); // catch all the cvar changes
	tr.glsl_valid_sequence = 1;
	tr.params_changed = false;

	// prepare visibility and setup leaf extradata
	Mod_SetupLeafExtradata( &header->lumps[LUMP_LEAFS], &header->lumps[LUMP_VISIBILITY], buf );

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

	if( extrahdr->id == IDEXTRAHEADER )
	{
		if( extrahdr->version == EXTRA_VERSION )
		{
			// new Xash3D extended format
			if( GL_Support( R_TEXTURECUBEMAP_EXT )) // loading cubemaps only when it's support
				Mod_LoadCubemaps( buf, &extrahdr->lumps[LUMP_CUBEMAPS] );
			Mod_LoadVertNormals( buf, &extrahdr->lumps[LUMP_VERTNORMALS] );
			Mod_LoadWorldLights( buf, &extrahdr->lumps[LUMP_WORLDLIGHTS] );
			Mod_LoadLeafAmbientLighting( buf, &extrahdr->lumps[LUMP_LEAF_LIGHTING] );
			Mod_LoadVertexLighting( buf, &extrahdr->lumps[LUMP_VERTEX_LIGHT] );
			Mod_LoadSurfaceLighting( buf, &extrahdr->lumps[LUMP_SURFACE_LIGHT] );
			Mod_LoadVisLightData( buf, &extrahdr->lumps[LUMP_VISLIGHTDATA] );
		}
		else if( extrahdr->version == EXTRA_VERSION_OLD )
		{
			// P2: Savior regular format
			if( GL_Support( R_TEXTURECUBEMAP_EXT )) // loading cubemaps only when it's support
				Mod_LoadCubemaps( buf, &extrahdr->lumps[LUMP_CUBEMAPS] );
			Mod_LoadWorldLights( buf, &extrahdr->lumps[LUMP_WORLDLIGHTS] );
		}

		Mod_SetupLeafLights();
	}

	// mark surfaces for world features
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		texture_t *tex = surf->texinfo->texture;

		if( FBitSet( surf->flags, SURF_DRAWSKY ))
			SetBits( world->features, WORLD_HAS_SKYBOX );

		if( !Q_strncmp(tex->name, "movie", 5 ))
		{
			SetBits( world->features, WORLD_HAS_MOVIES );
			SetBits( surf->flags, SURF_MOVIE );
		}

		if( !Q_strncmp(tex->name, "mirror", 6)
			|| !Q_strncmp(tex->name, "reflect", 7)
			|| !Q_strncmp(tex->name, "reflect1", 8)
			|| !Q_strncmp(tex->name, "!reflect", 8))
		{
			SetBits(world->features, WORLD_HAS_MIRRORS);
			SetBits(surf->flags, SURF_REFLECT);
		}

		if (!Q_strncmp(tex->name, "portal", 6))
		{
			SetBits(world->features, WORLD_HAS_PORTALS);
			SetBits(surf->flags, SURF_PORTAL);
		}

		if (!Q_strncmp(tex->name, "monitor", 7))
		{
			SetBits(world->features, WORLD_HAS_SCREENS);
			SetBits(surf->flags, SURF_SCREEN);
		}

		if( FBitSet( surf->flags, SURF_REFLECT ))
			GL_AllocOcclusionQuery( surf );
	}

	world->deluxedata = (color24 *)RENDER_GET_PARM( PARM_DELUXEDATA, 0 );
	world->shadowdata = (byte *)RENDER_GET_PARM( PARM_SHADOWDATA, 0 );

	Mod_FinalizeWorld();
	Mod_CreateBufferObject();

	// precache and upload cinematics
	R_InitCinematics();

	// helper to precache shaders
	R_InitDefaultLights();

	// time to place grass
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		// place to initialize our grass
		R_GrassInitForSurface( &worldmodel->surfaces[i] );
	}

	// precache world shaders
	Mod_PrecacheShaders();
}

static void Mod_FreeWorld( model_t *mod )
{
	Mod_FreeCubemaps();

	// destroy VBO & VAO
	Mod_DeleteBufferObject();

	if( world->leafs )
		Mem_Free( world->leafs );
	world->leafs = NULL;

	if( world->vertexes )
		Mem_Free( world->vertexes );
	world->vertexes = NULL;

	if( world->vertex_lighting )
		Mem_Free( world->vertex_lighting );
	world->vertex_lighting = NULL;

	if( world->surface_lighting )
		Mem_Free( world->surface_lighting );
	world->surface_lighting = NULL;

	// free old cinematics
	R_FreeCinematics();

	// free landscapes
	R_FreeLandscapes();

	if( world->materials )
	{
		for( int i = 0; i < worldmodel->numtextures; i++ )
		{
			texture_t *tx = worldmodel->textures[i];
			material_t *mat = &world->materials[i];
			TextureHandle texHandle = tx;

			if( !mat->pSource ) continue;	// not initialized?

			if( !FBitSet( mat->flags, BRUSH_MULTI_LAYERS ))
			{
				if( texHandle != mat->impl->gl_diffuse_id )
					FREE_TEXTURE( mat->impl->gl_diffuse_id );

				if( mat->impl->gl_normalmap_id != tr.normalmapTexture )
					FREE_TEXTURE( mat->impl->gl_normalmap_id );

				if( mat->impl->gl_specular_id != tr.blackTexture )
					FREE_TEXTURE( mat->impl->gl_specular_id );
            }

			if( mat->impl->gl_glowmap_id != tr.blackTexture )
				FREE_TEXTURE( mat->impl->gl_glowmap_id );
		}

		Mem_Free( world->materials );
		world->materials = NULL;
	}

	// delete queries
	for( int i = 0; i < worldmodel->numsurfaces; i++ )
		GL_DeleteOcclusionQuery( &worldmodel->surfaces[i] );

	if( FBitSet( world->features, WORLD_HAS_GRASS ))
	{
		// throw grass vbo's
		for( int i = 0; i < worldmodel->numsurfaces; i++ )
			R_RemoveGrassForSurface( worldmodel->surfaces[i].info );
	}

	memset( world, 0, sizeof( gl_world_t ));
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
	worldmodel = mod;

	if( create )
	{
		double start = Sys_DoubleTime();
		Mod_LoadWorld( mod, buffer );
		double end = Sys_DoubleTime();
		r_buildstats.create_buffer_object += (end - start);
		r_buildstats.total_buildtime += (end - start);
	}
	else Mod_FreeWorld( mod );
}

/*
=============================================================================

  WORLD RENDERING

=============================================================================
*/
static unsigned int tempElems[MAX_MAP_ELEMS];
static unsigned int numTempElems;

/*
=================
R_MarkSubmodelVisibleFaces

do all the visibility checks, update surface
params, and mark them as visible
=================
*/
void R_MarkSubmodelVisibleFaces( void )
{
	cl_entity_t	*e = RI->currententity;
	model_t		*model = RI->currentmodel;
	Vector		absmin, absmax;
	CFrustum		*frustum;
	mplane_t		plane;
	msurface_t	*surf;
	mextrasurf_t	*esrf;
	gl_state_t	*glm;
	int		i;

	// grab the transformed vieworg
	glm = GL_GetCache( e->hCachedMatrix );

	if( e->angles != g_vecZero )
	{
		TransformAABB( glm->transform, model->mins, model->maxs, absmin, absmax );
	}
	else
	{
		absmin = e->origin + model->mins;
		absmax = e->origin + model->maxs;
	}

	if( !Mod_CheckBoxVisible( absmin, absmax ))
	{
		r_stats.c_culled_entities++;
		return;
	}

	// frustum checking
	if( R_CullBrushModel( e ))
	{
		r_stats.c_culled_entities++;
		return;
	}

	// mark all the visible faces (will added later)
	for( i = 0; model != NULL && i < model->nummodelsurfaces; i++ )
	{
		surf = model->surfaces + model->firstmodelsurface + i;
		esrf = surf->info;

		esrf->parent = RI->currententity; // setup dynamic upcast
		if( FBitSet( surf->flags, SURF_DRAWTURB ))
		{
			if( FBitSet( surf->flags, SURF_PLANEBACK ))
				SetPlane( &plane, -surf->plane->normal, -surf->plane->dist );
			else SetPlane( &plane, surf->plane->normal, surf->plane->dist );
			glm->transform.TransformPositivePlane( plane, plane );

			if( surf->plane->type != PLANE_Z && !FBitSet( e->curstate.effects, EF_WATERSIDES ))
				continue;
			if( absmin[2] + 1.0 >= plane.dist )
				continue;
		}

		// non-moved bmodels can be culled by frustum
		if( R_StaticEntity( e ))
			frustum = &RI->view.frustum;
		else frustum = NULL;

		int cull_type = R_CullSurface( surf, tr.modelorg, frustum );

		if( cull_type >= CULL_FRUSTUM )
			continue;

		if( cull_type == CULL_BACKSIDE )
		{
			if( !FBitSet( surf->flags, SURF_DRAWTURB ))
				continue;
		}

		SETVISBIT( RI->view.visfaces, model->firstmodelsurface + i );

		// surface has passed all visibility checks
		// and can be update some data (lightmaps, mirror matrix, etc)
		if( RP_NORMALPASS( )) R_UpdateSurfaceParams( surf );
	}

	if( e->origin != e->baseline.vuser4 )
	{
		R_FindWorldLights( e->origin, e->model->mins, e->model->maxs, e->lights, true );
#if 0
		Msg( "%s gather lights: %d %d %d %d %d %d %d %d\n", e->model->name,
		e->lights[0], e->lights[1], e->lights[2], e->lights[3], e->lights[4], e->lights[4], e->lights[6], e->lights[7] );
#endif
		e->baseline.vuser4 = e->origin;
	}
}

/*
=================
R_UpdateSubmodelParams

run cinematic, evaluate conveyor etc
=================
*/
void R_UpdateSubmodelParams( void )
{
	cl_entity_t	*e = RI->currententity;
	model_t		*model = RI->currentmodel;
	msurface_t	*surf;
	mextrasurf_t	*esrf;

	// mark all the visible faces (will added later)
	for( int i = 0; model != NULL && i < model->nummodelsurfaces; i++ )
	{
		surf = model->surfaces + model->firstmodelsurface + i;
		esrf = surf->info;

		esrf->parent = RI->currententity; // setup dynamic upcast

		// and can be update some data (lightmaps, mirror matrix, etc)
		R_UpdateSurfaceParams( surf );
	}
}

_forceinline void R_DrawSurface( mextrasurf_t *es )
{
	// accumulate the indices
	for( int j = 0; j < es->numverts - 2; j++ )
	{
		ASSERT( numTempElems < ( MAX_MAP_ELEMS - 3 ));

		tempElems[numTempElems++] = es->firstvertex;
		tempElems[numTempElems++] = es->firstvertex + j + 1;
		tempElems[numTempElems++] = es->firstvertex + j + 2;
	}
}

void R_MarkVisibleLights( byte lights[MAXDYNLIGHTS] )
{
	// mark lights that visible for this frame
	for( int i = 0; i < MAXDYNLIGHTS && lights[i] != 255; i++ )
		SETVISBIT( RI->view.vislight, lights[i] );
}

/*
=================
R_AddSurfaceToDrawList

add specified face into sorted drawlist
=================
*/
bool R_AddSurfaceToDrawList( msurface_t *surf, drawlist_t drawlist_type )
{
	cl_entity_t	*e = RI->currententity;
	mextrasurf_t	*es = surf->info;
	word		hProgram = 0;
	CSolidEntry	entry_s;
	CTransEntry	entry_t;

	switch( drawlist_type )
	{
	case DRAWLIST_SOLID:
		if( FBitSet( surf->flags, SURF_NODRAW ))
			return false;

		hProgram = Mod_ShaderSceneForward( surf );
		R_MarkVisibleLights( es->lights );

		entry_s.SetRenderSurface( surf, hProgram );
		RI->frame.solid_faces.AddToTail( entry_s );
		break;
	case DRAWLIST_TRANS:
	{
		if (FBitSet(surf->flags, SURF_NODRAW))
			return false;

		Vector mins, maxs;
		hProgram = Mod_ShaderSceneForward(surf);
		entry_t.SetRenderSurface(surf, hProgram);

		gl_state_t *glm = GL_GetCache(e->hCachedMatrix);
		TransformAABB(glm->transform, es->mins, es->maxs, mins, maxs);
		ExpandBounds(mins, maxs, 2.0f); // create sentinel border for refractions
		if (ScreenCopyRequired(&glsl_programs[hProgram])) {
			entry_t.ComputeScissor(mins, maxs);
		}
		else {
			entry_t.ComputeViewDistance(mins, maxs);
		}
		RI->frame.trans_list.AddToTail(entry_t);
		break;
	}
	case DRAWLIST_SHADOW:
		if( FBitSet( surf->flags, SURF_NODRAW ))
			return false;
		hProgram = Mod_ShaderSceneDepth( surf );
		entry_s.SetRenderSurface( surf, hProgram );
		RI->frame.solid_faces.AddToTail( entry_s );
		break;
	case DRAWLIST_LIGHT:
		if( RI->currentlight->type == LIGHT_DIRECTIONAL )
		{
			if( FBitSet( surf->flags, SURF_NOSUNLIGHT ))
				return false;
		}
		else
		{
			if( FBitSet( surf->flags, SURF_NODLIGHT ))
				return false;
		}
		hProgram = Mod_ShaderLightForward( RI->currentlight, surf );
		entry_s.SetRenderSurface( surf, hProgram );
		RI->frame.light_faces.AddToTail( entry_s );
		break;
	case DRAWLIST_SUBVIEW:
		if ( RI->frame.num_subview_faces >= MAX_SUBVIEW_FACES )
			return false;

		// check for restrictions
		if (FBitSet(surf->flags, SURF_REFLECT) && !CVAR_TO_BOOL(r_allow_mirrors))
			return false;

		if (FBitSet(surf->flags, SURF_REFLECT_PUDDLE) && !CVAR_TO_BOOL(cv_realtime_puddles))
			return false;

		if (FBitSet(surf->flags, SURF_PORTAL) && !CVAR_TO_BOOL(r_allow_portals))
			return false;

		if (FBitSet(surf->flags, SURF_SCREEN) && !CVAR_TO_BOOL(r_allow_screens))
			return false;

		RI->frame.subview_faces[RI->frame.num_subview_faces] = surf;
		RI->frame.num_subview_faces++;
		GL_SurfaceOccluded( surf );// fetch queries result
		break;
	default:
		HOST_ERROR( "R_AddSurfaceToDrawList: unknown drawlist( %i )\n", drawlist_type );
		break;
	}

	return true;
}

/*
================
R_SetSurfaceUniforms

================
*/
void R_SetSurfaceUniforms( word hProgram, msurface_t *surface, bool force )
{
	ZoneScoped;

	mextrasurf_t *es = surface->info;
	Vector4D lightstyles, lightdir;
	cl_entity_t *e = es->parent;
	msurface_t *s = surface;
	float r, g, b, a, *v;
	int map;

	// begin draw the sorted list
	if( force || ( RI->currentshader != &glsl_programs[hProgram] ))
	{
		// force to bind new shader
		GL_BindShader( &glsl_programs[hProgram] );
	}

	material_t *mat = R_TextureAnimation( s )->material;
	glsl_program_t *shader = RI->currentshader;
	CDynLight *pl = RI->currentlight; // may be NULL
	gl_state_t *glm = GL_GetCache( e->hCachedMatrix );
	mtexinfo_t *tx = s->texinfo;
	mfaceinfo_t *land = tx->faceinfo;
	GLfloat viewMatrix[16];

	if( !FBitSet( RI->params, RP_SHADOWVIEW ))
	{
		GL_DepthRange( gldepthmin, gldepthmax );
		GL_ClipPlane( true );

		if( tr.waterlevel >= 3 && RP_NORMALPASS() && FBitSet( s->flags, SURF_DRAWTURB ))
			GL_Cull( GL_BACK );
		else GL_Cull( GL_FRONT );
	}

	if( FBitSet( mat->flags, BRUSH_TRANSPARENT|BRUSH_HAS_ALPHA ))
		GL_AlphaTest( GL_TRUE );
	else 
		GL_AlphaTest( GL_FALSE );

	tr.modelorg = glm->GetModelOrigin();

	// setup specified uniforms (and texture bindings)
	for( int i = 0; i < shader->numUniforms; i++ )
	{
		uniform_t *u = &shader->uniforms[i];

		switch( u->type )
		{
		case UT_COLORMAP:
			if( Surf_CheckSubview( es ))
				u->SetValue( Surf_GetSubview( es )->texturenum.ToInt() );
			else if( FBitSet( s->flags, SURF_MOVIE ) && RI->currententity->curstate.body )
				u->SetValue( tr.cinTextures[es->cintexturenum-1].ToInt() );
			else u->SetValue( mat->impl->gl_diffuse_id.ToInt() );
			break;
		case UT_NORMALMAP:
			if( FBitSet( mat->flags, BRUSH_LIQUID ) && tr.waterTextures[0].Initialized() )
				u->SetValue( tr.waterTextures[(int)( tr.time * WATER_ANIMTIME ) % WATER_TEXTURES].ToInt() );
			else u->SetValue( mat->impl->gl_normalmap_id.ToInt() );
			break;
		case UT_GLOSSMAP:
			u->SetValue( mat->impl->gl_specular_id.ToInt() );
			break;
		case UT_DETAILMAP:
			if( land && land->terrain && land->terrain->indexmap.gl_diffuse_id.Initialized() )
				u->SetValue( land->terrain->indexmap.gl_diffuse_id.ToInt() );
			else u->SetValue( mat->impl->gl_detailmap_id.ToInt() );
			break;
		case UT_PROJECTMAP:
			if( pl && pl->type == LIGHT_SPOT )
				u->SetValue( pl->spotlightTexture.ToInt() );
			else u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_SHADOWMAP:
		case UT_SHADOWMAP0:
			if( pl ) u->SetValue( pl->shadowTexture[0].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP1:
			if( pl ) u->SetValue( pl->shadowTexture[1].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP2:
			if( pl ) u->SetValue( pl->shadowTexture[2].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP3:
			if( pl ) u->SetValue( pl->shadowTexture[3].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_LIGHTMAP:
			if( R_FullBright( )) u->SetValue( tr.grayTexture.ToInt() );
			else u->SetValue( tr.lightmaps[es->lightmaptexturenum].lightmap.ToInt() );
			break;
		case UT_DELUXEMAP:
			if( R_FullBright( )) u->SetValue( tr.deluxemapTexture.ToInt() );
			else u->SetValue( tr.lightmaps[es->lightmaptexturenum].deluxmap.ToInt() );
			break;
		case UT_DECALMAP:
			// unacceptable for brushmodels
			u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_SCREENMAP:
			u->SetValue( tr.screen_color.ToInt() );
			break;
		case UT_DEPTHMAP:
			u->SetValue( tr.screen_depth.ToInt() );
			break;
		case UT_ENVMAP0:
		case UT_ENVMAP:
			if (!RP_CUBEPASS() && es->cubemap[0] != NULL) {
				u->SetValue(es->cubemap[0]->texture.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.texture.ToInt());
			}
			break;
		case UT_ENVMAP1:
			if (!RP_CUBEPASS() && es->cubemap[1] != NULL) {
				u->SetValue(es->cubemap[1]->texture.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.texture.ToInt());
			}
			break;
		case UT_SPECULARMAPIBL0:
			if (!RP_CUBEPASS() && es->cubemap[0] != NULL) {
				u->SetValue(es->cubemap[0]->textureSpecularIBL.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.textureSpecularIBL.ToInt());
			}
			break;
		case UT_SPECULARMAPIBL1:
			if (!RP_CUBEPASS() && es->cubemap[1] != NULL) {
				u->SetValue(es->cubemap[1]->textureSpecularIBL.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.textureSpecularIBL.ToInt());
			}
			break;
		case UT_BRDFAPPROXMAP:
			u->SetValue(tr.brdfApproxTexture.ToInt());
			break;
		case UT_GLOWMAP:
			u->SetValue( mat->impl->gl_glowmap_id.ToInt() );
			break;
		case UT_LAYERMAP:
			if( FBitSet( mat->flags, BRUSH_MULTI_LAYERS ) && land && land->terrain )
				u->SetValue( land->terrain->indexmap.gl_heightmap_id.ToInt() );
			else u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_HEIGHTMAP:
			u->SetValue( mat->impl->gl_heightmap_id.ToInt() );
			break;
		case UT_RELIEFPARAMS: 
		{
			float width = mat->impl->gl_heightmap_id.GetWidth();
			float height = mat->impl->gl_heightmap_id.GetHeight();
			u->SetValue(width, height, mat->impl->reliefScale, cv_shadow_offset->value);
			break;
		}
		case UT_FITNORMALMAP:
			u->SetValue( tr.normalsFitting.ToInt() );
			break;
		case UT_MODELMATRIX:
			u->SetValue( &glm->modelMatrix[0] );
			break;
		case UT_REFLECTMATRIX:
			if( Surf_CheckSubview( es ))
				Surf_GetSubview( es )->matrix.CopyToArray( viewMatrix );
			else memcpy( viewMatrix, glState.identityMatrix, sizeof( float ) * 16 );
			u->SetValue( &viewMatrix[0] );
			break;
		case UT_SCREENSIZEINV:
			u->SetValue( 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			break;
		case UT_ZFAR:
			u->SetValue( -tr.farclip * 1.74f );
			break;
		case UT_LIGHTSTYLES:
			for( map = 0; map < MAXLIGHTMAPS; map++ )
			{
				if( s->styles[map] != 255 )
					lightstyles[map] = tr.lightstyle[s->styles[map]];
				else lightstyles[map] = 0.0f;
			}
			u->SetValue( lightstyles.x, lightstyles.y, lightstyles.z, lightstyles.w );
			break;
		case UT_LIGHTSTYLEVALUES:
			u->SetValue( &tr.lightstyle[0], MAX_LIGHTSTYLES );
			break;
		case UT_REALTIME:
			u->SetValue( (float)tr.time );
			break;
		case UT_DETAILSCALE:
			u->SetValue( mat->impl->detailScale[0], mat->impl->detailScale[1] );
			break;
		case UT_FOGPARAMS:
			u->SetValue( tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			break;
		case UT_SHADOWPARMS:
			if( pl != NULL )
			{
				float shadowWidth = 1.0f / (float)pl->shadowTexture[0].GetWidth();
				float shadowHeight = 1.0f / (float)pl->shadowTexture[0].GetHeight();
				// depth scale and bias and shadowmap resolution
				u->SetValue( shadowWidth, shadowHeight, -pl->projectionMatrix[2][2], pl->projectionMatrix[3][2] );
			}
			else u->SetValue( 0.0f, 0.0f, 0.0f, 0.0f );
			break;
		case UT_TEXOFFSET:
			u->SetValue( es->texofs[0], es->texofs[1] );
			break;
		case UT_VIEWORIGIN:
			u->SetValue( GetVieworg().x, GetVieworg().y, GetVieworg().z );
			break;
		case UT_VIEWRIGHT:
			u->SetValue( GetVRight().x, GetVRight().y, GetVRight().z );
			break;
		case UT_RENDERCOLOR:
			if( e->curstate.rendermode == kRenderNormal )
			{
				r = g = b = a = 1.0f;
			}
			else
			{
				int sum = (e->curstate.rendercolor.r + e->curstate.rendercolor.g + e->curstate.rendercolor.b);

				if(( sum > 0 ) && !FBitSet( s->flags, SURF_CONVEYOR ) && !FBitSet( mat->flags, BRUSH_CONVEYOR ))
				{
					r = e->curstate.rendercolor.r / 255.0f;
					g = e->curstate.rendercolor.g / 255.0f;
					b = e->curstate.rendercolor.b / 255.0f;
				}
				else
				{
					r = g = b = 1.0f;
				}

				if (e->curstate.rendermode != kRenderTransAlpha) {
					a = e->curstate.renderamt / 255.0f;
				}
				else {
					a = 1.0f;
				}
			}
			u->SetValue( r, g, b, a );
			break;
		case UT_SMOOTHNESS:
			if( FBitSet( mat->flags, BRUSH_MULTI_LAYERS ) && land && land->terrain )
			{
				terrain_t *terra = land->terrain;
				u->SetValue( &terra->layermap.smoothness[0], terra->numLayers );
			}
			else u->SetValue( mat->impl->smoothness );
			break;
		case UT_SHADOWMATRIX:
			if( pl ) u->SetValue( &pl->gl_shadowMatrix[0][0], MAX_SHADOWMAPS );
			break;
		case UT_SHADOWSPLITDIST:
			v = RI->view.parallelSplitDistances;
			u->SetValue( v[0], v[1], v[2], v[3] );
			break;
		case UT_TEXELSIZE:
			u->SetValue( 1.0f / (float)sunSize[0], 1.0f / (float)sunSize[1], 1.0f / (float)sunSize[2], 1.0f / (float)sunSize[3] );
			break;
		case UT_LIGHTGAMMA:
			u->SetValue(tr.light_gamma);
			break;
		case UT_LIGHTDIR:
			if( pl )
			{
				if( pl->type == LIGHT_DIRECTIONAL ) lightdir = -tr.sky_normal;
				else lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
				u->SetValue( lightdir.x, lightdir.y, lightdir.z, pl->fov );
			}
			break;
		case UT_LIGHTDIFFUSE:
			if( pl ) u->SetValue( pl->color.x, pl->color.y, pl->color.z );
			break;
		case UT_LIGHTORIGIN:
			if( pl ) u->SetValue( pl->origin.x, pl->origin.y, pl->origin.z, ( 1.0f / pl->radius )); 
			break;
		case UT_LIGHTVIEWPROJMATRIX:
			if( pl )
			{
				GLfloat gl_lightViewProjMatrix[16];
				pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );
				u->SetValue( &gl_lightViewProjMatrix[0] );
			}
			break;
		case UT_AMBIENTFACTOR:
			if( pl && pl->type == LIGHT_DIRECTIONAL )
				u->SetValue( tr.sun_ambient );
			else u->SetValue( tr.ambientFactor );
			break;
		case UT_SUNREFRACT:
			u->SetValue( tr.sun_refract );
			break;
		case UT_LERPFACTOR:
			u->SetValue( es->lerpFactor );
			break;
		case UT_REFRACTSCALE:
			u->SetValue( bound( 0.0f, mat->impl->refractScale, 1.0f ));
			break;
		case UT_REFLECTSCALE:
			u->SetValue( bound( 0.0f, mat->impl->reflectScale, 1.0f ));
			break;
		case UT_ABERRATIONSCALE:
			u->SetValue( bound( 0.0f, mat->impl->aberrationScale, 1.0f ));
			break;
		case UT_BOXMINS:
			if( world->num_cubemaps > 0 )
			{
				Vector mins[2];
				mins[0] = es->cubemap[0]->mins;
				mins[1] = es->cubemap[1]->mins;
				u->SetValue( &mins[0][0], 2 );
			}
			break;
		case UT_BOXMAXS:
			if( world->num_cubemaps > 0 )
			{
				Vector maxs[2];
				maxs[0] = es->cubemap[0]->maxs;
				maxs[1] = es->cubemap[1]->maxs;
				u->SetValue( &maxs[0][0], 2 );
			}
			break;
		case UT_CUBEORIGIN:
			if( world->num_cubemaps > 0 )
			{
				Vector origin[2];
				origin[0] = es->cubemap[0]->origin;
				origin[1] = es->cubemap[1]->origin;
				u->SetValue( &origin[0][0], 2 );
			}
			break;
		case UT_CUBEMIPCOUNT:
			if( world->num_cubemaps > 0 )
			{
				r = Q_max( 1, es->cubemap[0]->numMips - cv_cube_lod_bias->value );
				g = Q_max( 1, es->cubemap[1]->numMips - cv_cube_lod_bias->value );
				u->SetValue( r, g );
			}
			break;
		case UT_LIGHTNUMS0:
			u->SetValue( (float)e->lights[0], (float)e->lights[1], (float)e->lights[2], (float)e->lights[3] );
			break;
		case UT_LIGHTNUMS1:
			u->SetValue( (float)e->lights[4], (float)e->lights[5], (float)e->lights[6], (float)e->lights[7] );
			break;
		default:
			ALERT( at_error, "%s: unhandled uniform %s\n", RI->currentshader->name, u->name );
			break;
		}
	}
}

/*
=================
R_SortSolidBrushFaces

=================
*/
static int R_SortSolidBrushFaces( const CSolidEntry *a, const CSolidEntry *b )
{
	msurface_t	*surf1, *surf2;
	mextrasurf_t	*esrf1, *esrf2;

	surf1 = a->m_pSurf;
	surf2 = b->m_pSurf;
	esrf1 = surf1->info;
	esrf2 = surf2->info;

	if( esrf1->forwardScene[0].GetHandle() > esrf2->forwardScene[0].GetHandle() )
		return 1;

	if( esrf1->forwardScene[0].GetHandle() < esrf2->forwardScene[0].GetHandle() )
		return -1;

	if( esrf1->forwardScene[1].GetHandle() > esrf2->forwardScene[1].GetHandle() )
		return 1;

	if( esrf1->forwardScene[1].GetHandle() < esrf2->forwardScene[1].GetHandle() )
		return -1;

	if( surf1->texinfo->texture->gl_texturenum > surf2->texinfo->texture->gl_texturenum )
		return 1;

	if( surf1->texinfo->texture->gl_texturenum < surf2->texinfo->texture->gl_texturenum )
		return -1;

	if( esrf1->lightmaptexturenum > esrf2->lightmaptexturenum )
		return 1;

	if( esrf1->lightmaptexturenum < esrf2->lightmaptexturenum )
		return -1;

	if( esrf1->parent > esrf2->parent )
		return 1;

	if( esrf1->parent < esrf2->parent )
		return -1;

	//if( esrf1->parent->hCachedMatrix > esrf2->parent->hCachedMatrix )
	//	return 1;

	//if( esrf1->parent->hCachedMatrix < esrf2->parent->hCachedMatrix )
	//	return -1;

	return 0;
}

/*
================
R_RenderDynLightList

================
*/
void R_BuildFaceListForLight( CDynLight *pl, bool solid )
{
	RI->currententity = GET_ENTITY( 0 );
	RI->currentmodel = RI->currententity->model;
	RI->frame.light_faces.RemoveAll();
	RI->frame.light_grass.RemoveAll();
	tr.modelorg = pl->origin;

	if( solid )
	{
		// only visible polys passed through the light list
		for( int i = 0; i < RI->frame.solid_faces.Count(); i++ )
		{
			CSolidEntry *entry = &RI->frame.solid_faces[i];

			if( entry->m_bDrawType != DRAWTYPE_SURFACE )
				continue;

			mextrasurf_t *es = entry->m_pSurf->info;
			gl_state_t *glm = GL_GetCache( es->parent->hCachedMatrix );
			RI->currententity = es->parent;
			RI->currentmodel = RI->currententity->model;

			if( FBitSet( RI->currententity->curstate.effects, EF_FULLBRIGHT ))
				continue;

			bool worldpos = R_StaticEntity( es->parent ) ? true : false;
			CFrustum	*frustum = (worldpos) ? &pl->frustum : NULL;
			tr.modelorg = glm->GetModelOrigin();

			R_AddGrassToDrawList( entry->m_pSurf, DRAWLIST_LIGHT );

			if( R_CullSurface( entry->m_pSurf, tr.modelorg, frustum ))
				continue;

			// move from main list into light list
			R_AddSurfaceToDrawList( entry->m_pSurf, DRAWLIST_LIGHT );
		}
	}
	else
	{
		// only visible polys passed through the light list
		for( int i = 0; i < RI->frame.trans_list.Count(); i++ )
		{
			CTransEntry *entry = &RI->frame.trans_list[i];

			if( entry->m_bDrawType != DRAWTYPE_SURFACE )
				continue;

			mextrasurf_t *es = entry->m_pSurf->info;
			gl_state_t *glm = GL_GetCache( es->parent->hCachedMatrix );
			RI->currententity = es->parent;
			RI->currentmodel = RI->currententity->model;

			if( FBitSet( RI->currententity->curstate.effects, EF_FULLBRIGHT ))
				continue;

			bool worldpos = R_StaticEntity( es->parent ) ? true : false;
			CFrustum	*frustum = (worldpos) ? &pl->frustum : NULL;
			tr.modelorg = glm->GetModelOrigin();

			R_AddGrassToDrawList( entry->m_pSurf, DRAWLIST_LIGHT );

			if( R_CullSurface( entry->m_pSurf, tr.modelorg, frustum ))
				continue;

			// move from main list into light list
			R_AddSurfaceToDrawList( entry->m_pSurf, DRAWLIST_LIGHT );
		}
	}
}

/*
================
R_DrawLightForSurfList

setup light projection for each 
================
*/
void R_DrawLightForSurfList( CDynLight *pl )
{
	material_t	*cached_material = NULL;
	cl_entity_t	*cached_entity = NULL;
	bool		flush_buffer = false;
	int		startv, endv;

	pglBindVertexArray( world->vertex_array_object );
	pglBlendFunc( GL_ONE, GL_ONE );
	float y2 = (float)RI->view.port[3] - pl->h - pl->y;
	pglScissor( pl->x, y2, pl->w, pl->h );
	numTempElems = 0;

	for( int i = 0; i < RI->frame.light_faces.Count(); i++ )
	{
		CSolidEntry *entry = &RI->frame.light_faces[i];

		if( entry->m_bDrawType != DRAWTYPE_SURFACE )
			continue;

		mextrasurf_t *es = entry->m_pSurf->info;
		cl_entity_t *e = RI->currententity = es->parent;
		RI->currentmodel = e->model;
		msurface_t *s = entry->m_pSurf;

		if( !entry->m_hProgram ) continue;

		material_t *mat = R_TextureAnimation( s )->material;

		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->m_hProgram] ))
			flush_buffer = true;

		if( cached_entity != RI->currententity )
			flush_buffer = true;

		if( cached_material != mat )
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElements( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.c_total_tris += (numTempElems / 3);
				r_stats.num_flushes_total++;
				numTempElems = 0;
			}

			flush_buffer = false;
			startv = MAX_MAP_ELEMS;
			endv = 0;
		}

		// now cache values
		cached_entity = RI->currententity;
		cached_material = mat;

		if( numTempElems == 0 ) // new chain has started, apply uniforms
			R_SetSurfaceUniforms( entry->m_hProgram, entry->m_pSurf, ( i == 0 ));
		startv = Q_min( startv, es->firstvertex );
		endv = Q_max( es->firstvertex + es->numverts, endv );

		R_DrawSurface( es );
	}

	if( numTempElems )
	{
		pglDrawRangeElements( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.c_total_tris += (numTempElems / 3);
		r_stats.num_flushes_total++;
		startv = MAX_MAP_ELEMS;
		numTempElems = 0;
		endv = 0;
	}

	R_DrawLightForGrass( pl );
}

/*
================
R_RenderDynLightList

draw dynamic lights for world and bmodels
================
*/
void R_RenderDynLightList( bool solid )
{
	ZoneScoped;

	if( FBitSet( RI->params, RP_ENVVIEW|RP_SKYVIEW ))
		return;

	if( !FBitSet( RI->view.flags, RF_HASDYNLIGHTS ))
		return;

	if( R_FullBright( )) return;

	GL_DEBUG_SCOPE();
	GL_Blend( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );

	CDynLight *pl = tr.dlights;

	for( int i = 0; i < MAX_DLIGHTS; i++, pl++ )
	{
		if( pl->Expired( )) continue;

		if( pl->type == LIGHT_SPOT || pl->type == LIGHT_OMNI )
		{
			if( !pl->Active( )) continue;

			if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ))
				continue;

			if( R_CullFrustum( &pl->frustum ))
				continue;

			pglEnable( GL_SCISSOR_TEST );
		}
		else
		{
			// couldn't use scissor for sunlight
			pglDisable( GL_SCISSOR_TEST );
		}

		RI->currentlight = pl;

		// draw world from light position
		R_BuildFaceListForLight( pl, solid );

		if( !RI->frame.light_faces.Count() && !RI->frame.light_grass.Count() )
			continue;	// no interaction with this light?

		R_DrawLightForSurfList( pl );
	}

	GL_CleanupDrawState();
	pglDisable( GL_SCISSOR_TEST );
	RI->currentlight = NULL;
}

/*
================
R_RenderSolidBrushList
================
*/
void R_RenderSolidBrushList( void )
{
	ZoneScoped;

	cl_entity_t	*cached_entity = NULL;
	material_t	*cached_material = NULL;
	int		cached_mirror = -1;
	int		cached_lightmap = -1;
	qboolean		flush_buffer = false;
	mcubemap_t	*cached_cubemap[2];
	int		startv, endv;

	if( !RI->frame.solid_faces.Count() )
		return;

	RI->frame.solid_faces.Sort( R_SortSolidBrushFaces );
	GL_DEBUG_SCOPE();
	GL_Blend( GL_FALSE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_TRUE );
	pglAlphaFunc( GL_GREATER, 0.25f );
	numTempElems = 0;

	if( GL_Support( R_SEAMLESS_CUBEMAP ))
		pglEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

	pglBindVertexArray( world->vertex_array_object );
	cached_cubemap[0] = &world->defaultCubemap;
	cached_cubemap[1] = &world->defaultCubemap;

	for( int i = 0; i < RI->frame.solid_faces.Count(); i++ )
	{
		CSolidEntry *entry = &RI->frame.solid_faces[i];

		if( entry->m_bDrawType != DRAWTYPE_SURFACE )
			continue;

		mextrasurf_t *es = entry->m_pSurf->info;
		cl_entity_t *e = RI->currententity = es->parent;
		RI->currentmodel = e->model;
		msurface_t *s = entry->m_pSurf;

		if( !entry->m_hProgram ) continue;

		material_t *mat = R_TextureAnimation( s )->material;

		if ((i == 0) || (RI->currentshader != &glsl_programs[entry->m_hProgram])) {
			flush_buffer = true;
			r_stats.solid_brush_list_flushes.num_flushes_shader++;
		}

		if (cached_entity != RI->currententity) {
			flush_buffer = true;
			r_stats.solid_brush_list_flushes.num_flushes_entity++;
		}

		if (cached_material != mat) {
			flush_buffer = true;
			r_stats.solid_brush_list_flushes.num_flushes_material++;
		}

		if (cached_lightmap != es->lightmaptexturenum) {
			flush_buffer = true;
			r_stats.solid_brush_list_flushes.num_flushes_lightmap++;
		}

		if (cached_mirror != es->subtexture[glState.stack_position]) {
			flush_buffer = true;
			r_stats.solid_brush_list_flushes.num_flushes_mirrortex++;
		}

		if (ShaderUseCubemaps(RI->currentshader) && (cached_cubemap[0] != es->cubemap[0] || cached_cubemap[1] != es->cubemap[1])) {
			flush_buffer = true;
			r_stats.solid_brush_list_flushes.num_flushes_cubemap++;
		}

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElements( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.c_total_tris += (numTempElems / 3);
				r_stats.num_flushes_total++;
				numTempElems = 0;
			}

			flush_buffer = false;
			startv = MAX_MAP_ELEMS;
			endv = 0;
		}

		// now cache values
		cached_entity = RI->currententity = es->parent;
		cached_lightmap = es->lightmaptexturenum;
		RI->currentmodel = es->parent->model;
		cached_mirror = es->subtexture[glState.stack_position];
		cached_cubemap[0] = es->cubemap[0];
		cached_cubemap[1] = es->cubemap[1];
		cached_material = mat;

		if( numTempElems == 0 ) // new chain has started, apply uniforms
			R_SetSurfaceUniforms( entry->m_hProgram, entry->m_pSurf, ( i == 0 ));
		startv = Q_min( startv, es->firstvertex );
		endv = Q_max( es->firstvertex + es->numverts, endv );

		R_DrawSurface( es );
	}

	if( numTempElems )
	{
		ZoneScopedN("Surfaces batch draw");
		pglDrawRangeElements( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.c_total_tris += (numTempElems / 3);
		r_stats.num_flushes_total++;
		startv = MAX_MAP_ELEMS;
		numTempElems = 0;
		endv = 0;
	}

	if( GL_Support( R_SEAMLESS_CUBEMAP ))
		pglDisable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

	GL_DepthRange( gldepthmin, gldepthmax );
	GL_CleanupDrawState();
	GL_AlphaTest( GL_FALSE );
	GL_ClipPlane( true );
	GL_Cull( GL_FRONT );

	// render all decals with alpha-channel
	R_RenderDecalsSolidList( DRAWLIST_SOLID );

	// draw grass on visible surfaces
	R_RenderGrassOnList();

	// draw dynamic lighting for world and bmodels
	R_RenderDynLightList( true );

	GL_CleanupDrawState();

	// render all decals with gray base
	R_RenderDecalsSolidList( DRAWLIST_TRANS );
}

void R_RenderTransSurface( CTransEntry *entry )
{
	if( entry->m_bDrawType != DRAWTYPE_SURFACE )
		return;

	if( !entry->m_hProgram ) 
		return;

	mextrasurf_t *es = entry->m_pSurf->info;
	cl_entity_t *e = RI->currententity = es->parent;
	msurface_t *s = entry->m_pSurf;
	RI->currentmodel = e->model;
	int startv = MAX_MAP_ELEMS;
	numTempElems = 0;
	int endv = 0;
	bool screenCopyRequired = ScreenCopyRequired(&glsl_programs[entry->m_hProgram]);

	GL_DEBUG_SCOPE();
	if (screenCopyRequired)
	{
		if( !FBitSet( s->flags, SURF_OCCLUDED ))
		{
			entry->RequestScreencopy();
			r_stats.c_screen_copy++;
		}
	}

	pglBindVertexArray( world->vertex_array_object );
	R_SetSurfaceUniforms( entry->m_hProgram, entry->m_pSurf, true );
	startv = Q_min( startv, es->firstvertex );
	endv = Q_max( es->firstvertex + es->numverts, endv );

	GL_AlphaTest(GL_FALSE);
	if( FBitSet( glsl_programs[entry->m_hProgram].status, SHADER_ADDITIVE ))
	{
		GL_DepthMask( GL_FALSE );
		GL_Blend( GL_TRUE );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	}
	else
	{
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		GL_DepthMask( GL_TRUE );

		if (screenCopyRequired)
			GL_Blend(GL_FALSE);
		else
			GL_Blend(GL_TRUE);
	}
	R_DrawSurface( es );

	if( numTempElems )
	{
		pglDrawRangeElements( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.c_total_tris += (numTempElems / 3);
		r_stats.num_flushes_total++;
		numTempElems = 0;
	}

	GL_DepthMask( GL_TRUE );
	GL_Blend( GL_FALSE );
	GL_ClipPlane( true );
	GL_Cull( GL_FRONT );
}

/*
================
R_RenderShadowBrushList

================
*/
void R_RenderShadowBrushList( void )
{
	int			cached_matrix = -1;
	material_t *cached_material = NULL;
	qboolean	flush_buffer = false;
	int			startv, endv;

	if( !RI->frame.solid_faces.Count() )
		return;

	GL_DEBUG_SCOPE();
	pglBindVertexArray( world->vertex_array_object );
	pglAlphaFunc( GL_GREATER, 0.25f );
	numTempElems = 0;

	for( int i = 0; i < RI->frame.solid_faces.Count(); i++ )
	{
		CSolidEntry *entry = &RI->frame.solid_faces[i];

		if (entry->m_bDrawType != DRAWTYPE_SURFACE)
			continue;

		msurface_t *surf = entry->m_pSurf;
		mextrasurf_t *esurf = surf->info;
		cl_entity_t *entity = RI->currententity = esurf->parent;
		material_t *mat = surf->texinfo->texture->material;
		RI->currentmodel = entity->model;
		
		if (!entry->m_hProgram)
			continue;

		if ((i == 0) || (RI->currentshader != &glsl_programs[entry->m_hProgram]))
			flush_buffer = true;

		if (cached_matrix != esurf->parent->hCachedMatrix)
			flush_buffer = true;

		if (cached_material != mat)
			flush_buffer = true;

		if( flush_buffer )
		{
			if( numTempElems )
			{
				pglDrawRangeElements( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
				r_stats.c_total_tris += (numTempElems / 3);
				r_stats.num_flushes_total++;
				numTempElems = 0;
			}

			flush_buffer = false;
			startv = MAX_MAP_ELEMS;
			endv = 0;
		}

		// now cache values
		cached_matrix = esurf->parent->hCachedMatrix;
		cached_material = mat;

		if( numTempElems == 0 ) // new chain has started, apply uniforms
			R_SetSurfaceUniforms( entry->m_hProgram, entry->m_pSurf, ( i == 0 ));

		startv = Q_min( startv, esurf->firstvertex );
		endv = Q_max( esurf->firstvertex + esurf->numverts, endv );

		R_DrawSurface( esurf );
	}

	if( numTempElems )
	{
		pglDrawRangeElements( GL_TRIANGLES, startv, endv - 1, numTempElems, GL_UNSIGNED_INT, tempElems );
		r_stats.c_total_tris += (numTempElems / 3);
		r_stats.num_flushes_total++;
		startv = MAX_MAP_ELEMS;
		numTempElems = 0;
		endv = 0;
	}

	R_RenderShadowGrassOnList();
	GL_CleanupDrawState();
}
