/*
gl_rmisc.cpp - renderer miscellaneous
Copyright (C) 2013 Uncle Mike

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
#include "gl_studio.h"
#include "gl_aurora.h"
#include "gl_rpart.h"
#include "material.h"
#include "gl_occlusion.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_shader.h"
#include "gl_cvars.h"
#include "gl_debug.h"
#include "gl_unit_cube.h"
#include "r_weather.h"

#define DEFAULT_SMOOTHNESS	0.0f
#define FILTER_SIZE		2

/*
==================
CL_LoadMaterials

parse material from a given file
==================
*/
void CL_LoadMaterials( const char *path )
{
	COM_LoadMaterials(path);
}

/*
==================
CL_InitMaterials

parse all files with .mat extension
==================
*/
void CL_InitMaterials( void )
{
	COM_InitMaterials(tr.materials, tr.matcount);
}

/*
==================
CL_FindMaterial

This function never failed
==================
*/
matdesc_t *CL_FindMaterial( const char *name )
{
	return COM_FindMaterial(name);
}

/*
========================
LoadHeightMap

parse heightmap pixels and remap it
to real layer count
========================
*/
bool LoadHeightMap( indexMap_t *im, int numLayers )
{
	unsigned int	*src;
	int		i, tex;
	int		depth = 1;

	if( !GL_Support( R_TEXTURE_ARRAY_EXT ) || numLayers <= 0 )
		return false;

	// loading heightmap and keep the source pixels
	if( !( tex = LOAD_TEXTURE( im->name, NULL, 0, TF_KEEP_SOURCE|TF_EXPAND_SOURCE )))
		return false;

	if(( src = (unsigned int *)GET_TEXTURE_DATA( tex )) == NULL )
	{
		ALERT( at_error, "LoadHeightMap: couldn't get source pixels for %s\n", im->name );
		FREE_TEXTURE( tex );
		return false;
	}

	im->gl_diffuse_id = LOAD_TEXTURE( im->diffuse, NULL, 0, 0 );

	int width = RENDER_GET_PARM( PARM_TEX_SRC_WIDTH, tex );
	int height = RENDER_GET_PARM( PARM_TEX_SRC_HEIGHT, tex );

	im->pixels = (byte *)Mem_Alloc( width * height );
	im->numLayers = bound( 1, numLayers, 255 );
	im->height = height;
	im->width = width;

	for( i = 0; i < ( im->width * im->height ); i++ )
	{
		byte rawHeight = ( src[i] & 0xFF );
		im->maxHeight = Q_max(( 16 * (int)ceil( rawHeight / 16 )), im->maxHeight ); 
	}

	// merge layers count
	im->numLayers = (im->maxHeight / 16) + 1;
	depth = Q_max((int)Q_ceil((float)im->numLayers / 4.0f ), 1 );

	// clamp to layers count
	for( i = 0; i < ( im->width * im->height ); i++ )
		im->pixels[i] = (( src[i] & 0xFF ) * ( im->numLayers - 1 )) / im->maxHeight;

	size_t lay_size = im->width * im->height * 4;
	size_t img_size = lay_size * depth;
	byte *layers = (byte *)Mem_Alloc( img_size );
	byte *pixels = (byte *)src;

	for( int x = 0; x < im->width; x++ )
	{
		for( int y = 0; y < im->height; y++ )
		{
			float weights[MAX_LANDSCAPE_LAYERS];

			memset( weights, 0, sizeof( weights ));

			for( int pos_x = 0; pos_x < FILTER_SIZE; pos_x++ ) 
			{ 
				for( int pos_y = 0; pos_y < FILTER_SIZE; pos_y++ ) 
				{ 
					int img_x = (x - (FILTER_SIZE / 2) + pos_x + im->width) % im->width; 
					int img_y = (y - (FILTER_SIZE / 2) + pos_y + im->height) % im->height; 

					float rawHeight = (float)( src[img_y * im->width + img_x] & 0xFF );
					float curLayer = ( rawHeight * ( im->numLayers - 1 )) / (float)im->maxHeight;

					if( curLayer != (int)curLayer )
					{
						byte layer0 = (int)floor( curLayer );
						byte layer1 = (int)ceil( curLayer );
						float factor = curLayer - (int)curLayer;
						weights[layer0] += (1.0 - factor) * (1.0 / (FILTER_SIZE * FILTER_SIZE));
						weights[layer1] += (factor ) * (1.0 / (FILTER_SIZE * FILTER_SIZE));
					}
					else
					{
						weights[(int)curLayer] += (1.0 / (FILTER_SIZE * FILTER_SIZE));
					}
				}
			}

			// encode layers into RGBA channels
			layers[lay_size * 0 + (y * im->width + x)*4+0] = weights[0] * 255;
			layers[lay_size * 0 + (y * im->width + x)*4+1] = weights[1] * 255;
			layers[lay_size * 0 + (y * im->width + x)*4+2] = weights[2] * 255;
			layers[lay_size * 0 + (y * im->width + x)*4+3] = weights[3] * 255;

			if( im->numLayers <= 4 ) continue;

			layers[lay_size * 1 + ((y * im->width + x)*4+0)] = weights[4] * 255;
			layers[lay_size * 1 + ((y * im->width + x)*4+1)] = weights[5] * 255;
			layers[lay_size * 1 + ((y * im->width + x)*4+2)] = weights[6] * 255;
			layers[lay_size * 1 + ((y * im->width + x)*4+3)] = weights[7] * 255;

			if( im->numLayers <= 8 ) continue;

			layers[lay_size * 2 + ((y * im->width + x)*4+0)] = weights[8] * 255;
			layers[lay_size * 2 + ((y * im->width + x)*4+1)] = weights[9] * 255;
			layers[lay_size * 2 + ((y * im->width + x)*4+2)] = weights[10] * 255;
			layers[lay_size * 2 + ((y * im->width + x)*4+3)] = weights[11] * 255;

			if( im->numLayers <= 12 ) continue;

			layers[lay_size * 3 + ((y * im->width + x)*4+0)] = weights[12] * 255;
			layers[lay_size * 3 + ((y * im->width + x)*4+1)] = weights[13] * 255;
			layers[lay_size * 3 + ((y * im->width + x)*4+2)] = weights[14] * 255;
			layers[lay_size * 3 + ((y * im->width + x)*4+3)] = weights[15] * 255;
		}
	}

	// release source texture
	FREE_TEXTURE( tex );

	tex = CREATE_TEXTURE_ARRAY( im->name, im->width, im->height, depth, layers, TF_CLAMP|TF_HAS_ALPHA );
	Mem_Free( layers );

	im->gl_heightmap_id = tex;

	return true;
}

