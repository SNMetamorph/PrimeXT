/*
material.cpp - material description for both client and server
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "material.h"

#ifndef CLIENT_DLL
#include"extdll.h"
#include "util.h"
#else
#include "hud.h"
#include "utils.h"
#endif

#include "stringlib.h"
#include "com_model.h"

static matdef_t	*com_matdef; // pointer to global materials array
static int	com_matcount;
static matdef_t	*com_defmat;

char *_COM_CopyString( const char *s, const char *file, const int line )
{
	if( !s ) return NULL;

	char *b = (char *)_Mem_Alloc( Q_strlen( s ) + 1, file, line );
	Q_strcpy( b, s );

	return b;
}

matdef_t *COM_CreateDefaultMatdef( void )
{
	static matdef_t mat;

	if( mat.name[0] )
		return &mat; // already created

	Q_strcpy( mat.name, "default" );
	mat.detailName[0] = '\0';

	mat.impact_decal = COM_CopyString( "shot" ); // default Paranoia decal
	mat.impact_sounds[0] = COM_CopyString( "debris/concrete1.wav" );
	mat.impact_sounds[1] = COM_CopyString( "debris/concrete2.wav" );
	mat.impact_sounds[2] = COM_CopyString( "debris/concrete3.wav" );
	mat.impact_sounds[3] = COM_CopyString( "debris/concrete4.wav" );
	mat.step_sounds[0] = COM_CopyString( "player/pl_beton1.wav" );
	mat.step_sounds[1] = COM_CopyString( "player/pl_beton2.wav" );
	mat.step_sounds[2] = COM_CopyString( "player/pl_beton3.wav" );
	mat.step_sounds[3] = COM_CopyString( "player/pl_beton4.wav" );

	return &mat;
}

/*
==================
COM_FindMatdef

This function never failed
==================
*/
matdef_t *COM_FindMatdef( const char *name )
{
	for( int i = 0; i < com_matcount; i++ )
	{
		if( !Q_stricmp( name, com_matdef[i].name ))
			return &com_matdef[i];
	}

	ALERT( at_error, "material specific '%s' not found. forcing to defualt\n", name );

	return com_defmat;
}

/*
==================
GetLayerIndexForPoint

this function came from q3map2
==================
*/
static char COM_GetLayerIndexForPoint( mfaceinfo_t *land, const Vector &point )
{
	Vector	size;

	if( !land || !land->heightmap )
		return -1;

	for( int i = 0; i < 3; i++ )
		size[i] = ( land->maxs[i] - land->mins[i] );

	float s = ( point[0] - land->mins[0] ) / size[0];
	float t = ( land->maxs[1] - point[1] ) / size[1];

	int x = s * land->heightmap_width;
	int y = t * land->heightmap_height;

	x = bound( 0, x, ( land->heightmap_width - 1 ));
	y = bound( 0, y, ( land->heightmap_height - 1 ));

	return land->heightmap[y * land->heightmap_width + x];
}

/*
=================
COM_MatDefFromSurface

safe version to get matdef_t
=================
*/
matdef_t *COM_MatDefFromSurface(msurface_t *surf, const Vector &pointOnSurf)
{
	if( !surf || !surf->texinfo )
		return NULL;

	mtexinfo_t *tx = surf->texinfo;
	int layerNum = COM_GetLayerIndexForPoint( tx->faceinfo, pointOnSurf);

	if( layerNum != -1 && layerNum < MAX_LANDSCAPE_LAYERS )
	{
		// landscape support
		return tx->faceinfo->effects[layerNum];
	}

	if( !tx->texture )
		return NULL;

	return surf->texinfo->texture->effects;	// epic chain!
}

matdef_t *COM_DefaultMatdef( void )
{
	return com_defmat;
}

