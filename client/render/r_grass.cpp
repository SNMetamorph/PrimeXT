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
#include "event_api.h"

grasshdr_t		*grasschain[3];
grassvert_t		grassverts[GRASS_VERTS];
grasstex_t		grasstexs[GRASS_TEXTURES];
CUtlArray<grasspinfo_t>	grassInfo;

/*
================
R_ReLightGrass

We already have a valid spot on texture
Just find lightmap point and update grass color
================
*/
void R_ReLightGrass( msurface_t *surf, bool force )
{
	mextrasurf_t *es = SURF_INFO( surf, RI.currentmodel );
	grasshdr_t *hdr = es->grass;

	if( !hdr ) return;

	for( int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++ )
	{
		if( force || ( RI.lightstylevalue[surf->styles[maps]] != hdr->cached_light[maps] ))
		{
			unsigned int lightcolor[3];
			grass_t *g = hdr->g;

			for( int i = 0; i < hdr->count; i++, g++ )
			{
				if( !RI.currentmodel->lightdata )
				{
					// just to get fullbright
					g->color[0] = g->color[1] = g->color[2] = 255;
					continue;
				}

				if( !surf->samples )
				{
					// no light here
					g->color[0] = g->color[1] = g->color[2] = 0;
					continue;
				}

				mtexinfo_t *tex = surf->texinfo;

				// NOTE: don't need to call R_LightForPoint here because we already have a valid surface spot
				// just read lightmap color and out
				int s = (DotProduct( g->pos, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0]) / LM_SAMPLE_SIZE;
				int t = (DotProduct( g->pos, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1]) / LM_SAMPLE_SIZE;
				color24 *lm = surf->samples + (t * ((surf->extents[0] / LM_SAMPLE_SIZE ) + 1) + s);
				int size = ((surf->extents[0] / LM_SAMPLE_SIZE) + 1) * ((surf->extents[1] / LM_SAMPLE_SIZE) + 1);
				lightcolor[0] = lightcolor[1] = lightcolor[2] = 0;

				for( int map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
				{
					unsigned int scale = RI.lightstylevalue[surf->styles[map]];

					lightcolor[0] += TEXTURE_TO_TEXGAMMA( lm->r ) * scale;
					lightcolor[1] += TEXTURE_TO_TEXGAMMA( lm->g ) * scale;
					lightcolor[2] += TEXTURE_TO_TEXGAMMA( lm->b ) * scale;
					lm += size; // skip to next lightmap
				}

				// convert to normal rangle
				g->color[0] = min((lightcolor[0] >> 7), 255 );
				g->color[1] = min((lightcolor[1] >> 7), 255 );
				g->color[2] = min((lightcolor[2] >> 7), 255 );

				for( int j = 0; j < 16; j++ )
					*(int *)g->mesh[j].c = *(int *)g->color;
			}

			// NOTE: 'maps' used twice but is no matter because we breaking main cycle 				
			for( maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++ )
				hdr->cached_light[maps] = RI.lightstylevalue[surf->styles[maps]];

			break;
		}
	}
}

/*
================
R_AddGrass

Load grass textures, compute total count
================
*/
void R_AddGrass( grasspinfo_t *ppinfo )
{
	grasstex_t *gtex;
	msurface_t *surf;
	texture_t *stex = NULL;
	int i;

	if( !ppinfo )
	{
		ALERT( at_error, "grassinfo is NULL!\n" );
		return;
	}

	for( i = 0; i < worldmodel->numtextures; i++ )
	{
		if( !Q_stricmp( ppinfo->stex, worldmodel->textures[i]->name ))
			stex = worldmodel->textures[i];
	}

	if( !stex ) 
	{
		ALERT( at_warning, "Invalid world texture in grass info: \"%s\"\n", ppinfo->stex );
		return;
	}

	for( i = 0, gtex = grasstexs; i < GRASS_TEXTURES; i++, gtex++ )
	{
		if( !gtex->name[0] )
		{
			gtex->gl_texturenum = LOAD_TEXTURE( ppinfo->gtex, NULL, 0, TF_CLAMP );
			Q_strncpy( gtex->name, ppinfo->gtex, sizeof( ppinfo->gtex ));

			if( !gtex->gl_texturenum )
			{
				// show level-designer what texture is missed
				ALERT( at_warning, "Invalid grass texture in grass info: \"%s\"\n", ppinfo->gtex );
				gtex->gl_texturenum = tr.defaultTexture;
			}

			break;
		}
		else if( !Q_stricmp( gtex->name, ppinfo->gtex )) 
			break;
	}

	if( i == GRASS_TEXTURES )
	{
		ALERT( at_warning, "Grass textures limit exceeded!\n" );
		return;
	}

	// save texture slot
	ppinfo->texture = i;

	// store grass description for build it later
	grassInfo.AddToTail( *ppinfo );

	for( surf = stex->texturechain; surf; surf = surf->texturechain )
	{
		mextrasurf_t *es = SURF_INFO( surf, worldmodel );
		if( !( surf->flags & SURF_PLANEBACK ) && surf->plane->normal.z < 0.40f )
			continue; // vertical too much
		if( ( surf->flags & SURF_PLANEBACK ) && surf->plane->normal.z > -0.40f )
			continue; // vertical too much

		// skip grass for too thick polys
		if(( es->maxs[0] - es->mins[0] ) < 4.0f )
			continue;

		if(( es->maxs[1] - es->mins[1] ) < 4.0f )
			continue;

		Vector size = (es->maxs - es->mins);
		es->grasscount += GRASS_DENSITY( size, ppinfo->density );
	}
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
	mextrasurf_t *es = SURF_INFO( psurf, worldmodel );
	grasspinfo_t *ppinfo;
	grasshdr_t *hdr;
	int total = 0;

	// already init or not specified?
	if( es->grass || !es->grasscount || !es->mesh )
		return;

	for( int i = 0; i < grassInfo.Count(); i++ )
	{
		ppinfo = &grassInfo[i];

		if( !Q_stricmp( texname, ppinfo->stex ))
		{
			// begin build the grass
			Vector size = (es->maxs - es->mins);
			int count = GRASS_DENSITY( size, ppinfo->density );
			if( !count ) continue; // skip to next grass definition for this surface (if present) 

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
				for( int i = 0; i < es->grasscount; i++ )
				{
					grass_t *g = &hdr->g[i];
					g->mesh = (grassvert_t *)ptr;
					ptr += mesh_size;
				}
                              }

			// update random set to get predictable positions for grass 'random' placement
			RANDOM_SEED( ppinfo->seed );

			int numTris = (es->mesh->numElems / 3);
			count = (count / numTris);

			grass_t *g = &hdr->g[hdr->count];

			for( int j = 0; j < numTris; j++ )
			{
				// get triangle
				Vector *v0 = &es->mesh->verts[es->mesh->elems[j*3+0]].vertex;
				Vector *v1 = &es->mesh->verts[es->mesh->elems[j*3+1]].vertex;
				Vector *v2 = &es->mesh->verts[es->mesh->elems[j*3+2]].vertex;

				for( int k = 0; k < count; k++ )
				{
					float a = RANDOM_FLOAT( 0.0f, 1.0f ); 
					float b = RANDOM_FLOAT( 0.0f, 1.0f ); 

					if(( a + b ) > 1.0f )
					{
						a = 1.0f - a;
						b = 1.0f - b;
					}

					float c = 1.0f - a - b;

					g->pos = (*v0 * a) + (*v1 * b) + (*v2 * c);
					g->color[0] = g->color[1] = g->color[2] = 255;

					g->texture = ppinfo->texture;
					g->size = RANDOM_FLOAT( ppinfo->min, ppinfo->max );

					R_BuildGrassMesh( hdr, g );
					hdr->radius = RadiusFromBounds( hdr->mins, hdr->maxs );

					hdr->count++;
					total++;
					g++;
				}
			}
		}
	}

	if( total ) ALERT( at_aiconsole, "Surface %i, %i bushes created\n", psurf - worldmodel->surfaces, total );

	// restore normal randomization
	RANDOM_SEED( 0 );
}