/*
========================
LoadTerrainLayers

loading all the landscape layers
into texture arrays
========================
*/
bool LoadTerrainLayers( layerMap_t *lm, int numLayers )
{
	char	*texnames[MAX_LANDSCAPE_LAYERS];
	char	*ptr, buffer[1024], tmpname[64];
	size_t	nameLen = 64;
	int	i;

	memset( buffer, 0, sizeof( buffer )); // list must be null terminated

	// initialize names array
	for( i = 0, ptr = buffer; i < MAX_LANDSCAPE_LAYERS; i++, ptr += nameLen )
		texnames[i] = ptr;

	// process diffuse textures
	for( i = 0; i < numLayers; i++ )
	{
		Q_snprintf( texnames[i], nameLen, "textures/%s", lm->pathes[i] );
		lm->material[i] = CL_FindMaterial( lm->names[i] );
		lm->smoothness[i] = lm->material[i]->smoothness;
	}

	if(( lm->gl_diffuse_id = LOAD_TEXTURE_ARRAY( (const char **)texnames, 0 )) == 0 )
		return false;

	memset( buffer, 0, sizeof( buffer )); // list must be null terminated

	// process normalmaps
	for( i = 0; i < numLayers; i++ )
	{
		const char *ext = COM_FileExtension( lm->pathes[i] );
		COM_FileBase( lm->pathes[i], tmpname );

		if( !Q_stricmp( ext, "" ))
		{
			Q_snprintf( texnames[i], nameLen, "textures/%s_norm", tmpname );
			if( IMAGE_EXISTS( texnames[i] ))
				continue;

			ALERT( at_warning, "layer normalmap %s is missed\n", texnames[i] );
			break;
		}
		else
		{
			Q_snprintf( texnames[i], nameLen, "textures/%s_norm.%s", tmpname, ext );
			if( FILE_EXISTS( texnames[i] ))
				continue;

			ALERT( at_warning, "layer normalmap %s is missed\n", texnames[i] );
			break;
		}
	}

	if( i == numLayers ) lm->gl_normalmap_id = LOAD_TEXTURE_ARRAY( (const char **)texnames, TF_NORMALMAP );
	if( !lm->gl_normalmap_id ) lm->gl_normalmap_id = tr.normalmapTexture;

	memset( buffer, 0, sizeof( buffer )); // list must be null terminated

	// process glossmaps
	for( i = 0; i < numLayers; i++ )
	{
		const char *ext = COM_FileExtension( lm->pathes[i] );
		COM_FileBase( lm->pathes[i], tmpname );

		if( !Q_stricmp( ext, "" ))
		{
			Q_snprintf( texnames[i], nameLen, "textures/%s_gloss", tmpname );
			if( IMAGE_EXISTS( texnames[i] ))
				continue;

			ALERT( at_warning, "layer glossmap %s is missed\n", texnames[i] );
			break;
		}
		else
		{
			Q_snprintf( texnames[i], nameLen, "textures/%s_gloss.%s", tmpname, ext );
			if( FILE_EXISTS( texnames[i] ))
				continue;

			ALERT( at_warning, "layer glossmap %s is missed\n", texnames[i] );
			break;
		}
	}

	if( i == numLayers ) lm->gl_specular_id = LOAD_TEXTURE_ARRAY( (const char **)texnames, 0 );
	if( !lm->gl_specular_id ) lm->gl_specular_id = tr.blackTexture;

	return true;
}

