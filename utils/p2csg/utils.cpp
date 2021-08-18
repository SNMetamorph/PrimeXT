/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "csg.h"
#include "aselib.h"
#include "imagelib.h"

/*
=============
Image_LoadTGA

expand any image to RGBA32 but keep 8-bit unchanged
=============
*/
void TEX_LoadTGA( const char *texname, mipentry_t *tex )
{
	byte	buffer[256], *buf_p;
	tga_t	targa_header;
	file_t	*f;

	// texture is loaded, wad is not used
	f = FS_Open( texname, "rb", false );
	tex->datasize = FS_FileLength( f );
	FS_Read( f, buffer, sizeof( buffer ));
	FS_Close( f );

	if( tex->datasize < sizeof( tga_t ))
		return;

	buf_p = (byte *)buffer;
	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = *(short *)buf_p;	buf_p += 2;
	targa_header.colormap_length = *(short *)buf_p;	buf_p += 2;
	targa_header.colormap_size = *buf_p;		buf_p += 1;
	targa_header.x_origin = *(short *)buf_p;	buf_p += 2;
	targa_header.y_origin = *(short *)buf_p;	buf_p += 2;
	targa_header.width = *(short *)buf_p;		buf_p += 2;
	targa_header.height = *(short *)buf_p;		buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	// we need only width & height
	tex->width = targa_header.width;
	tex->height = targa_header.height;
	tex->wadnum = -1; // wad is not used
//	Msg( "%s width %d, height %d\n", texname, tex->width, tex->height );
}

/*
============
InsertASEModel

Convert a model entity to raw geometry surfaces and insert it in the tree
============
*/
bool InsertASEModel( const char *modelName, mapent_t *mapent, short entindex, short faceinfo )
{
	side_t		modelSide;
	int		numSurfaces;
	const char	*name;
	polyset_t		*pset;
	int		numFrames;

//return false;	// temporary disabled

	// load the model
	if( !ASE_Load( modelName, false ))
		return false;

	// each ase surface will become a new bsp surface
	numSurfaces = ASE_GetNumSurfaces();

	// expand, translate, and rotate the vertexes
	// swap all the surfaces
	for( int i = 0; i < numSurfaces; i++ )
	{
		name = ASE_GetSurfaceName( i );

		pset = ASE_GetSurfaceAnimation( i, &numFrames, -1, -1, -1 );
		if( !name || !pset ) continue;

		memset( &modelSide, 0, sizeof( modelSide ));

		Q_strncpy( modelSide.name, pset->materialname, sizeof( modelSide.name ));
		modelSide.shader = ShaderInfoForShader( pset->materialname );
		modelSide.contents = CONTENTS_EMPTY;
		modelSide.faceinfo = faceinfo;

		// emit the vertexes
		for( int j = 0; j < pset->numtriangles; j++ )
		{
			poly_t	*tri = &pset->triangles[j];

			MakeBrushFor3Points( mapent, &modelSide, entindex, &tri->verts[0], &tri->verts[1], &tri->verts[2] );
		}
	}

	ASE_Free();
	return true;
}