/*
r_garss.cpp - grass rendering
Copyright (C) 2013 SovietCoder

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
#include "mathlib.h"
#include "r_world.h"
#include "r_shader.h"

#define LEAF_MAX_EXPAND	32.0f

grassvert_t		grassverts[GRASS_VERTS];
grasstex_t		grasstexs[GRASS_TEXTURES];
CUtlArray<grassentry_t>	grassInfo;

/*
================
R_ReLightGrass

We already have a valid spot on texture
Just find lightmap point and update grass color
================
*/
void R_ReLightGrass( msurface_t *surf, bool force )
{
	grasshdr_t *hdr = surf->info->grass;
	bool update_lightcache = false;

	if( !hdr ) return;

	for( int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++ )
	{
		if( force || ( tr.lightstylevalue[surf->styles[maps]] != hdr->cached_light[maps] ))
		{
			if( r_dynamic->value || force )
				update_lightcache = true;
			break;
		}
	}

	if( !update_lightcache )
		return;

	float sample_size = Mod_SampleSizeForFace( surf );
	unsigned int lightcolor[3];
	grass_t *g = hdr->g;

	for( int i = 0; i < hdr->count; i++, g++ )
	{
		g->color[3] = 255;

		if( !worldmodel->lightdata )
		{
			// just to get fullbright
			g->color[0] = g->color[1] = g->color[2] = 255;

			for( int j = 0; j < 16; j++ )
				*(int *)g->mesh[j].c = *(int *)g->color;
			continue;
		}

		if( !surf->samples )
		{
			// no light here
			g->color[0] = g->color[1] = g->color[2] = 0;

			for( int j = 0; j < 16; j++ )
				*(int *)g->mesh[j].c = *(int *)g->color;
			continue;
		}

		mtexinfo_t *tex = surf->texinfo;

		// NOTE: don't need to trace here because we already have a valid surface spot. just read lightmap color and out
		float s = ( DotProduct( g->pos, surf->info->lmvecs[0] ) + surf->info->lmvecs[0][3] - surf->info->lightmapmins[0] );
		float t = ( DotProduct( g->pos, surf->info->lmvecs[1] ) + surf->info->lmvecs[1][3] - surf->info->lightmapmins[1] );
		int smax = (surf->info->lightextents[0] / sample_size) + 1;
		int tmax = (surf->info->lightextents[1] / sample_size) + 1;
		int size = smax * tmax;

		// some bushes may be out of current poly
		s = bound( 0, s, surf->info->lightextents[0] );
		t = bound( 0, t, surf->info->lightextents[1] );
		s /= sample_size;
		t /= sample_size;

		color24 *lm = surf->samples + Q_rint( t ) * smax + Q_rint( s );
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 0;

		for( int map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			unsigned int scale = tr.lightstylevalue[surf->styles[map]];

			lightcolor[0] += TEXTURE_TO_TEXGAMMA( lm->r ) * scale;
			lightcolor[1] += TEXTURE_TO_TEXGAMMA( lm->g ) * scale;
			lightcolor[2] += TEXTURE_TO_TEXGAMMA( lm->b ) * scale;
			lm += size; // skip to next lightmap
		}

		// convert to normal rangle
		g->color[0] = Q_min((lightcolor[0] >> 7), 255 );
		g->color[1] = Q_min((lightcolor[1] >> 7), 255 );
		g->color[2] = Q_min((lightcolor[2] >> 7), 255 );

		for( int j = 0; j < 16; j++ )
			*(int *)g->mesh[j].c = *(int *)g->color;
	}

	// cache new lightvalues
	for( maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++ )
		hdr->cached_light[maps] = tr.lightstylevalue[surf->styles[maps]];
}

