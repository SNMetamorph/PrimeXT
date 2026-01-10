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
#include "extdll.h"
#include "util.h"
#else
#include "hud.h"
#include "utils.h"
#endif

#include "stringlib.h"
#include "com_model.h"

static matdef_t	*com_matdef; // pointer to global materials array
static int	com_matcount;
static matdef_t	*com_defaultmat;

static matdesc_t **g_matDescList;
static int *g_matDescCount;

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

	return com_defaultmat;
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
	if (!surf || !surf->texinfo)
		return NULL;

	mtexinfo_t *tx = surf->texinfo;
	int layerNum = COM_GetLayerIndexForPoint(tx->faceinfo, pointOnSurf);

	if (layerNum != -1 && layerNum < MAX_LANDSCAPE_LAYERS)
	{
		// landscape support
		return tx->faceinfo->effects[layerNum];
	}

	if (!tx->texture || !tx->texture->material) {
		return NULL;
	}

	return surf->texinfo->texture->material->effects;
}

matdef_t *COM_DefaultMatdef()
{
	return com_defaultmat;
}

static void COM_PrecacheMatdefSounds(matdef_t *mat)
{
#ifndef CLIENT_DLL
	for (size_t i = 0; mat->step_sounds[i] && i < MAX_MAT_SOUNDS; i++) {
		PRECACHE_SOUND(mat->step_sounds[i]);
	}
	for (size_t i = 0; mat->impact_sounds[i] && i < MAX_MAT_SOUNDS; i++) {
		PRECACHE_SOUND(mat->impact_sounds[i]);
	}
#endif
}

void COM_PrecacheMaterialsSounds()
{
	for (int i = 0; i < com_matcount; i++) {
		COM_PrecacheMatdefSounds(com_matdef + i);
	}
}

void COM_InitMatdef()
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

		matdef_t *mat = &com_matdef[current];

		// read the material name
		Q_strncpy( mat->name, token, sizeof( mat->name ));

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
			else ALERT( at_warning, "Unknown material token %s\n", token );
		}
	}
getout:
	FREE_FILE( afile );
	ALERT( at_aiconsole, "%d matdefs parsed\n", current );

	com_defaultmat = NULL;

	// search for default material
	for( int i = 0; i < com_matcount; i++ )
	{
		if( !Q_stricmp( "default", com_matdef[i].name ))
		{
			com_defaultmat = &com_matdef[i];
			break;
		}
	}

	// if default material was not specified by mod-maker - create internal
	if( !com_defaultmat )
	{
		ALERT( at_warning, "Default material was not specified. Create internal\n" );
		com_defaultmat = COM_CreateDefaultMatdef(); 
	}
}

matdesc_t *COM_DefaultMatdesc()
{
	static matdesc_t defmat;
	if (!defmat.name[0])
	{
		// initialize default material
		Q_strncpy( defmat.name, "*default", sizeof( defmat.name ));
		defmat.detailmap[0] = '\0';
		defmat.detailScale[0] = 10.0f;
		defmat.detailScale[1] = 10.0f;
		defmat.aberrationScale = 0.0f;
		defmat.reflectScale = 0.0f;
		defmat.refractScale = 0.0f;
		defmat.reliefScale = 0.0f;
		defmat.smoothness = DEFAULT_SMOOTHNESS; 
		defmat.effects = COM_DefaultMatdef();
	}
	return &defmat;
}