/*
========================
R_FreeLandscapes

free the landscape definitions
========================
*/
void R_FreeLandscapes( void )
{
	for( int i = 0; i < world->num_terrains; i++ )
	{
		terrain_t *terra = &world->terrains[i];
		indexMap_t *im = &terra->indexmap;
		if( im->pixels ) Mem_Free( im->pixels );
		layerMap_t *lm = &terra->layermap;

		if( lm->gl_diffuse_id )
			FREE_TEXTURE( lm->gl_diffuse_id );

		if( lm->gl_normalmap_id != tr.normalmapTexture )
			FREE_TEXTURE( lm->gl_normalmap_id );

		if( lm->gl_specular_id != tr.blackTexture )
			FREE_TEXTURE( lm->gl_specular_id );

		if( im->gl_diffuse_id != 0 )
			FREE_TEXTURE( im->gl_diffuse_id );

		FREE_TEXTURE( im->gl_heightmap_id );
	}

	if( world->terrains ) Mem_Free( world->terrains );

	world->num_terrains = 0;
	world->terrains = NULL;
}

/*
========================
R_LoadLandscapes

load the landscape definitions
========================
*/
void R_LoadLandscapes( const char *filename )
{
	char filepath[256];

	Q_snprintf( filepath, sizeof( filepath ), "maps/%s_land.txt", filename );

	char *afile = (char *)gEngfuncs.COM_LoadFile( filepath, 5, NULL );
	if( !afile ) return;

	ALERT( at_aiconsole, "loading %s\n", filepath );

	char *pfile = afile;
	char token[256];
	int depth = 0;

	// count materials
	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( Q_strlen( token ) > 1 )
			continue;

		if( token[0] == '{' )
		{
			depth++;
		}
		else if( token[0] == '}' )
		{
			world->num_terrains++;
			depth--;
		}
	}

	if( depth > 0 ) ALERT( at_warning, "%s: EOF reached without closing brace\n", filepath );
	if( depth < 0 ) ALERT( at_warning, "%s: EOF reached without opening brace\n", filepath );

	world->terrains = (terrain_t *)Mem_Alloc( sizeof( terrain_t ) * world->num_terrains );
	pfile = afile; // start real parsing

	int current = 0;

	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( current >= world->num_terrains )
		{
			ALERT ( at_error, "landscape parse is overrun %d > %d\n", current, world->num_terrains );
			break;
		}

		terrain_t *terra = &world->terrains[current];

		// read the landscape name
		Q_strncpy( terra->name, token, sizeof( terra->name ));
		terra->texScale = 1.0f;

		// read opening brace
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( token[0] != '{' )
		{
			ALERT( at_error, "found %s when expecting {\n", token );
			break;
		}

		while( pfile != NULL )
		{
			pfile = COM_ParseFile( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "EOF without closing brace\n" );
				goto land_getout;
			}

			// description end goto next material
			if( token[0] == '}' )
			{
				current++;
				break;
			}
			else if( !Q_stricmp( token, "indexMap" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'indexMap'\n" );
					goto land_getout;
				}

				Q_strncpy( terra->indexmap.name, token, sizeof( terra->indexmap.name ));
			}
			else if( !Q_stricmp( token, "diffuseMap" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'diffuseMap'\n" );
					goto land_getout;
				}

				Q_strncpy( terra->indexmap.diffuse, token, sizeof( terra->indexmap.diffuse ));
			}
			else if( !Q_strnicmp( token, "layer", 5 ))
			{
				int	layerNum = Q_atoi( token + 5 );

				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'layer'\n" );
					goto land_getout;
				}

				if( layerNum < 0 || layerNum > ( MAX_LANDSCAPE_LAYERS - 1 ))
				{
					ALERT( at_error, "%s is out of range. Ignored\n", token );
				}
				else
				{
					Q_strncpy( terra->layermap.pathes[layerNum], token, sizeof( terra->layermap.pathes[0] ));
					COM_FileBase( terra->layermap.pathes[layerNum], terra->layermap.names[layerNum] );
				}

				terra->numLayers = Q_max( terra->numLayers, layerNum + 1 );
			}
			else if( !Q_stricmp( token, "tessSize" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'tessSize'\n" );
					goto land_getout;
				}

				terra->tessSize = Q_atoi( token );
				terra->tessSize = bound( 8, terra->tessSize, 256 );
			}
			else if( !Q_stricmp( token, "texScale" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'texScale'\n" );
					goto land_getout;
				}

				terra->texScale = Q_atof( token );
				terra->texScale = 1.0 / (bound( 0.000001f, terra->texScale, 16.0f ));
			}
			else ALERT( at_warning, "Unknown landscape token %s\n", token );
		}

		if( LoadHeightMap( &terra->indexmap, terra->numLayers ))
		{
			if( LoadTerrainLayers( &terra->layermap, terra->numLayers ))
				terra->valid = true; // all done
		}
	}