/*
================
R_ParseGrassFile

parse grass defs for current level
================
*/
void R_ParseGrassFile( void )
{
	char file[80];
	char token[256];
	char mapname[64];
	char *afile, *pfile;

	tr.grass_total_size = 0;

	// remove grass description from the pervious map
	grassInfo.Purge();

	Q_strcpy( mapname, worldmodel->name );
	COM_StripExtension( mapname );

	Q_snprintf( file, sizeof( file ), "%s_grass.txt", mapname );
	ALERT( at_aiconsole, "Load grass info file %s\n", file );
	afile = (char *)gEngfuncs.COM_LoadFile( file, 5, NULL );
	
	if( afile )
	{
		grasspinfo_t pinfo;
		int i;

		for( i = 0; i < GRASS_TEXTURES; i++ )
			grasstexs[i].name[0] = NULL;

		// NOTE: bmodels are includes too
		for( i = 0; i < worldmodel->numsurfaces; i++ )
		{
			msurface_t *surf = &worldmodel->surfaces[i];
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
		}

		pfile = afile;

		while(( pfile = COM_ParseFile( pfile, token )) != NULL )
		{
			Q_strncpy( pinfo.stex, token, sizeof( pinfo.stex ));

			ParceGrassFirstPass();
			Q_strncpy( pinfo.gtex, token, sizeof( pinfo.gtex ));

			ParceGrassFirstPass();
			pinfo.density = Q_atof( token );
			pinfo.density = bound( 0.1f, pinfo.density, 256.0f );

			ParceGrassFirstPass();
			pinfo.min = Q_atof( token );
			pinfo.min = bound( 0.01f, pinfo.min, 100.0f );

			ParceGrassFirstPass();
			pinfo.max = Q_atof( token );
			pinfo.max = bound( 0.01f, pinfo.max, 100.0f );

			ParceGrassFirstPass();
			pinfo.seed = Q_atoi( token );
			pinfo.seed = bound( 1, pinfo.seed, 65535 );

			R_AddGrass( &pinfo );
		}

		gEngfuncs.COM_FreeFile( afile );

		for( i = 0; i < worldmodel->numtextures; i++ )
			worldmodel->textures[i]->texturechain = NULL;
	}
}