/*
================
R_GrassInitForSurface

lookup all the descriptions
because this system allow many
sorts of grass per one surface
================
*/
void R_GrassInitForSurface( msurface_t *surf )
{
	if( !surf || !surf->texinfo || !surf->texinfo->texture )
		return; // some bad polygons

	mfaceinfo_t *land = surf->texinfo->faceinfo;
	mextrasurf_t *es = surf->info;
	bvert_t *v0, *v1, *v2;

	for( int i = 0; i < grassInfo.Count(); i++ )
	{
		grassentry_t *entry = &grassInfo[i];

		if( !Mod_CheckLayerNameForSurf( surf, entry->name ))
			continue;

		if( !FBitSet( surf->flags, SURF_PLANEBACK ) && surf->plane->normal.z < 0.40f )
			continue; // vertical too much

		if( FBitSet( surf->flags, SURF_PLANEBACK ) && surf->plane->normal.z > -0.40f )
			continue; // vertical too much

		// update random set to get predictable positions for grass 'random' placement
		RANDOM_SEED( entry->seed );

		// turn the face into a bunch of polygons, and compute the area of each
		v0 = &world->vertexes[es->firstvertex];
		int count = 0;

		for( int j = 1; j < es->numverts - 1; j++ )
		{
			v1 = &world->vertexes[es->firstvertex+j];
			v2 = &world->vertexes[es->firstvertex+j+1];

			// compute two triangle edges
			Vector e1 = v1->vertex - v0->vertex;
			Vector e2 = v2->vertex - v0->vertex;

			// compute the triangle area
			Vector areaVec = CrossProduct( e1, e2 );
			float normalLength = areaVec.Length();
			float area = 0.5f * normalLength;

			// compute the maximum number of samples to take
			count += area * entry->density * 0.0001f;
		}

		surf->info->grasscount += count;
	}

	if( surf->info->grasscount > 0 )
		SetBits( world->features, WORLD_HAS_GRASS );

	// restore normal randomization
	RANDOM_SEED( 0 );
}

/*
================
R_BuildGrassMesh

Create mesh for VBO
================
*/
void R_BuildGrassMesh( grasshdr_t *grass, grass_t *g )
{
	float s2 = 16.0f * g->size;
	float s3 = 24.0f * g->size;
	float pos;
	int i;

	if( !g->mesh )
	{
		ALERT( at_error, "R_BuildGrassMesh: g->mesh == NULL\n" );
		return;
	}

	for( i = 0; i < 16; i++ )
		*(int *)g->mesh[i].c = *(int *)g->color;

	for( i = 0; i < 16; i += 4 )
	{
		g->mesh[i+0].t[0] = 0;
		g->mesh[i+0].t[1] = 1;
		g->mesh[i+1].t[0] = 0;
		g->mesh[i+1].t[1] = 0;
		g->mesh[i+2].t[0] = 1;
		g->mesh[i+2].t[1] = 0;
		g->mesh[i+3].t[0] = 1;
		g->mesh[i+3].t[1] = 1;
	}

	pos = g->pos[0] + s2;
	if( pos > grass->maxs[0] )
		grass->maxs[0] = pos;

	pos = g->pos[0] - s2;
	if( pos < grass->mins[0] )
		grass->mins[0] = pos;

	pos = g->pos[1] + s2;
	if( pos > grass->maxs[1] )
		grass->maxs[1] = pos;

	pos = g->pos[1] - s2;
	if( pos < grass->mins[1] )
		grass->mins[1] = pos;

	pos = g->pos[2] + s3;
	if( pos > grass->maxs[2] )
		grass->maxs[2] = pos;
}