land_getout:
	gEngfuncs.COM_FreeFile( afile );
	ALERT( at_console, "%d landscapes parsed\n", current );
}

/*
========================
R_FindTerrain

find the terrain description
========================
*/
terrain_t *R_FindTerrain( const char *texname )
{
	for( int i = 0; i < world->num_terrains; i++ )
	{
		if( !Q_stricmp( texname, world->terrains[i].name ) && world->terrains[i].valid )
			return &world->terrains[i];
	}

	return NULL;
}

/*
==================
R_InitShadowTextures

Re-create shadow textures
if them size was changed
==================
*/
void R_InitShadowTextures( void )
{
	int	i;

	for( i = 0; i < MAX_SHADOWS; i++ )
	{
		if( !tr.shadowTextures[i] ) break;
		FREE_TEXTURE( tr.shadowTextures[i] );
	}

	for( i = 0; i < MAX_SHADOWS; i++ )
	{
		if( !tr.shadowCubemaps[i] ) break;
		FREE_TEXTURE( tr.shadowCubemaps[i] );
	}

	memset( tr.shadowTextures, 0, sizeof( tr.shadowTextures ));
	memset( tr.shadowCubemaps, 0, sizeof( tr.shadowCubemaps ));

	int base_shadow_size = bound( 128, r_shadowmap_size->value, 2048 );

	int shadow_size2D = bound( 128, NearestPOW( base_shadow_size, true ), 2048 );
	tr.fbo_shadow2D.Init( FBO_DEPTH, shadow_size2D, shadow_size2D, FBO_NOTEXTURE );

	int shadow_sizeCM = bound( 128, NearestPOW( base_shadow_size * 0.5f, true ), 1024 );
	tr.fbo_shadowCM.Init( FBO_DEPTH, shadow_sizeCM, shadow_sizeCM, FBO_NOTEXTURE );
}