void COM_LoadMaterials(const char *path)
{
	ALERT(at_aiconsole, "COM_LoadMaterials: parsing file %s\n", path);

#if CLIENT_DLL
	char *afile = (char *)gEngfuncs.COM_LoadFile( (char *)path, 5, NULL );
#else
	char *afile = (char *)LOAD_FILE( (char *)path, NULL );
#endif

	if (!afile)
	{
		ALERT(at_error, "COM_LoadMaterials: cannot open file %s\n", path);
		return;
	}

	matdesc_t *&matlist = *g_matDescList;
	int &matcount = *g_matDescCount;
	matdesc_t *oldmaterials = matlist;
	int oldcount = matcount;
	char *pfile = afile;
	char token[256];
	int depth = 0;
	float flValue;

	// count materials
	while (pfile != NULL)
	{
		pfile = COM_ParseFile(pfile, token);
		if (!pfile) break;

		if (Q_strlen(token) > 1)
			continue;

		if (token[0] == '{')
		{
			depth++;
		}
		else if (token[0] == '}')
		{
			matcount++;
			depth--;
		}
	}

	if (depth > 0) ALERT(at_warning, "COM_LoadMaterials: %s: EOF reached without closing brace\n", path);
	if (depth < 0) ALERT(at_warning, "COM_LoadMaterials: %s: EOF reached without opening brace\n", path);

	matlist = (matdesc_t *)Mem_Alloc(sizeof(matdesc_t) * matcount);
	memcpy(matlist, oldmaterials, oldcount * sizeof(matdesc_t));
	Mem_Free(oldmaterials);
	pfile = afile; // start real parsing

	int current = oldcount; // starts from

	while (pfile != NULL)
	{
		pfile = COM_ParseFile(pfile, token);
		if (!pfile) break;

		if (current >= matcount)
		{
			ALERT(at_error, "COM_LoadMaterials: material parse is overrun %d > %d\n", current, matcount);
			break;
		}

		matdesc_t *mat = &matlist[current];

		// read the material name
		Q_strncpy(mat->name, token, sizeof(mat->name));
		COM_StripExtension(mat->name);
		mat->hash = COM_GetMaterialHash(mat);

		for (int i = 0; i < current; i++)
		{
			if (!Q_stricmp(mat->name, matlist[i].name))
			{
				ALERT(at_warning, "COM_LoadMaterials: material %s was duplicated, first definition will be overwritten\n", mat->name);
				break;
			}
		}

		// set defaults
		mat->detailScale[0] = 10.0f;
		mat->detailScale[1] = 10.0f;
		mat->smoothness = DEFAULT_SMOOTHNESS;

		// read opening brace
		pfile = COM_ParseFile(pfile, token);
		if (!pfile) break;

		if (token[0] != '{')
		{
			ALERT(at_error, "COM_LoadMaterials: found %s when expecting {\n", token);
			break;
		}

		while (pfile != NULL)
		{
			pfile = COM_ParseFile(pfile, token);
			if (!pfile)
			{
				ALERT(at_error, "COM_LoadMaterials: EOF without closing brace\n");
				goto getout;
			}

			// description end goto next material
			if (token[0] == '}')
			{
				current++;
				break;
			}
			else if (!Q_stricmp(token, "glossExp")) // for backward compatibility
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'glossExp'\n");
					goto getout;
				}

				flValue = Q_atof(token);
				flValue = bound(0.0f, flValue, 256.0f);
				mat->smoothness = sqrt(flValue / 256.0f);
			}
			else if (!Q_stricmp(token, "smoothness"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'smoothness'\n");
					goto getout;
				}

				mat->smoothness = Q_atof(token);
				mat->smoothness = bound(0.0f, mat->smoothness, 1.0f);
			}
			else if (!Q_stricmp(token, "detailScale"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'detailScale'\n");
					goto getout;
				}

				mat->detailScale = Q_atov2(token);
				mat->detailScale[0] = bound(0.01f, mat->detailScale[0], 100.0f);
				mat->detailScale[1] = bound(0.01f, mat->detailScale[0], 100.0f);
			}
			else if (!Q_stricmp(token, "detailmap"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'detailmap'\n");
					goto getout;
				}

				COM_FixSlashes(token);
				Q_strncpy(mat->detailmap, token, sizeof(mat->detailmap));
			}
			else if (!Q_stricmp(token, "diffuseMap"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'diffusemap'\n");
					goto getout;
				}

				COM_FixSlashes(token);
				Q_strncpy(mat->diffusemap, token, sizeof(mat->diffusemap));
			}
			else if (!Q_stricmp(token, "normalMap"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'normalmap'\n");
					goto getout;
				}

				COM_FixSlashes(token);
				Q_strncpy(mat->normalmap, token, sizeof(mat->normalmap));
			}
			else if (!Q_stricmp(token, "glossMap"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'glossmap'\n");
					goto getout;
				}

				COM_FixSlashes(token);
				Q_strncpy(mat->glossmap, token, sizeof(mat->glossmap));
			}
			else if (!Q_stricmp(token, "AberrationScale"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'AberrationScale'\n");
					goto getout;
				}

				mat->aberrationScale = Q_atof(token) * 0.1f;
				mat->aberrationScale = bound(0.0f, mat->aberrationScale, 0.1f);
			}
			else if (!Q_stricmp(token, "ReflectScale"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'ReflectScale'\n");
					goto getout;
				}

				mat->reflectScale = Q_atof(token);
				mat->reflectScale = bound(0.0f, mat->reflectScale, 1.0f);
			}
			else if (!Q_stricmp(token, "RefractScale"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'RefractScale'\n");
					goto getout;
				}

				mat->refractScale = Q_atof(token);
				mat->refractScale = bound(0.0f, mat->refractScale, 1.0f);
			}
			else if (!Q_stricmp(token, "SwayHeight"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'SwayHeight'\n");
					goto getout;
				}

				mat->swayHeight = Q_atoi(token);
			}
			else if (!Q_stricmp(token, "ReliefScale"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'ReliefScale'\n");
					goto getout;
				}

				mat->reliefScale = Q_atof(token);
				mat->reliefScale = bound(0.0f, mat->reliefScale, 1.0f);
			}
			else if (!Q_stricmp(token, "material"))
			{
				pfile = COM_ParseFile(pfile, token);
				if (!pfile)
				{
					ALERT(at_error, "COM_LoadMaterials: hit EOF while parsing 'material'\n");
					goto getout;
				}

				mat->effects = COM_FindMatdef(token);
			}
			else ALERT(at_warning, "COM_LoadMaterials: unknown material token %s\n", token);
		}

		// apply default values
		if (!mat->effects)
			mat->effects = COM_DefaultMatdef();
	}