/*
================
R_ConstructGrass

Fill polygon when player see him
================
*/
void R_ConstructGrass( msurface_t *psurf )
{
	char *texname = psurf->texinfo->texture->name;
	mfaceinfo_t *land = psurf->texinfo->faceinfo;
	mextrasurf_t *es = psurf->info;
	bvert_t *v0, *v1, *v2;
	grassentry_t *entry;
	grasshdr_t *hdr;
	int total = 0;

	// already init or not specified?
	if( es->grass || !es->grasscount )
		return;

	for( int i = 0; i < grassInfo.Count(); i++ )
	{
		entry = &grassInfo[i];

		if( !Mod_CheckLayerNameForSurf( psurf, entry->name ))
			continue;

		hdr = es->grass;

		if( !hdr ) // first call?
		{
			size_t mesh_size = sizeof( grassvert_t ) * 16; // single mesh
			size_t meshes_size = mesh_size * es->grasscount;	// all meshes
			size_t grasshdr_size = sizeof( grasshdr_t ) + sizeof( grass_t ) * ( es->grasscount - 1 );
			hdr = es->grass = (grasshdr_t *)IEngineStudio.Mem_Calloc( 1, grasshdr_size + meshes_size );
			memset( hdr->cached_light, -1, sizeof( hdr->cached_light )); // relight for next update
			tr.grass_total_size += grasshdr_size + meshes_size;
			hdr->mins = es->mins;
			hdr->maxs = es->maxs;
			hdr->dist = -1.0f;

			byte *ptr = (byte *)hdr + grasshdr_size; // grass meshes goes immediately after all grass_t

			// set pointers for meshes
			for( int k = 0; k < es->grasscount; k++ )
			{
				grass_t *g = &hdr->g[k];
				g->mesh = (grassvert_t *)ptr;
				ptr += mesh_size;
			}
		}

		// update random set to get predictable positions for grass 'random' placement
		RANDOM_SEED( entry->seed );

		// turn the face into a bunch of polygons, and compute the area of each
		v0 = &world->vertexes[es->firstvertex];

		for( int j = 1; j < es->numverts - 1; j++ )
		{
			v1 = &world->vertexes[es->firstvertex+j];
			v2 = &world->vertexes[es->firstvertex+j+1];

			// compute two triangle edges
			Vector e1 = v1->vertex - v0->vertex;
			Vector e2 = v2->vertex - v0->vertex;

			// compute the triangle area
			Vector areaVec = CrossProduct( e1, e2 );
			float normalLength = areaVec.Length();
			float area = 0.5f * normalLength;

			// compute the number of samples to take
			int numSamples = area * entry->density * 0.0001f;
			grass_t *g = &hdr->g[hdr->count];

			for( int k = 0; k < numSamples; k++ )
			{
				// Create a random sample...
				float u = RANDOM_FLOAT( 0.0f, 1.0f );
				float v = RANDOM_FLOAT( 0.0f, 1.0f );

				if( v > ( 1.0f - u ))
				{
					u = 1.0f - u;
					v = 1.0f - v;
				}

				g->pos = v0->vertex + e1 * u + e2 * v;

				if( !Mod_CheckLayerNameForPixel( land, g->pos, entry->name ))
					continue;

				g->color[0] = g->color[1] = g->color[2] = g->color[3] = 255;

				g->texture = entry->texture;
				g->size = RANDOM_FLOAT( entry->min, entry->max );

				R_BuildGrassMesh( hdr, g );
				hdr->radius = RadiusFromBounds( hdr->mins, hdr->maxs );

				hdr->count++;
				total++;
				g++;
			}
		}
	}

	if( total ) ALERT( at_aiconsole, "Surface %i, %i bushes created\n", psurf - worldmodel->surfaces, total );
	if( total > psurf->info->grasscount )
		ALERT( at_error, "R_ConstructGrass: overflow %i > %i. Memory is corrupted!\n", total, psurf->info->grasscount );

	// restore normal randomization
	RANDOM_SEED( 0 );
}

/*
================
R_GrassTextureForName

find or add unique texture for grass
================
*/
byte R_GrassTextureForName( const char *name )
{
	for( byte i = 0; i < GRASS_TEXTURES && grasstexs[i].name[0]; i++ )
	{
		if( !Q_stricmp( grasstexs[i].name, name ))
			return i;	// found
	}

	// allocate a new one
	Q_strncpy( grasstexs[i].name, name, sizeof( grasstexs[i].name ));
	grasstexs[i].gl_texturenum = LOAD_TEXTURE( name, NULL, 0, TF_CLAMP );

	if( !grasstexs[i].gl_texturenum )
	{
		ALERT( at_warning, "couldn't load %s\n", name );
		grasstexs[i].gl_texturenum = tr.defaultTexture;
	}

	return i;
}