/*
==================
R_InitCommonTextures

Init common textures for renderer that size
based on screen size
==================
*/
void R_InitCommonTextures( void )
{
	int	i;

	GL_BindFBO( 0 );
	R_InitShadowTextures();

	// release old subview textures
	for( i = 0; i < MAX_SUBVIEW_TEXTURES; i++ )
	{
		if( !tr.subviewTextures[i].texturenum ) break;
		FREE_TEXTURE( tr.subviewTextures[i].texturenum );
	}

	memset( tr.subviewTextures, 0, sizeof( tr.subviewTextures ));

	for( i = 0; i < tr.num_framebuffers; i++ )
	{
		if( !tr.frame_buffers[i].init )
			break;
		R_FreeFrameBuffer( i );
	}

	for( i = 0; i < MAX_SHADOWMAPS; i++ )
	{
		tr.sunShadowFBO[i].Init( FBO_DEPTH, sunSize[i], sunSize[i] );
	}

	tr.fbo_light.Init( FBO_COLOR, glState.width, glState.height, FBO_LINEAR );
	tr.fbo_filter.Init( FBO_COLOR, glState.width, glState.height, FBO_LINEAR );
	tr.fbo_shadow.Init( FBO_COLOR, glState.defWidth, glState.defHeight, FBO_LINEAR );

	// setup the skybox sides
	for( i = 0; i < 6; i++ )
		tr.skyboxTextures[i] = RENDER_GET_PARM( PARM_TEX_SKYBOX, i );

	tr.num_framebuffers = 0;
	tr.num_subview_used = 0;
	tr.glsl_valid_sequence++; // refresh shader cache

	InitPostTextures();
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
====================
*/
void ED_ParseEdict( char **pfile )
{
	int	vertex_light_cache = -1;
	int	surface_light_cache = -1;
	char	modelname[64];
	char	token[2048];

	// go through all the dictionary pairs
	while( 1 )
	{	
		char	keyname[256];

		// parse key
		if(( *pfile = COM_ParseFile( *pfile, token )) == NULL )
			HOST_ERROR( "ED_ParseEdict: EOF without closing brace\n" );

		if( token[0] == '}' ) break; // end of desc

		Q_strncpy( keyname, token, sizeof( keyname ));

		// parse value	
		if(( *pfile = COM_ParseFile( *pfile, token )) == NULL ) 
			HOST_ERROR( "ED_ParseEdict: EOF without closing brace\n" );

		if( token[0] == '}' )
			HOST_ERROR( "ED_ParseEdict: closing brace without data\n" );

		// ignore attempts to set key ""
		if( !keyname[0] ) continue;

		// "wad" field is completely ignored in XashXT
		if( !Q_strcmp( keyname, "wad" ))
			continue;

		// ignore attempts to set value ""
		if( !token[0] ) continue;

		// also grab light settings from p2rad
		if( !Q_strcmp( keyname, "_lightgamma" ))
			tr.light_gamma = Q_atof( token );
		if( !Q_strcmp( keyname, "_dscale" ))
			tr.direct_scale = Q_atof( token );
		if( !Q_strcmp( keyname, "_maxlight" ))
			tr.light_threshold = Q_atof( token );
		if (!Q_strcmp(keyname, "_ambient"))
			tr.ambient_color = Q_atov(token);
		if( !Q_strcmp( keyname, "_smooth" ))
			tr.smoothing_threshold = cos( DEG2RAD( Q_atof( token )));

		// only two fields that we needed
		if( !Q_strcmp( keyname, "model" ))
			Q_strncpy( modelname, token, sizeof( modelname ));

		if( !Q_strcmp( keyname, "vlight_cache" ))
			vertex_light_cache = atoi( token );
		if( !Q_strcmp( keyname, "flight_cache" ))
			surface_light_cache = atoi( token );
	}

	// deal with light cache
	double start = Sys_DoubleTime();
	if( vertex_light_cache > 0 && vertex_light_cache < MAX_LIGHTCACHE )
		g_StudioRenderer.CreateStudioCacheVL( modelname, vertex_light_cache - 1 );
	if( surface_light_cache > 0 && surface_light_cache < MAX_LIGHTCACHE )
		g_StudioRenderer.CreateStudioCacheFL( modelname, surface_light_cache - 1 );
	double end = Sys_DoubleTime();

	r_buildstats.create_light_cache += (end - start);
	r_buildstats.total_buildtime += (end - start);
}

/*
==================
GL_InitModelLightCache

create VBO cache for vertex-lit and lightmapped studio models
==================
*/
void GL_InitModelLightCache( void )
{
	char		*entities = worldmodel->entities;
	static char	worldname[64];
	char		token[2048];

	if (!Q_stricmp( world->name, worldname) && !world->ignore_restart_check)
		return; // just a restart

	world->ignore_restart_check = false;
	Q_strncpy( worldname, world->name, sizeof( worldname ));
	RI->currententity = GET_ENTITY( 0 ); // to have something valid here

	// parse ents to find vertex light cache
	while(( entities = COM_ParseFile( entities, token )) != NULL )
	{
		if( token[0] != '{' )
			HOST_ERROR( "ED_LoadFromFile: found %s when expecting {\n", token );

		ED_ParseEdict( &entities );
	}

	// create lightmap pages right after studiomodel alloc their lightmaps (empty at this moment)
	GL_EndBuildingLightmaps( (worldmodel->lightdata != NULL), FBitSet( world->features, WORLD_HAS_DELUXEMAP ) ? true : false );
}

static void GL_InitScreenFBOTextures(int colorFlags, int depthFlags)
{
	tr.screen_multisample_fbo_texture_color = CREATE_TEXTURE("*screen_temp_fbo_msaa_texture_color", glState.width, glState.height, NULL, colorFlags);
	tr.screen_multisample_fbo_texture_depth = CREATE_TEXTURE("*screen_temp_fbo_msaa_texture_depth", glState.width, glState.height, NULL, depthFlags);
}

static void GL_DestroyScreenFBOTextures()
{
	FREE_TEXTURE(tr.screen_multisample_fbo_texture_color);
	FREE_TEXTURE(tr.screen_multisample_fbo_texture_depth);
	tr.screen_multisample_fbo_texture_color = 0;
	tr.screen_multisample_fbo_texture_depth = 0;
}

void GL_InitMultisampleScreenFBO()
{
	int texColorFlags = TF_NOMIPMAP | TF_HAS_ALPHA | TF_ARB_16BIT | TF_ARB_FLOAT | TF_MULTISAMPLE;
	int texDepthFlags = TF_RT_DEPTH | TF_MULTISAMPLE;
	int texTarget = GL_TEXTURE_2D_MULTISAMPLE;

	GL_DestroyScreenFBOTextures();
	for (int i = 0; !tr.screen_multisample_fbo_texture_color && !tr.screen_multisample_fbo_texture_depth; ++i)
	{
		if (i > 2) {
			HOST_ERROR("GL_InitMultisampleScreenFBO: failed to create FBO textures\n");
		}

		GL_InitScreenFBOTextures(texColorFlags, texDepthFlags);
		if (!tr.screen_multisample_fbo_texture_color || !tr.screen_multisample_fbo_texture_depth)
		{
			texTarget = GL_TEXTURE_2D;
			ClearBits(texColorFlags, TF_MULTISAMPLE);
			ClearBits(texDepthFlags, TF_MULTISAMPLE);
			ALERT(at_warning, "GL_InitMultisampleScreenFBO: failed to create multisample textures, MSAA not available\n");
		}
	}

	if (!tr.screen_multisample_fbo)
		tr.screen_multisample_fbo = GL_AllocDrawbuffer("*screen_multisample_fbo", glState.width, glState.height, 1);
	else
		GL_ResizeDrawbuffer(tr.screen_multisample_fbo, glState.width, glState.height, 1);

	GL_AttachColorTextureToFBO(tr.screen_multisample_fbo, tr.screen_multisample_fbo_texture_color, 0);
	GL_AttachDepthTextureToFBO(tr.screen_multisample_fbo, tr.screen_multisample_fbo_texture_depth);
	GL_CheckFBOStatus(tr.screen_multisample_fbo);
}

void GL_InitTempScreenFBO()
{
	GLenum	MRTBuffers[1] = { GL_COLOR_ATTACHMENT0_EXT };

	if (!GL_Support(R_FRAMEBUFFER_OBJECT) || !GL_Support(R_DRAW_BUFFERS_EXT))
		return;

	if (!tr.screen_hdr_fbo)
		tr.screen_hdr_fbo = GL_AllocDrawbuffer("*screen_temp_fbo", glState.width, glState.height, 1);
	else
		GL_ResizeDrawbuffer(tr.screen_hdr_fbo, glState.width, glState.height, 1);

	FREE_TEXTURE(tr.screen_hdr_fbo_texture_color);
	FREE_TEXTURE(tr.screen_hdr_fbo_texture_depth);

	tr.screen_hdr_fbo_texture_color = CREATE_TEXTURE("*screen_temp_fbo_texture_color", glState.width, glState.height, NULL, TF_HAS_ALPHA | TF_ARB_16BIT | TF_CLAMP | TF_ARB_FLOAT);
	tr.screen_hdr_fbo_texture_depth = CREATE_TEXTURE("*screen_temp_fbo_texture_depth", glState.width, glState.height, NULL, TF_RT_DEPTH | TF_CLAMP);

	GL_Bind(GL_TEXTURE0, tr.screen_hdr_fbo_texture_color);
	pglGenerateMipmap(GL_TEXTURE_2D);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	GL_Bind(GL_TEXTURE0, 0);

	GL_AttachColorTextureToFBO(tr.screen_hdr_fbo, tr.screen_hdr_fbo_texture_color, 0);
	GL_AttachDepthTextureToFBO(tr.screen_hdr_fbo, tr.screen_hdr_fbo_texture_depth);
	GL_CheckFBOStatus(tr.screen_hdr_fbo);

	// generate screen fbo texture mips
	for (int i = 0; i < 6; i++)
	{
		if (tr.screen_hdr_fbo_mip[i] <= 0)
			pglGenFramebuffers(1, &tr.screen_hdr_fbo_mip[i]);
		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, tr.screen_hdr_fbo_mip[i]);
		pglFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, tr.screen_hdr_fbo_texture_color, i + 1);
	}

	pglDrawBuffersARB(ARRAYSIZE(MRTBuffers), MRTBuffers);
	GL_InitMultisampleScreenFBO();
}