/*
================
R_AddToGrassChain

add grass to drawchains
================
*/
void R_AddToGrassChain( msurface_t *surf, const mplane_t frustum[6], unsigned int clipflags, qboolean lightpass )
{
	mextrasurf_t *es = SURF_INFO( surf, RI.currentmodel );
	grasshdr_t *grass = es->grass;

	qboolean shadowpass = RI.params & RP_SHADOWVIEW;
	
	if( !r_grass->value || ( shadowpass && !r_grass_shadows->value ) || ( !shadowpass && lightpass && !r_grass_lighting->value )) 
		return;

	float draw_dist = r_grass_fade_start->value;

	if( draw_dist < GRASS_ANIM_DIST )
		draw_dist = GRASS_ANIM_DIST;
	draw_dist += abs( r_grass_fade_dist->value );

	if( es->grasscount && !grass )
	{
		float cur_dist;

		if( shadowpass || ( RI.params & RP_MIRRORVIEW ))
			cur_dist = ( r_lastRefdef.vieworg - es->origin ).Length();
		else cur_dist = ( RI.vieworg - es->origin ).Length();

		// poly is too far and grass may be construct later
		// to prevent draw lag
		if( cur_dist > draw_dist ) return;

		// initialize grass for surface
		R_ConstructGrass( surf );
		grass = es->grass;
	}

	if( grass )
	{
		if( grass->cullframe != tr.grassframecount )
		{
			grass->cullframe = tr.grassframecount;

			if( shadowpass || ( RI.params & RP_MIRRORVIEW ))
				grass->dist = ( r_lastRefdef.vieworg - es->origin ).Length();
			else grass->dist = ( RI.vieworg - es->origin ).Length();		
		}

		cl_entity_t *e = RI.currententity;
		Vector mins, maxs;

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

		if( !R_CullBoxExt( frustum, mins, maxs, clipflags ) && ( grass->dist <= draw_dist ))
		{
			if( !lightpass )
				R_ReLightGrass( surf );

			if( !lightpass || shadowpass )
			{
				grass->chain[0] = grasschain[0];
				grasschain[0] = grass;
			}
			else
			{
				grass->chain[2] = grasschain[2];
				grasschain[2] = grass;
				
				if( grass->renderframe != tr.grassframecount )
				{
					grass->chain[1] = grasschain[1];
					grasschain[1] = grass;
					grass->renderframe = tr.grassframecount;
				}
			}
		}
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
R_DrawGrass

multi-pass rendering
================
*/
void R_DrawGrass( int pass )
{
	grassvert_t *grasspos = grassverts;
	int numverts = 0;
	int nexttex = 0;
	int curtex = 0;
	int cn = 0;

	float fadestart = r_grass_fade_start->value;

	if( fadestart < GRASS_ANIM_DIST ) 
		fadestart = GRASS_ANIM_DIST;

	float fadedist = abs( r_grass_fade_dist->value );
	float fadeend = fadestart + fadedist;

	if( pass == GRASS_PASS_AMBIENT ) 
		cn = 1;
	else if( pass == GRASS_PASS_LIGHT ) 
		cn = 2;

	if( !grasschain[cn] )
		return;

	if( pass == GRASS_PASS_FOG )
	{
		if( !R_SetupFogProjection() )
		{
			// PASS_FOG is always last, so nothing to draw
			memset( grasschain, 0, sizeof( grasschain ));
			return;
		}

		pglDepthFunc( GL_EQUAL );
	}
	else if( pass == GRASS_PASS_NORMAL )
	{
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		R_SetupOverbright( true );	
	}
	else if( pass == GRASS_PASS_DIFFUSE )
	{
		pglEnable( GL_BLEND );
		pglBlendFunc( R_OVERBRIGHT_SFACTOR(), GL_SRC_COLOR );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	}
	else if( pass == GRASS_PASS_AMBIENT )
	{
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
		pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE );
	}
	else if( pass == GRASS_PASS_SHADOW )
		pglEnable( GL_TEXTURE_2D );

	if( pass != GRASS_PASS_LIGHT && pass != GRASS_PASS_FOG )
	{
		pglEnable( GL_ALPHA_TEST );

		pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		pglTexCoordPointer( 2, GL_SHORT, sizeof( grassvert_t ), grassverts->t );
	}

	if( pass == GRASS_PASS_NORMAL || pass == GRASS_PASS_AMBIENT )
	{
		pglEnableClientState( GL_COLOR_ARRAY );
		pglColorPointer( 3, GL_UNSIGNED_BYTE, sizeof( grassvert_t ), grassverts->c );
	}

	GL_Cull( GL_NONE );
	pglAlphaFunc( GL_GEQUAL, bound( 0.0f, r_grass_alpha->value, 1.0f ));

	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, sizeof( grassvert_t ), grassverts->v );

	float time = GET_CLIENT_TIME();

	do 
	{
		numverts = 0;
		curtex = nexttex;
		grasspos = grassverts;

		if( pass != GRASS_PASS_LIGHT && pass != GRASS_PASS_FOG )
			GL_Bind( GL_TEXTURE0, grasstexs[curtex].gl_texturenum );

		for( grasshdr_t *chain = grasschain[cn]; chain; chain = chain->chain[cn] )
		{
			if( pass == GRASS_PASS_NORMAL && chain->renderframe == tr.grassframecount )
				continue;

			if( pass == GRASS_PASS_DIFFUSE && chain->renderframe != tr.grassframecount )
				continue;

			if( chain->dist > fadeend )
				continue;

			float scale = 1.0;
			int resize = false;
			int animate = false;

			if( chain->dist > GRASS_ANIM_DIST )
			{
				if( chain->dist > fadestart )
					scale = ceil( ( fadeend - chain->dist ) / GRASS_SCALE_STEP ) * GRASS_SCALE_STEP / fadedist;

				if( chain->scale != scale )
				{
					resize = true;
					chain->scale = scale;	
				}
			}
			else if(( chain->animtime < time ) || ( chain->animtime > ( time + GRASS_ANIM_TIME * 2.0f )))
			{
				animate = true;
				chain->scale = 0.0f;
				chain->animtime = time + GRASS_ANIM_TIME;
			}
			
			grass_t *grass = chain->g;
			int grasstex = grass->texture;

			for( int i = 0; i < chain->count; i++, grass++, grasstex = grass->texture )
			{
				if( resize ) R_ReSizeGrass( grass, scale );
				if( animate ) R_AnimateGrass( grass, time );

				if( grasstex != curtex )
				{
					if( grasstex > curtex )
					{
						if( nexttex > curtex )
						{
							if( nexttex > grasstex )
								nexttex = grasstex;
						}
						else
							nexttex = grasstex;
					}

					continue;
				}
				
				memcpy( grasspos, grass->mesh, sizeof( grassvert_t ) * 16 );
				grasspos += 16;
				numverts += 16;
				
				if( numverts == GRASS_VERTS )
				{
					r_stats.c_grass_polys += (numverts / 4);
					pglDrawArrays( GL_QUADS, 0, numverts );
					grasspos = grassverts;
					numverts = 0;
				}
			}
		}

		// flush all remaining vertices
		if( numverts )
		{
			r_stats.c_grass_polys += (numverts / 4);
			pglDrawArrays( GL_QUADS, 0, numverts );
		}
	} while( curtex != nexttex );

	pglDisableClientState( GL_COLOR_ARRAY );
	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	if( pass != GRASS_PASS_LIGHT ) 
		pglDisable( GL_ALPHA_TEST );
	if( pass != GRASS_PASS_NORMAL && pass != GRASS_PASS_DIFFUSE )
		grasschain[cn] = NULL;
	else R_SetupOverbright( false );

	if( pass == GRASS_PASS_SHADOW ) 
		pglDisable( GL_TEXTURE_2D );
	else if( pass == GRASS_PASS_DIFFUSE )
	{
		pglDisable( GL_BLEND );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else if( pass == GRASS_PASS_FOG )
	{
		pglDepthFunc( GL_LEQUAL );
		GL_CleanUpTextureUnits( 0 );
		// PASS_FOG is always last, so nothing to draw
		memset( grasschain, 0, sizeof( grasschain ));
	}
	
	GL_Cull( GL_FRONT );
	pglAlphaFunc( GL_GEQUAL, 0.5f );	
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
}
