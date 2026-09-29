/*
model_source.cpp - common interface for reference mesh/animation sources
Copyright (C) 2017 Uncle Mike
Copyright (C) 2026 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "port.h"
#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "file_system.h"
#include "model_source.h"
#include "smd_source.h"
#include "gltf_source.h"

bool IsEnd( char const *pLine )
{
	if( !Q_strncmp( "end", pLine, 3 )) 
		return true;
	return ( pLine[3] == '\0' ) || ( pLine[3] == '\n' );
}

int LookupTexture( const char *texturename )
{
	int	i;

	for( i = 0; i < g_numtextures; i++ )
	{
		if( !Q_stricmp( g_texture[i].name, texturename ))
			return i;
	}

	Q_strncpy( g_texture[i].name, texturename, sizeof( g_texture[0].name ));

	// XDM: allow such names as "tex_chrome_bright" - chrome and full brightness effects
	if( Q_stristr( texturename, "chrome" ) != NULL )
		g_texture[i].flags |= STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME;
	if( Q_stristr( texturename, "bright" ) != NULL )
		g_texture[i].flags |= STUDIO_NF_FULLBRIGHT;
	g_numtextures++;

	return i;
}

s_mesh_t *LookupMesh( s_model_t *pmodel, char *texturename )
{
	int	i, j;

	j = LookupTexture( texturename );

	for( i = 0; i < pmodel->nummesh; i++ )
	{
		if( pmodel->pmesh[i]->skinref == j )
			return pmodel->pmesh[i];
	}
	
	if( i >= MAXSTUDIOMESHES )
		COM_FatalError( "too many meshes in model: \"%s\"\n", pmodel->name );

	pmodel->nummesh = i + 1;
	pmodel->pmesh[i] = (s_mesh_t *)Mem_Alloc( sizeof( s_mesh_t ));
	pmodel->pmesh[i]->skinref = j;

	return pmodel->pmesh[i];
}

s_trianglevert_t *LookupTriangle( s_mesh_t *pmesh, int index )
{
	if( index >= MAXSTUDIOTRIANGLES )
		COM_FatalError( "max studio triangles exceeds 65536\n" );

	if( index >= pmesh->alloctris )
	{
		int start = pmesh->alloctris;
		pmesh->alloctris = index + 256;

		if( pmesh->triangle )
		{
			pmesh->triangle = (s_trianglevert_t (*)[3])Mem_Realloc( pmesh->triangle, pmesh->alloctris * sizeof( *pmesh->triangle ));
			memset( &pmesh->triangle[start], 0, ( pmesh->alloctris - start ) * sizeof( *pmesh->triangle ));
		} 
		else
		{
			pmesh->triangle = (s_trianglevert_t (*)[3])Mem_Alloc( pmesh->alloctris * sizeof( *pmesh->triangle ));
		}
	}

	return pmesh->triangle[index];
}

//-----------------------------------------------------------------------------
// Purpose: build compound boneToPose matrices from the reference skeleton
//-----------------------------------------------------------------------------
void Build_Reference( s_model_t *pmodel )
{
	for( int i = 0; i < pmodel->numbones; i++ )
	{
		matrix3x4	bonematrix = matrix3x4( pmodel->skeleton[i].pos, pmodel->skeleton[i].rot );
		int parent = pmodel->localBone[i].parent;

		if( parent == -1 )
		{
			// scale the done pos.
			// calc rotational matrices
			pmodel->boneToPose[i] = bonematrix;

		}
		else
		{
			// calc compound rotational matrices
			pmodel->boneToPose[i] = pmodel->boneToPose[parent].ConcatTransforms( bonematrix );
		}
	}
}

CSourcePtr CreateStudioSource( const char *name )
{
	const char *ext = COM_FileExtension( name );

	if( !Q_stricmp( ext, "gltf" ) || !Q_stricmp( ext, "glb" ))
		return std::make_unique<CGltfSource>( name );

	return std::make_unique<CSmdSource>();
}