/*
================
R_GrassInit

parse grass definitions and load textures
================
*/
void R_GrassInit( void )
{
	static int random_seed = 1; // starts from 1

	char *afile = (char *)gEngfuncs.COM_LoadFile( "gfx/grass/grassinfo.txt", 5, NULL );
	if( !afile ) ALERT( at_error, "couldn't load grassinfo.txt\n" );

	// remove grass description from the pervious map
	grassInfo.Purge();

	memset( grasstexs, 0, sizeof( grasstexs ));

	if( afile )
	{
		grassentry_t entry;
		char *pfile = afile;
		int parse_line = 1;
		char token[1024];

		while(( pfile = COM_ParseFile( pfile, token )) != NULL )
		{
			Q_strncpy( entry.name, token, sizeof( entry.name ));

			pfile = COM_ParseLine( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "R_GrassInit: missed grass texture path at line %i\n", parse_line );
				parse_line++;
				continue;
			}

			entry.texture = R_GrassTextureForName( token );

			pfile = COM_ParseLine( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "R_GrassInit: missed grass density at line %i\n", parse_line );
				parse_line++;
				continue;
			}
			entry.density = Q_atof( token );
			entry.density = bound( 0.1f, entry.density, 512.0f );

			pfile = COM_ParseLine( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "R_GrassInit: missed grass min scale at line %i\n", parse_line );
				parse_line++;
				continue;
			}
			entry.min = Q_atof( token );
			entry.min = bound( 0.01f, entry.min, 100.0f );

			pfile = COM_ParseLine( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "R_GrassInit: missed grass max scale at line %i\n", parse_line );
				parse_line++;
				continue;
			}
			entry.max = Q_atof( token );
			entry.max = bound( entry.min, entry.max, 100.0f );
			if( entry.min > entry.max ) entry.min = entry.max;

			pfile = COM_ParseLine( pfile, token );
			if( pfile )
			{
				// seed is optional
				entry.seed = Q_atoi( token );
				entry.seed = max( 1, entry.seed );
			}
			else entry.seed = random_seed++;

			grassInfo.AddToTail( entry );
			parse_line++;
		}

		gEngfuncs.COM_FreeFile( afile );
	}
}

/*
================
R_GrassShutdown

prepare grass system to shutdown
================
*/
void R_GrassShutdown( void )
{
	// release all grass textures
	for( int i = 0; i < GRASS_TEXTURES; i++ )
	{
		if( !grasstexs[i].gl_texturenum )
			continue;

		FREE_TEXTURE( grasstexs[i].gl_texturenum );
	}
}

/*
================
R_ReSizeGrass

distance disappearing
================
*/
static inline void R_ReSizeGrass( grass_t *g, float s )
{
	float scale = s * g->size;

	float s1 = 12.0f * scale;
	float s2 = 16.0f * scale;
	float s3 = 24.0f * scale;

	g->mesh[0].v[0] = g->pos[0] - s2;	g->mesh[0].v[1] = g->pos[1];		g->mesh[0].v[2] = g->pos[2];
	g->mesh[1].v[0] = g->pos[0] - s2;	g->mesh[1].v[1] = g->pos[1];		g->mesh[1].v[2] = g->pos[2] + s3;
	g->mesh[2].v[0] = g->pos[0] + s2;	g->mesh[2].v[1] = g->pos[1];		g->mesh[2].v[2] = g->pos[2] + s3;
	g->mesh[3].v[0] = g->pos[0] + s2;	g->mesh[3].v[1] = g->pos[1];		g->mesh[3].v[2] = g->pos[2];

	g->mesh[4].v[0] = g->pos[0];		g->mesh[4].v[1] = g->pos[1] - s2;	g->mesh[4].v[2] = g->pos[2];
	g->mesh[5].v[0] = g->pos[0];		g->mesh[5].v[1] = g->pos[1] - s2;	g->mesh[5].v[2] = g->pos[2] + s3;
	g->mesh[6].v[0] = g->pos[0];		g->mesh[6].v[1] = g->pos[1] + s2;	g->mesh[6].v[2] = g->pos[2] + s3;
	g->mesh[7].v[0] = g->pos[0];		g->mesh[7].v[1] = g->pos[1] + s2;	g->mesh[7].v[2] = g->pos[2];

	g->mesh[8].v[0] = g->pos[0] - s1;	g->mesh[8].v[1] = g->pos[1] - s1;	g->mesh[8].v[2] = g->pos[2];
	g->mesh[9].v[0] = g->pos[0] - s1;	g->mesh[9].v[1] = g->pos[1] - s1;	g->mesh[9].v[2] = g->pos[2] + s3;
	g->mesh[10].v[0] = g->pos[0] + s1;	g->mesh[10].v[1] = g->pos[1] + s1;	g->mesh[10].v[2] = g->pos[2] + s3;
	g->mesh[11].v[0] = g->pos[0] + s1;	g->mesh[11].v[1] = g->pos[1] + s1;	g->mesh[11].v[2] = g->pos[2];

	g->mesh[12].v[0] = g->pos[0] - s1;	g->mesh[12].v[1] = g->pos[1] + s1;	g->mesh[12].v[2] = g->pos[2];
	g->mesh[13].v[0] = g->pos[0] - s1;	g->mesh[13].v[1] = g->pos[1] + s1;	g->mesh[13].v[2] = g->pos[2] + s3;
	g->mesh[14].v[0] = g->pos[0] + s1;	g->mesh[14].v[1] = g->pos[1] - s1;	g->mesh[14].v[2] = g->pos[2] + s3;
	g->mesh[15].v[0] = g->pos[0] + s1;	g->mesh[15].v[1] = g->pos[1] - s1;	g->mesh[15].v[2] = g->pos[2];
}