getout:

#ifdef CLIENT_DLL
	gEngfuncs.COM_FreeFile(afile);
#else
	FREE_FILE(afile);
#endif
	ALERT(at_aiconsole, "COM_LoadMaterials: %d materials parsed\n", current);
}

matdesc_t *COM_FindMaterial(const char *texName)
{
	matdesc_t *&matlist = *g_matDescList;
	int &matcount = *g_matDescCount;

	for (int i = 0; i < matcount; i++)
	{
		if (!Q_stricmp(texName, matlist[i].name))
			return &matlist[i];
	}
	return COM_DefaultMatdesc();
}

matdesc_t *COM_FindMaterial(uint32_t matHash)
{
	matdesc_t *&matlist = *g_matDescList;
	int &matcount = *g_matDescCount;

	for (int i = 0; i < matcount; i++)
	{
		if (matHash == matlist[i].hash) {
			return &matlist[i];
		}
	}
	return COM_DefaultMatdesc();
}

void COM_InitMaterials(matdesc_t *&matlist, int &matcount)
{
	int count = 0;
#ifdef CLIENT_DLL
	char **filenames = FS_SEARCH("scripts/*.mat", &count, 0);
#else
	char **filenames = GET_FILES_LIST("scripts/*.mat", &count, 0);
#endif

	g_matDescList = &matlist;
	g_matDescCount = &matcount;

	// sequentially load materials
	matcount = 0;
	for (int i = 0; i < count; i++) {
		COM_LoadMaterials(filenames[i]);
	}
}

uint32_t COM_GetMaterialHash(matdesc_t *mat)
{
	if (mat)
	{
		uint32_t hash;
		CRC32_Init(&hash);
		CRC32_ProcessBuffer(&hash, mat->name, strlen(mat->name));
		hash = CRC32_Final(hash);
		return hash;
	}
	return 0;
}