void GL_VidInitTempScreenFBO(void)
{
	GL_InitTempScreenFBO();

	// unbind the framebuffer
	GL_BindDrawbuffer(NULL);
}

/*
=================
R_LightToTexGamma
Convert gamma from light-space to linear-space
TODO: get rid of that when completely HDR rendering pipeline will be ready for usage
=================
*/
byte R_LightToTexGamma(byte input)
{
	float a = input / 255.f;
	return static_cast<byte>(pow(a, 1.0f / tr.light_gamma) * 255.f);
}

/*
==================
R_NewMap

Called always when map is changed or restarted
==================
*/
void R_NewMap( void )
{
	// initialize world surfaces, setup special flags
	for( int i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		mextrasurf_t *info = surf->info;

		// clear the in-game set flags
		ClearBits( surf->flags, SURF_NODRAW|SURF_NODLIGHT|SURF_NOSUNLIGHT|SURF_REFLECT_PUDDLE );

		// clear occlusion queries results
		ClearBits( surf->flags, SURF_QUEUED|SURF_OCCLUDED );

		memset( info->subtexture, 0, sizeof( info->subtexture ));
		info->cubemap[0] = &world->defaultCubemap;
		info->cubemap[1] = &world->defaultCubemap;
		info->checkcount = -1;
		info->parent = nullptr;
	}

	// we need to reapply cubemaps to surfaces after restart level
	if( world->num_cubemaps > 0 )
		world->loading_cubemaps = true;

	// reset light settings
	// by default map compilers uses gamma 0.5
	// in case map compiled with P2ST, game parses value from worldspawn entdata
	// but don't reset in case of loading savefile or restart
	if (world->ignore_restart_check)
	{
		tr.light_gamma = 0.5f;
		tr.direct_scale = 2.0f;
		tr.light_threshold = 196.0f;
		tr.ambient_color = g_vecZero;
		tr.smoothing_threshold = 0.642788f; // 50 degrees
	}
}