/*
================
R_AnimateGrass

animate nearest bushes
================
*/
static inline void R_AnimateGrass( grass_t *g, float time )
{
	float s1 = 12.0f * g->size;
	float s2 = 16.0f * g->size;
	float s3 = 24.0f * g->size;

	float movex, movey;
	SinCos( g->pos[0] + g->pos[1] + time, &movex, &movey );

	g->mesh[0].v[0] = g->pos[0] - s2;		g->mesh[0].v[1] = g->pos[1];			g->mesh[0].v[2] = g->pos[2];
	g->mesh[1].v[0] = g->pos[0] - s2 + movex;	g->mesh[1].v[1] = g->pos[1] + movey;		g->mesh[1].v[2] = g->pos[2] + s3;
	g->mesh[2].v[0] = g->pos[0] + s2 + movex;	g->mesh[2].v[1] = g->pos[1] + movey;		g->mesh[2].v[2] = g->pos[2] + s3;
	g->mesh[3].v[0] = g->pos[0] + s2;		g->mesh[3].v[1] = g->pos[1];			g->mesh[3].v[2] = g->pos[2];

	g->mesh[4].v[0] = g->pos[0];			g->mesh[4].v[1] = g->pos[1] - s2;		g->mesh[4].v[2] = g->pos[2];
	g->mesh[5].v[0] = g->pos[0] + movex;		g->mesh[5].v[1] = g->pos[1] - s2 + movey;	g->mesh[5].v[2] = g->pos[2] + s3;
	g->mesh[6].v[0] = g->pos[0] + movex;		g->mesh[6].v[1] = g->pos[1] + s2 + movey;	g->mesh[6].v[2] = g->pos[2] + s3;
	g->mesh[7].v[0] = g->pos[0];			g->mesh[7].v[1] = g->pos[1] + s2;		g->mesh[7].v[2] = g->pos[2];

	g->mesh[8].v[0] = g->pos[0] - s1;		g->mesh[8].v[1] = g->pos[1] - s1;		g->mesh[8].v[2] = g->pos[2];
	g->mesh[9].v[0] = g->pos[0] - s1 + movex;	g->mesh[9].v[1] = g->pos[1] - s1 + movey;	g->mesh[9].v[2] = g->pos[2] + s3;
	g->mesh[10].v[0] = g->pos[0] + s1 + movex;	g->mesh[10].v[1] = g->pos[1] + s1 + movey;	g->mesh[10].v[2] = g->pos[2] + s3;
	g->mesh[11].v[0] = g->pos[0] + s1;		g->mesh[11].v[1] = g->pos[1] + s1;		g->mesh[11].v[2] = g->pos[2];

	g->mesh[12].v[0] = g->pos[0] - s1;		g->mesh[12].v[1] = g->pos[1] + s1;		g->mesh[12].v[2] = g->pos[2];
	g->mesh[13].v[0] = g->pos[0] - s1 + movex;	g->mesh[13].v[1] = g->pos[1] + s1 + movey;	g->mesh[13].v[2] = g->pos[2] + s3;
	g->mesh[14].v[0] = g->pos[0] + s1 + movex;	g->mesh[14].v[1] = g->pos[1] - s1 + movey;	g->mesh[14].v[2] = g->pos[2] + s3;
	g->mesh[15].v[0] = g->pos[0] + s1;		g->mesh[15].v[1] = g->pos[1] - s1;		g->mesh[15].v[2] = g->pos[2];
}