void COM_InitMatdef( void )
{
	ALERT( at_aiconsole, "loading materials.def\n" );

	char *afile = (char *)LOAD_FILE( "scripts/materials.def", NULL );

	if( !afile )
	{
		ALERT( at_error, "Cannot open file \"scripts/materials.def\"\n" );
		return;
	}

	char *pfile = afile;
	char token[256];
	int depth = 0;
	com_matcount = 0;

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
			com_matcount++;
			depth--;
		}
	}

	if( depth > 0 ) ALERT( at_warning, "materials.def: EOF reached without closing brace\n" );
	if( depth < 0 ) ALERT( at_warning, "materials.def: EOF reached without opening brace\n" );

	com_matdef = (matdef_t *)Mem_Alloc( sizeof( matdef_t ) * com_matcount );
	pfile = afile; // start real parsing

	int current = 0;

	while( pfile != NULL )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( current >= com_matcount )
		{
			ALERT ( at_error, "material parse is overrun %d > %d\n", current, com_matcount );
			break;
		}

		matdef_t	*mat = &com_matdef[current];

		// read the material name
		Q_strncpy( mat->name, token, sizeof( mat->name ));

		// detail scale default values
		mat->detailScale[0] = 10.0f;
		mat->detailScale[1] = 10.0f;

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
				goto getout;
			}

			// description end goto next material
			if( token[0] == '}' )
			{
				current++;
				break;
			}
			else if( !Q_stricmp( token, "impact_decal" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'impact_decal'\n" );
					goto getout;
				}

				mat->impact_decal = COM_CopyString( token );
			}
			else if( !Q_stricmp( token, "impact_parts" ))
			{
				while(( pfile = COM_ParseLine( pfile, token )) != NULL )
				{
					if( !Q_strlen( token )) break; // end of line

					int i;
					// find the free sound slot
					for( i = 0; mat->impact_parts[i] != NULL && i < MAX_MAT_SOUNDS; i++ );

					if( i < MAX_MAT_SOUNDS )
					{
						mat->impact_parts[i] = COM_CopyString( token );
					}
				}
			}
			else if( !Q_stricmp( token, "impact_sound" ))
			{
				while(( pfile = COM_ParseLine( pfile, token )) != NULL )
				{
					if( !Q_strlen( token )) break; // end of line

					int i;
					// find the free sound slot
					for( i = 0; mat->impact_sounds[i] != NULL && i < MAX_MAT_SOUNDS; i++ );

					if( i < MAX_MAT_SOUNDS )
					{
						mat->impact_sounds[i] = COM_CopyString( token );
					}
				}
			}
			else if( !Q_stricmp( token, "step_sound" ))
			{
				while(( pfile = COM_ParseLine( pfile, token )) != NULL )
				{
					if( !Q_strlen( token )) break; // end of line

					int i;
					// find the free sound slot
					for( i = 0; mat->step_sounds[i] != NULL && i < MAX_MAT_SOUNDS; i++ );

					if( i < MAX_MAT_SOUNDS )
					{
						mat->step_sounds[i] = COM_CopyString( token );
					}
				}
			}
			else if( !Q_stricmp( token, "detailScale" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'detailScale'\n" );
					goto getout;
				}

				mat->detailScale = Q_atov2(token);
				mat->detailScale[0] = bound( 0.01f, mat->detailScale[0], 100.0f );
				mat->detailScale[1] = bound( 0.01f, mat->detailScale[0], 100.0f );
			}
			else if( !Q_stricmp( token, "detailmap" ))
			{
				pfile = COM_ParseFile( pfile, token );
				if( !pfile )
				{
					ALERT( at_error, "hit EOF while parsing 'detailmap'\n" );
					goto getout;
				}

				COM_FixSlashes( token );
				Q_strncpy( mat->detailName, token, sizeof( mat->detailName ));
			}
			else ALERT( at_warning, "Unknown material token %s\n", token );
		}
	}
getout:
	FREE_FILE( afile );
	ALERT( at_aiconsole, "%d matdefs parsed\n", current );

	com_defmat = NULL;

	// search for default material
	for( int i = 0; i < com_matcount; i++ )
	{
		if( !Q_stricmp( "default", com_matdef[i].name ))
		{
			com_defmat = &com_matdef[i];
			break;
		}
	}

	// if default material was not specified by mod-maker - create internal
	if( !com_defmat )
	{
		ALERT( at_warning, "Default material was not specified. Create internal\n" );
		com_defmat = COM_CreateDefaultMatdef(); 
	}
}