/*
==================
R_InitDynLightShader

changed dynamic lighting shaders
==================
*/
void R_InitDynLightShader( int type )
{
	char options[MAX_OPTIONS_LENGTH];

	switch( type )
	{
	case LIGHT_SPOT:
		GL_SetShaderDirective( options, "LIGHT_SPOT" );
		break;
	case LIGHT_OMNI:
		GL_SetShaderDirective( options, "LIGHT_OMNI" );
		break;
	default:	return;
		break;
	}

	if (CVAR_TO_BOOL(cv_brdf))
		GL_AddShaderDirective( options, "APPLY_PBS" );

	if (CVAR_TO_BOOL(cv_specular))
		GL_AddShaderDirective( options, "HAS_GLOSSMAP" );

	if (CVAR_TO_BOOL(r_shadows))
	{
		int shadow_smooth_type = static_cast<int>(r_shadows->value);
		// shadow cubemaps only support if GL_EXT_gpu_shader4 is support
		if (type == LIGHT_SPOT || GL_Support(R_EXT_GPU_SHADER4))
		{
			GL_AddShaderDirective(options, "APPLY_SHADOW");
			if (shadow_smooth_type == 2)
				GL_AddShaderDirective(options, "SHADOW_PCF2X2");
			else if (shadow_smooth_type == 3)
				GL_AddShaderDirective(options, "SHADOW_PCF3X3");
			else if (shadow_smooth_type == 4)
				GL_AddShaderDirective(options, "SHADOW_VOGEL_DISK");
		}
	}

	tr.defDynLightShader[type] = GL_FindShader( "deferred/dynlight", "deferred/generic", "deferred/dynlight", options );
}