/*
================
R_AddToGrassChain

add grass to drawchains
================
*/
bool R_AddGrassToChain( msurface_t *surf, CFrustum *frustum, bool lightpass, mworldleaf_t *leaf )
{
	if( !CVAR_TO_BOOL( r_grass ))
		return false;

	mextrasurf_t *es = surf->info;
	grasshdr_t *grass = es->grass;
	bool normalpass = false;

	if( !FBitSet( world->features, WORLD_HAS_GRASS ) || FBitSet( RI->params, RP_NOGRASS ))
		return false; // don't waste time

	qboolean shadowpass = RI->params & RP_SHADOWVIEW;
	
	if(( shadowpass && !CVAR_TO_BOOL( r_grass_shadows )) || ( lightpass && !CVAR_TO_BOOL( r_grass_lighting ))) 
		return false;

	if( !lightpass && !shadowpass )
		normalpass = true;

	float fadestart = r_grass_fade_start->value;
	if( fadestart < GRASS_ANIM_DIST ) 
		fadestart = GRASS_ANIM_DIST;
	float fadedist = abs( r_grass_fade_dist->value );
	float fadeend = fadestart + fadedist;	// draw_dist

	if( es->grasscount && !grass )
	{
		float cur_dist;

		cur_dist = ( tr.cached_vieworigin - es->origin ).Length();

		// poly is too far and grass may be construct later
		// to prevent draw lag
		if( cur_dist > fadeend )
			return false;

		// initialize grass for surface
		R_ConstructGrass( surf );
		grass = es->grass;

		if( grass && leaf )
		{
			// prevent to expand leafs too much
			AddPointToBounds( grass->mins, leaf->mins, leaf->maxs, LEAF_MAX_EXPAND );
			AddPointToBounds( grass->maxs, leaf->mins, leaf->maxs, LEAF_MAX_EXPAND );
		}
	}

	// we are in special mode!
	if( leaf ) return false;

	if( grass )
	{
		grass->dist = ( RI->vieworg - es->origin ).Length();
		cl_entity_t *e = RI->currententity;
		Vector mins, maxs;

		// only static ents can be culled by frustum
		if( !R_StaticEntity( e )) frustum = NULL;

		if( e->angles != g_vecZero )
		{
			for( int i = 0; i < 3; i++ )
			{
				mins[i] = e->origin[i] - grass->radius;
				maxs[i] = e->origin[i] + grass->radius;
			}
		}
		else
		{
			mins = e->origin + grass->mins;
			maxs = e->origin + grass->maxs;
		}

		if( frustum && frustum->CullBox( mins, maxs ))
			return false;	// invisible

		if( grass->dist <= fadeend )
		{
			float scale = 1.0;
			int resize = false;
			int animate = false;

			if( normalpass )
				R_ReLightGrass( surf );

			if( normalpass && grass->dist > GRASS_ANIM_DIST )
			{
				if( grass->dist > fadestart )
					scale = Q_ceil( ( fadeend - grass->dist ) / GRASS_SCALE_STEP ) * GRASS_SCALE_STEP / fadedist;

				if( grass->scale != scale )
				{
					grass->scale = scale;
					resize = true;
				}
			}
			else if( normalpass && (( grass->animtime < tr.time ) || ( grass->animtime > ( tr.time + GRASS_ANIM_TIME * 2.0f ))))
			{
				grass->animtime = tr.time + GRASS_ANIM_TIME;
				grass->scale = 0.0f;
				animate = true;
			}

			// build chains by texture
			for( int i = 0; i < grass->count; i++ )
			{
				grass_t *g = &grass->g[i];

				if( grasstexs[g->texture].gl_texturenum != 0 )
				{
					if( resize ) R_ReSizeGrass( g, scale );
					if( animate ) R_AnimateGrass( g, tr.time );
					g->chain = grasstexs[g->texture].grasschain;
					grasstexs[g->texture].grasschain = g;
				}
			}
		}
	}

	if( lightpass )
		tr.num_light_grass++;
	else tr.num_draw_grass++;

	return true;
}

void R_RemoveGrassForSurface( mextrasurf_t *es )
{
	// not specified?
	if( !es->grass ) return;

	size_t mesh_size = sizeof( grassvert_t ) * 16; // single mesh
	size_t meshes_size = mesh_size * es->grasscount;	// all meshes
	size_t grasshdr_size = sizeof( grasshdr_t ) + sizeof( grass_t ) * ( es->grasscount - 1 );
	tr.grass_total_size -= grasshdr_size + meshes_size;

	grasshdr_t *hdr = es->grass;
	es->grass = NULL;
	Mem_Free( hdr );
}

