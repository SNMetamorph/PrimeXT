/*
sv_materials.cpp - materials handling and processing at the server-side
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

#include "extdll.h"
#include "sv_materials.h"
#include "com_model.h"
#include "material.h"
#include "triangleapi.h"
#include "physcallback.h"
#include "enginecallback.h"
#include "stringlib.h"

matdesc_t	*sv_materials;
int		sv_matcount;

sv_terrain_t *sv_terrains;
int		sv_numterrains;

/*
==================
SV_InitMaterials

parse global material settings
==================
*/
void SV_InitMaterials()
{
	COM_InitMaterials(sv_materials, sv_matcount);
}

/*
==================
SV_FindMaterial

This function never failed
==================
*/
matdesc_t *SV_FindMaterial(const char *name)
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
static bool LoadHeightMap( sv_indexMap_t *im, int numLayers )
{
	unsigned int	*src;
	byte		*buffer;
	int		i, width, height;
	int		depth = 1;

	if( numLayers <= 0 ) return false;

	// loading heightmap and keep the source pixels
	if(( buffer = (byte *)LOAD_IMAGE_PIXELS( im->name, &width, &height )) == NULL )
	{
		ALERT( at_error, "LoadHeightMap: couldn't get source pixels for %s\n", im->name );
		return false;
	}

	im->pixels = (byte *)Mem_Alloc( width * height );
	im->numLayers = bound( 1, numLayers, 255 );
	im->height = height;
	im->width = width;

	src = (unsigned int *)buffer;

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

	Mem_Free( buffer ); // no reason to keep this data

	return true;
}

/*
========================
LoadTerrainLayers

loading all the landscape layers
into texture arrays
========================
*/
static bool LoadTerrainLayers( sv_layerMap_t *lm, int numLayers )
{
	// setup materials
	for( int i = 0; i < numLayers; i++ )
		lm->effects[i] = SV_FindMaterial( lm->names[i] )->effects;

	return true;
}

/*
========================
SV_FreeLandscapes

free the landscape definitions
========================
*/
void SV_FreeLandscapes( void )
{
	for( int i = 0; i < sv_numterrains; i++ )
	{
		sv_terrain_t *terra = &sv_terrains[i];
		sv_indexMap_t *im = &terra->indexmap;
		if( im->pixels ) Mem_Free( im->pixels );
	}

	if( sv_terrains )
		Mem_Free( sv_terrains );
	sv_numterrains = 0;
	sv_terrains = NULL;
}

/*
========================
SV_LoadLandscapes

load the landscape definitions
========================
*/
void SV_LoadLandscapes( const char *filename )
{
	char filepath[256];

	Q_snprintf( filepath, sizeof( filepath ), "maps/%s_land.txt", filename );

	char *afile = (char *)LOAD_FILE( filepath, NULL );
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
			sv_numterrains++;
			depth--;
		}
	}

	if( depth > 0 ) ALERT( at_warning, "%s: EOF reached without closing brace\n", filepath );
	if( depth < 0 ) ALERT( at_warning, "%s: EOF reached without opening brace\n", filepath );

	sv_terrains = (sv_terrain_t *)Mem_Alloc( sizeof( sv_terrain_t ) * sv_numterrains );
	pfile = afile; // start real parsing

	int current = 0;

	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( current >= sv_numterrains )
		{
			ALERT ( at_error, "landscape parse is overrun %d > %d\n", current, sv_numterrains );
			break;
		}

		sv_terrain_t *terra = &sv_terrains[current];

		// read the landscape name
		Q_strncpy( terra->name, token, sizeof( terra->name ));

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
					COM_FileBase( token, terra->layermap.names[layerNum] );
				}

				terra->numLayers = Q_max( terra->numLayers, layerNum + 1 );
			}
			else continue; // skip all other tokens on server-side
		}

		if( LoadHeightMap( &terra->indexmap, terra->numLayers ))
		{
			if( LoadTerrainLayers( &terra->layermap, terra->numLayers ))
				terra->valid = true; // all done
		}
	}

land_getout:
	FREE_FILE( afile );
	ALERT( at_console, "server: %d landscapes parsed\n", current );
}

/*
========================
SV_FindTerrain

find the terrain description
========================
*/
sv_terrain_t *SV_FindTerrain( const char *texname )
{
	for( int i = 0; i < sv_numterrains; i++ )
	{
		if( !Q_stricmp( texname, sv_terrains[i].name ) && sv_terrains[i].valid )
			return &sv_terrains[i];
	}

	return NULL;
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
		land->terrain = SV_FindTerrain( land->landname );

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
			land->effects[i] = land->terrain->layermap.effects[i];

		land->heightmap = land->terrain->indexmap.pixels;
		land->heightmap_width = land->terrain->indexmap.width;
		land->heightmap_height = land->terrain->indexmap.height;
	}

	// update terrain bounds
	AddPointToBounds( esrf->mins, land->mins, land->maxs );
	AddPointToBounds( esrf->maxs, land->mins, land->maxs );
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
	char barename[64];
	int i;

	header = (dheader_t *)buf;
	extrahdr = (dextrahdr_t *)((byte *)buf + sizeof( dheader_t ));

	COM_FileBase( mod->name, barename );

	// process landscapes first
	SV_LoadLandscapes( barename );

	// apply materials to right get them on dedicated server
	for( i = 0; i < mod->numtextures; i++ )
	{
		texture_t *tx = mod->textures[i];

		// bad texture? 
		if( !tx || !tx->name[0] ) continue;

		matdesc_t *desc = SV_FindMaterial( tx->name );
		tx->effects = desc->effects;
	}

	// mark surfaces for world features
	for( i = 0; i < mod->numsurfaces; i++ )
	{
		msurface_t *surf = &mod->surfaces[i];
		Mod_ProcessLandscapes( surf, surf->info );
	}
}

void Mod_FreeWorld( model_t *mod )
{
	// free landscapes
	SV_FreeLandscapes();
}

/*
==================
SV_ProcessWorldData

resource management
==================
*/
void SV_ProcessWorldData( model_t *mod, qboolean create, const byte *buffer )
{
	if (create) 
		Mod_LoadWorld(mod, buffer);
	else
		Mod_FreeWorld(mod);
}