/*
==================
R_InitDynLightShaders

changed dynamic lighting shaders
==================
*/
void R_InitDynLightShaders( void )
{
	char options[MAX_OPTIONS_LENGTH];

	GL_CleanupDrawState();

	R_InitDynLightShader( LIGHT_SPOT );
	R_InitDynLightShader( LIGHT_OMNI );

	options[0] = '\0';

	if( CVAR_TO_BOOL( cv_deferred_tracebmodels ))
		GL_AddShaderDirective( options, "BSPTRACE_BMODELS" );

	// init deferred shaders
	tr.defSceneShader[0] = GL_FindShader( "deferred/scene_sep", "deferred/generic", "deferred/scene_sep", options );
	tr.defSceneShader[1] = GL_FindShader( "deferred/scene_all", "deferred/generic", "deferred/scene_all", options );
	tr.defLightShader = GL_FindShader( "deferred/light", "deferred/generic", "deferred/light", options );
	tr.bilateralShader = GL_FindUberShader( "deferred/bilateral" );
}

/*
==================
R_VidInit

Called always when "vid_mode" or "fullscreen" cvars is changed
==================
*/
void R_VidInit( void )
{
	// get the actual screen size
	glState.width = RENDER_GET_PARM( PARM_SCREEN_WIDTH, 0 );
	glState.height = RENDER_GET_PARM( PARM_SCREEN_HEIGHT, 0 );

	// TESTTEST
	glState.defWidth = 256;
	glState.defHeight = 192;

	COpenGLUnitCube::GetInstance().Initialize();
	R_InitCommonTextures();
	R_InitCubemapShaders();
	GL_VidInitDrawBuffers();
	if (CVAR_TO_BOOL(gl_hdr)) {
		GL_VidInitTempScreenFBO();
	}
}

/*
==================
GL_MapChanged

Called always when map is changed or restarted
==================
*/
void GL_MapChanged( void )
{
	if( !g_fRenderInitialized )
		return;

	R_InitRefState();

	tr.glsl_valid_sequence = 1;
	tr.grassunloadframe = 0;
	tr.fClearScreen = false;
	tr.realframecount = 1;
	tr.num_cin_used = 0;

	// catch changes of screen
	R_VidInit ();

	g_pParticleSystems.ClearSystems(); // buz

	g_pParticles.Clear();

	CL_ClearDlights();

	ClearDecals();

	R_ResetWeather();

	// don't flush shaders for each map - save time to recompile
	if( num_glsl_programs >= ( MAX_GLSL_PROGRAMS * 0.9f ))
		GL_FreeUberShaders();

	R_NewMap (); // tell the renderer what a new map started

	g_StudioRenderer.VidInit();

	GL_InitModelLightCache();
}