/*
================
R_UnloadFarGrass

release far VBO's
================
*/
void R_UnloadFarGrass( void )
{
	if( !FBitSet( world->features, WORLD_HAS_GRASS ))
		return; // don't waste time

	if( ++tr.grassunloadframe < 1000 )
		return; // thinking ~one tick per second

	float draw_dist = r_grass_fade_start->value;

	if( draw_dist < GRASS_ANIM_DIST )
		draw_dist = GRASS_ANIM_DIST;
	draw_dist += abs( r_grass_fade_dist->value );

	// check surfaces
	for( int i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		mextrasurf_t *es = surf->info;

		if( !es->grasscount ) continue; // surface doesn't contain grass
		float curdist = ( tr.cached_vieworigin - es->origin ).Length();

		if( curdist > ( draw_dist * 1.2f ) && es->grass )
			R_RemoveGrassForSurface( es );
	}

	tr.grassunloadframe = 0;
}

/*
================
R_DrawGrass

multi-pass rendering
================
*/
void R_DrawGrass( qboolean lightpass )
{
	bool normalpass = false;

	if( !FBitSet( world->features, WORLD_HAS_GRASS ))
		return; // don't waste time

	if( !tr.num_draw_grass && !tr.num_light_grass )
		return;

	qboolean shadowpass = FBitSet( RI->params, RP_SHADOWVIEW );

	if( !lightpass && !shadowpass )
		normalpass = true;

	GL_SelectTexture( GL_TEXTURE0 ); // keep texcoords at 0-th unit
	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 2, GL_SHORT, sizeof( grassvert_t ), grassverts->t );
	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, sizeof( grassvert_t ), grassverts->v );

	if( normalpass )
	{
		pglEnableClientState( GL_COLOR_ARRAY );
		pglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( grassvert_t ), grassverts->c );

		if( tr.fogEnabled && normalpass )
		{
			GL_BindShader( glsl.genericFog );
			pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
		}
	}

	GL_Cull( GL_NONE );
	pglEnable( GL_ALPHA_TEST );
	pglAlphaFunc( GL_GREATER, bound( 0.0f, r_grass_alpha->value, 1.0f ));
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	for( int i = 0; i < GRASS_TEXTURES; i++ )
	{
		grass_t	*g = grasstexs[i].grasschain;

		if( !g || !grasstexs[i].gl_texturenum )
			continue;

		GL_Bind( GL_TEXTURE0, grasstexs[i].gl_texturenum );
		grassvert_t *grasspos = grassverts;
		int numverts = 0;

		for( ; g != NULL; g = g->chain )
		{
			memcpy( grasspos, g->mesh, sizeof( grassvert_t ) * 16 );
			grasspos += 16;
			numverts += 16;

			// array is full, time to dump				
			if( numverts == GRASS_VERTS )
			{
				r_stats.c_grass_polys += (numverts / 4);
				r_stats.c_total_tris += (numverts / 2);
				pglDrawArrays( GL_QUADS, 0, numverts );
				grasspos = grassverts;
				r_stats.num_flushes++;
				numverts = 0;
			}
		}

		// flush all remaining vertices
		if( numverts )
		{
			r_stats.c_grass_polys += (numverts / 4);
			r_stats.c_total_tris += (numverts / 2);
			pglDrawArrays( GL_QUADS, 0, numverts );
			r_stats.num_flushes++;
		}

		grasstexs[i].grasschain = NULL;
	}

	if( tr.fogEnabled && normalpass )
		GL_BindShader( GL_NONE );
	if( !lightpass && !shadowpass )
		pglDisableClientState( GL_COLOR_ARRAY );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	pglDisableClientState( GL_VERTEX_ARRAY );
	tr.num_draw_grass = tr.num_light_grass = 0;
	pglAlphaFunc( GL_GREATER, 0.25f );
	pglDisable( GL_ALPHA_TEST );
	GL_Cull( GL_FRONT );
}

/*
================
R_DrawGrass

multi-pass rendering
================
*/
void R_DrawGrassLight( plight_t *pl )
{
	word	shaderNum = GL_UberShaderForDlightGeneric( pl );
	GLfloat	gl_lightViewProjMatrix[16];

	if( !tr.num_light_grass || !shaderNum || tr.nodlights )
		return;

	pglBindVertexArray( GL_FALSE );
	GL_BindShader( &glsl_programs[shaderNum] );			
	ASSERT( RI->currentshader != NULL );

	Vector lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
	pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );

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
	pglUniform1fARB( RI->currentshader->u_LightScale, 1.0f );

	GL_Bind( GL_TEXTURE1, pl->projectionTexture );
	GL_Bind( GL_TEXTURE2, pl->shadowTexture );

	R_DrawGrass( true );

	pglBindVertexArray( world->vertex_array_object );
	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
}
