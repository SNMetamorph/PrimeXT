/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "scriplib.h"
#include "filesystem.h"
#include "stringlib.h"
#include <io.h>

//=============================================================================
bool		g_found_extradata = false;

int		g_nummodels;
dmodel_t		g_dmodels[MAX_MAP_MODELS];

int		g_visdatasize;
byte		g_dvisdata[MAX_MAP_VISIBILITY];

int		g_vislightdatasize;
byte		*g_dvislightdata;

int		g_lightdatasize;
byte		*g_dlightdata;

int		g_deluxdatasize;
byte		*g_ddeluxdata;

int		g_shadowdatasize;
byte		*g_dshadowdata;

int		g_texdatasize;
byte		*g_dtexdata; // (dmiptexlump_t)

int		g_entdatasize;
char		g_dentdata[MAX_MAP_ENTSTRING];

int		g_numleafs;
int		g_numvisleafs;
dleaf_t		g_dleafs[MAX_MAP_LEAFS];

int		g_numplanes;
dplane_t		g_dplanes[MAX_INTERNAL_MAP_PLANES];

int		g_numvertexes;
dvertex_t		g_dvertexes[MAX_MAP_VERTS];

int		g_numnodes;
dnode_t		g_dnodes[MAX_MAP_NODES];

int		g_numtexinfo;
dtexinfo_t	g_texinfo[MAX_INTERNAL_MAP_TEXINFO];

int		g_numfaces;
dface_t		g_dfaces[MAX_MAP_FACES];

int		g_numclipnodes;
dclipnode_t	g_dclipnodes[MAX_MAP_CLIPNODES];
dclipnode32_t	g_dclipnodes32[MAX_MAP_CLIPNODES32];

int		g_numedges;
dedge_t		g_dedges[MAX_MAP_EDGES];

int		g_nummarksurfaces;
dmarkface_t	g_dmarksurfaces[MAX_MAP_MARKSURFACES];

int		g_numsurfedges;
dsurfedge_t	g_dsurfedges[MAX_MAP_SURFEDGES];

int		g_numfaceinfo;
dfaceinfo_t	g_dfaceinfo[MAX_MAP_FACEINFO];

int		g_numcubemaps;
dcubemap_t	g_dcubemaps[MAX_MAP_CUBEMAPS];

int		g_numleaflights;
dleafsample_t	g_dleaflights[MAX_MAP_LEAFLIGHTS];

int		g_numworldlights;
dworldlight_t	g_dworldlights[MAX_MAP_WORLDLIGHTS];

int		g_vlightdatasize;
byte		*g_dvlightdata; // (dvlightlump_t)

int		g_flightdatasize;
byte		*g_dflightdata; // (dflightlump_t)

int		g_normaldatasize;
byte		*g_dnormaldata; // (dnormallump_t)

int		g_numentities;
entity_t		g_entities[MAX_MAP_ENTITIES];

dheader_t		*header, outheader;
long		wadfile;

char		g_wadpath[1024];	// path to wads may be empty

// can be overrided from hlcsg
vec3_t g_hull_size[MAX_MAP_HULLS][2] = 
{
{ // 0x0x0
{  0,   0,  0  },	{  0,  0,  0 }
},
{ // 32x32x72
{-16, -16, -36 },	{ 16, 16, 36 }
},
{ // 64x64x64
{-32, -32, -32 },	{ 32, 32, 32 }
},
{ // 32x32x36
{-16, -16, -18 },	{ 16, 16, 18 }
}
};

/*
============
CheckHullFile
============
*/
void CheckHullFile( const char *filename )
{
	vec3_t	new_hulls[MAX_MAP_HULLS][2];
	bool	read_error = false;
	char	scan[128];

	if( !filename[0] ) return;

	// open up hull file
	FILE *f = fopen( filename, "r" );

	if( !f )
	{
		MsgDev( D_WARN, "couldn't open hullfile %s, using default hulls", filename );
		return; 
	}
	else
	{
		MsgDev( D_INFO, "[Reading hulls from '%s']\n", filename );
	}

	for( int i = 0; i < MAX_MAP_HULLS; i++ )
	{
		float	x1, y1, z1, x2, y2, z2;
		vec3_t	mins, maxs;
		int	argCnt;

		if( !fgets( scan, sizeof( scan ), f ))
		{
			MsgDev( D_WARN, "parsing %s, couln't read hull line %i, using default hulls\n", filename, i );
			read_error = true;
			break;
		}

		argCnt = sscanf( scan, "( %f %f %f ) ( %f %f %f ) ", &x1, &y1, &z1, &x2, &y2, &z2 );

		if( argCnt != 6 )
		{
			MsgDev( D_ERROR, "parsing %s, expeciting '( x y z ) ( x y z )' using default hulls\n", filename );
			read_error = true;
			break;
		}
		else
		{
			VectorSet( mins, x1, y1, z1 );
			VectorSet( maxs, x2, y2, z2 );
		}

		VectorCopy( mins, new_hulls[i][0] );
		VectorCopy( maxs, new_hulls[i][1] );
	}

	if( read_error )
	{
		MsgDev( D_REPORT, "error parsing %s, using default hulls\n", filename );
	}
	else
	{
		memcpy( g_hull_size, new_hulls, 2 * MAX_MAP_HULLS * sizeof( vec3_t ) );
	}

	fclose( f );
}

/*
===============
CompressVis

===============
*/
int CompressVis( const byte *src, const uint src_length, byte *dest, uint dest_length )
{
	uint	j, current_length = 0;
	byte	*dest_p = dest;
	int	rep;

	for( j = 0; j < src_length; j++ )
	{
		if( ++current_length > dest_length )
			COM_FatalError( "CompressVis: compressed vismap overflow\n" );

		*dest_p = src[j];
		dest_p++;
		if( src[j] ) continue;

		for( j++, rep = 1; j < src_length; j++ )
		{
			if( src[j] || rep == 255 )
				break;
			else rep++;
		}

		if( ++current_length > dest_length )
			COM_FatalError( "CompressVis: compressed vismap overflow\n" );

		*dest_p++ = rep;
		j--;
	}

	return dest_p - dest;
}

/*
===================
DecompressVis
===================
*/
void DecompressVis( byte *in, byte *decompressed )
{
	byte	*out;
	int	c, row;

	row = (g_numvisleafs + 7) >> 3;
	out = decompressed;

	do
	{
		if( *in )
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		if( !c ) COM_FatalError( "DecompressVis: 0 repeat\n" );

		in += 2;
		while( c )
		{
			*out++ = 0;
			c--;
		}
	} while( out - decompressed < row );
}

//=============================================================================

int CopyLump( int lump, void *dest, int size )
{
	int length = header->lumps[lump].filelen;
	int ofs = header->lumps[lump].fileofs;

	if( length % size )
		COM_FatalError( "LoadBSPFile: odd lump size\n" );

	// alloc matched size
	if( lump == LUMP_TEXTURES )
		dest = g_dtexdata = (byte *)Mem_Alloc( length );
	if( lump == LUMP_LIGHTING )
		dest = g_dlightdata = (byte *)Mem_Alloc( length );
	memcpy( dest, (byte *)header + ofs, length );

	return length / size;
}

static int CopyExtraLump( int lump, void *dest, int size, const dheader_t *header )
{
	dextrahdr_t *extrahdr = (dextrahdr_t *)((byte *)header + sizeof( dheader_t ));

	int length = extrahdr->lumps[lump].filelen;
	int ofs = extrahdr->lumps[lump].fileofs;

	if( length % size )
		COM_FatalError( "LoadBSPFile: odd lump size\n" );

	if( lump == LUMP_LIGHTVECS )
		dest = g_ddeluxdata = (byte *)Mem_Alloc( length );
	if( lump == LUMP_SHADOWMAP )
		dest = g_dshadowdata = (byte *)Mem_Alloc( length );
	if( lump == LUMP_VERTEX_LIGHT )
		dest = g_dvlightdata = (byte *)Mem_Alloc( length );
	if( lump == LUMP_SURFACE_LIGHT )
		dest = g_dflightdata = (byte *)Mem_Alloc( length );
	if( lump == LUMP_VERTNORMALS )
		dest = g_dnormaldata = (byte *)Mem_Alloc( length );
	if( lump == LUMP_VISLIGHTDATA )
		dest = g_dvislightdata = (byte *)Mem_Alloc( length );
	memcpy( dest, (byte *)header + ofs, length );

	return length / size;
}

static int CopyLumpClipnodes( int lump )
{
	int length = header->lumps[lump].filelen;
	int ofs = header->lumps[lump].fileofs;
	bool clipnodes_compatible = true;
	int size;

	if(( length % sizeof( dclipnode_t )) || ( length / sizeof( dclipnode_t )) >= MAX_MAP_CLIPNODES )
		clipnodes_compatible = false;

	if( clipnodes_compatible )
	{
		// compatible 16-bit clipnodes
		size = sizeof( dclipnode_t );
		memcpy( g_dclipnodes, (byte *)header + ofs, length );

		// share clipnodes with 32-bit array
		for( int i = 0; i < (length / size); i++ )
		{
			g_dclipnodes32[i].children[0] = g_dclipnodes[i].children[0];
			g_dclipnodes32[i].children[1] = g_dclipnodes[i].children[1];
			g_dclipnodes32[i].planenum = g_dclipnodes[i].planenum;
		}
	}
	else
	{
		// extended 32-bit clipnodes
		size = sizeof( dclipnode32_t );
		memcpy( g_dclipnodes32, (byte *)header + ofs, length );
	}

	return length / size;
}

/*
=============
LoadBSPFile
=============
*/
void LoadBSPFile( const char *filename )
{
	size_t	filesize;

	// load the file header
	header = (dheader_t *)COM_LoadFile( filename, &filesize );
	if( !header ) COM_FatalError( "couldn't load: %s\n", filename );

	if( header->version != BSPVERSION )
		COM_FatalError( "%s is version %i, not %i\n", filename, header->version, BSPVERSION );
	MsgDev( D_REPORT, "loading %s\n", filename );

	g_nummodels = CopyLump( LUMP_MODELS, g_dmodels, sizeof( dmodel_t ));
	g_numvertexes = CopyLump( LUMP_VERTEXES, g_dvertexes, sizeof( dvertex_t ));
	g_numplanes = CopyLump( LUMP_PLANES, g_dplanes, sizeof( dplane_t ));
	g_numleafs = CopyLump( LUMP_LEAFS, g_dleafs, sizeof( dleaf_t ));
	g_numnodes = CopyLump( LUMP_NODES, g_dnodes, sizeof( dnode_t ));
	g_numtexinfo = CopyLump( LUMP_TEXINFO, g_texinfo, sizeof( dtexinfo_t ));
	g_numfaces = CopyLump( LUMP_FACES, g_dfaces, sizeof( dface_t ));
	g_nummarksurfaces = CopyLump( LUMP_MARKSURFACES, g_dmarksurfaces, sizeof( dmarkface_t ));
	g_numsurfedges = CopyLump( LUMP_SURFEDGES, g_dsurfedges, sizeof( dsurfedge_t ));
	g_numedges = CopyLump( LUMP_EDGES, g_dedges, sizeof( dedge_t ));
	g_numclipnodes = CopyLumpClipnodes( LUMP_CLIPNODES );
	g_numvisleafs = g_dmodels[0].visleafs;

	g_texdatasize = CopyLump( LUMP_TEXTURES, g_dtexdata, 1 );
	g_visdatasize = CopyLump( LUMP_VISIBILITY, g_dvisdata, 1 );
	g_lightdatasize = CopyLump( LUMP_LIGHTING, g_dlightdata, 1 );
	g_entdatasize = CopyLump( LUMP_ENTITIES, g_dentdata, 1 );

	dextrahdr_t *extrahdr = (dextrahdr_t *)((byte *)header + sizeof( dheader_t ));

	if( extrahdr->id == IDEXTRAHEADER )
	{
		if( extrahdr->version != EXTRA_VERSION )
			COM_FatalError( "BSP is extra version %i, not %i", extrahdr->version, EXTRA_VERSION );

		g_found_extradata = true;

		// g-cont. copy the extra lumps
		g_normaldatasize = CopyExtraLump( LUMP_VERTNORMALS, g_dnormaldata, 1, header );
		g_deluxdatasize = CopyExtraLump( LUMP_LIGHTVECS, g_ddeluxdata, 1, header );
		g_numcubemaps = CopyExtraLump( LUMP_CUBEMAPS, g_dcubemaps, sizeof( dcubemap_t ), header );
		g_numfaceinfo = CopyExtraLump( LUMP_FACEINFO, g_dfaceinfo, sizeof( dfaceinfo_t ), header );
		g_numworldlights = CopyExtraLump( LUMP_WORLDLIGHTS, g_dworldlights, sizeof( dworldlight_t ), header );
		g_numleaflights = CopyExtraLump( LUMP_LEAF_LIGHTING, g_dleaflights, sizeof( dleafsample_t ), header );
		g_shadowdatasize = CopyExtraLump( LUMP_SHADOWMAP, g_dshadowdata, 1, header );
		g_vlightdatasize = CopyExtraLump( LUMP_VERTEX_LIGHT, g_dvlightdata, 1, header );
		g_flightdatasize = CopyExtraLump( LUMP_SURFACE_LIGHT, g_dflightdata, 1, header );
		g_vislightdatasize = CopyExtraLump( LUMP_VISLIGHTDATA, g_dvislightdata, 1, header );
	}

	Mem_Free( header, C_FILESYSTEM ); // everything has been copied out
}

//============================================================================

void AddLump( int lumpnum, void *data, int len )
{
	dlump_t *lump = &header->lumps[lumpnum];
	lump->fileofs = tell( wadfile );
	lump->filelen = len;
	SafeWrite( wadfile, data, (len + 3) & ~3 );
}

static void AddExtraLump( int lumpnum, void *data, int len, dextrahdr_t *header )
{
	dlump_t* lump = &header->lumps[lumpnum];
	lump->fileofs = tell( wadfile );
	lump->filelen = len;
	SafeWrite( wadfile, data, (len + 3) & ~3 );
}

void AddLumpClipnodes( int lumpnum )
{
	dlump_t *lump = &header->lumps[lumpnum];
	lump->fileofs = tell( wadfile );

	if( g_numclipnodes < MAX_MAP_CLIPNODES )
	{
		// copy clipnodes into 16-bit array
		for( int i = 0; i < g_numclipnodes; i++ )
		{
			g_dclipnodes[i].children[0] = (short)g_dclipnodes32[i].children[0];
			g_dclipnodes[i].children[1] = (short)g_dclipnodes32[i].children[1];
			g_dclipnodes[i].planenum = g_dclipnodes32[i].planenum;
		}

		lump->filelen = g_numclipnodes * sizeof( dclipnode_t );
		SafeWrite( wadfile, g_dclipnodes, (lump->filelen + 3) & ~3 );
	}
	else
	{
		// copy clipnodes into 32-bit array
		lump->filelen = g_numclipnodes * sizeof( dclipnode32_t );
		SafeWrite( wadfile, g_dclipnodes32, (lump->filelen + 3) & ~3 );
	}
}

/*
=============
WriteBSPFile

Swaps the bsp file in place, so it should not be referenced again
=============
*/
void WriteBSPFile( const char *filename )
{		
	dextrahdr_t	outextrahdr;
	dextrahdr_t	*extrahdr;

	header = &outheader;
	memset( header, 0, sizeof( dheader_t ));
	extrahdr = &outextrahdr;
	memset( extrahdr, 0, sizeof( dextrahdr_t ));
	
	header->version = BSPVERSION;
	extrahdr->id = IDEXTRAHEADER;
	extrahdr->version = EXTRA_VERSION;
	
	wadfile = SafeOpenWrite( filename );
	SafeWrite( wadfile, header, sizeof( dheader_t ));		// overwritten later
	SafeWrite( wadfile, extrahdr, sizeof( dextrahdr_t ));	// overwritten later

	AddLump( LUMP_PLANES, g_dplanes, g_numplanes * sizeof( dplane_t ));
	AddLump( LUMP_LEAFS, g_dleafs, g_numleafs * sizeof( dleaf_t ));
	AddLump( LUMP_VERTEXES, g_dvertexes, g_numvertexes * sizeof( dvertex_t ));
	AddLump( LUMP_NODES, g_dnodes, g_numnodes * sizeof( dnode_t ));
	AddLump( LUMP_TEXINFO, g_texinfo, g_numtexinfo * sizeof( dtexinfo_t ));
	AddLump( LUMP_FACES, g_dfaces, g_numfaces * sizeof( dface_t ));
	AddLump( LUMP_MARKSURFACES, g_dmarksurfaces, g_nummarksurfaces * sizeof( dmarkface_t ));
	AddLump( LUMP_SURFEDGES, g_dsurfedges, g_numsurfedges * sizeof( dsurfedge_t ));
	AddLump( LUMP_EDGES, g_dedges, g_numedges * sizeof( dedge_t ));
	AddLump( LUMP_MODELS, g_dmodels, g_nummodels * sizeof( dmodel_t ));
	AddLumpClipnodes( LUMP_CLIPNODES );

	AddLump( LUMP_LIGHTING, g_dlightdata, g_lightdatasize );
	AddLump( LUMP_VISIBILITY, g_dvisdata, g_visdatasize );
	AddLump( LUMP_ENTITIES, g_dentdata, g_entdatasize );
	AddLump( LUMP_TEXTURES, g_dtexdata, g_texdatasize );

	AddExtraLump( LUMP_VERTNORMALS,  g_dnormaldata,   g_normaldatasize,                           extrahdr );
	AddExtraLump( LUMP_LIGHTVECS,    g_ddeluxdata,    g_deluxdatasize,                            extrahdr );
	AddExtraLump( LUMP_CUBEMAPS,     g_dcubemaps,     g_numcubemaps * sizeof( dcubemap_t ),       extrahdr );
	AddExtraLump( LUMP_FACEINFO,     g_dfaceinfo,     g_numfaceinfo * sizeof( dfaceinfo_t ),      extrahdr );
	AddExtraLump( LUMP_WORLDLIGHTS,  g_dworldlights,  g_numworldlights * sizeof( dworldlight_t ), extrahdr );
	AddExtraLump( LUMP_SHADOWMAP,    g_dshadowdata,   g_shadowdatasize,                           extrahdr );
	AddExtraLump( LUMP_LEAF_LIGHTING,g_dleaflights,   g_numleaflights * sizeof( dleafsample_t ),  extrahdr );
	AddExtraLump( LUMP_VERTEX_LIGHT, g_dvlightdata,   g_vlightdatasize,                           extrahdr );
	AddExtraLump( LUMP_SURFACE_LIGHT,g_dflightdata,   g_flightdatasize,                           extrahdr );
	AddExtraLump( LUMP_VISLIGHTDATA, g_dvislightdata, g_vislightdatasize,                         extrahdr );

	lseek( wadfile, 0, SEEK_SET );
	SafeWrite( wadfile, header, sizeof( dheader_t ));
	SafeWrite( wadfile, extrahdr, sizeof( dextrahdr_t ));

	close( wadfile );	

	if( g_dvislightdata ) Mem_Free( g_dvislightdata );
	if( g_dlightdata ) Mem_Free( g_dlightdata );
	if( g_ddeluxdata ) Mem_Free( g_ddeluxdata );
	if( g_dshadowdata ) Mem_Free( g_dshadowdata );
	if( g_dvlightdata ) Mem_Free( g_dvlightdata );
	if( g_dflightdata ) Mem_Free( g_dflightdata );
	if( g_dnormaldata ) Mem_Free( g_dnormaldata );
	if( g_dtexdata ) Mem_Free( g_dtexdata );

	g_dvislightdata = NULL;
	g_dlightdata = NULL;
	g_ddeluxdata = NULL;
	g_dshadowdata = NULL;
	g_dvlightdata = NULL;
	g_dflightdata = NULL;
	g_dnormaldata = NULL;
	g_dtexdata = NULL;
}

//============================================================================

#define ENTRIES(a)		(sizeof(a)/sizeof(*(a)))
#define ENTRYSIZE(a)	(sizeof(*(a)))

// =====================================================================================
//  ArrayUsage
//      blah
// =====================================================================================
static int ArrayUsage( const char *szItem, const int items, const int maxitems, const int itemsize )
{
	float	percentage = maxitems ? items * 100.0 / maxitems : 0.0;

	Msg("%-13s %7i/%-7i %8i/%-8i (%4.1f%%)\n", szItem, items, maxitems, items * itemsize, maxitems * itemsize, percentage );

	return items * itemsize;
}

// =====================================================================================
//  GlobUsage
//      pritn out global ussage line in chart
// =====================================================================================
static int GlobUsage( const char *szItem, const int itemstorage, const int maxstorage )
{
	float	percentage = maxstorage ? itemstorage * 100.0 / maxstorage : 0.0;

	Msg("%-13s    [variable]   %8i/%-8i (%4.1f%%)\n", szItem, itemstorage, maxstorage, percentage );

	return itemstorage;
}

/*
=============
PrintBSPFileSizes

Dumps info about current file
=============
*/
void PrintBSPFileSizes (void)
{
	int	numtextures = g_texdatasize ? ((dmiptexlump_t*)g_dtexdata)->nummiptex : 0;
	int	totalmemory = 0;

	Msg( "\n");
	Msg( "Object names  Objects/Maxobjs  Memory / Maxmem  Fullness\n" );
	Msg( "------------  ---------------  ---------------  --------\n" );

	totalmemory += ArrayUsage( "models",		g_nummodels,	ENTRIES( g_dmodels ),	ENTRYSIZE( g_dmodels ));
	totalmemory += ArrayUsage( "planes",		g_numplanes,	ENTRIES( g_dplanes ),	ENTRYSIZE( g_dplanes ));
	totalmemory += ArrayUsage( "vertexes",		g_numvertexes,	ENTRIES( g_dvertexes ),	ENTRYSIZE( g_dvertexes ));
	totalmemory += ArrayUsage( "nodes",		g_numnodes,	ENTRIES( g_dnodes ),	ENTRYSIZE( g_dnodes ));
	totalmemory += ArrayUsage( "texinfos",		g_numtexinfo,	ENTRIES( g_texinfo ),	ENTRYSIZE( g_texinfo ));
	totalmemory += ArrayUsage( "faces",		g_numfaces,	ENTRIES( g_dfaces ),	ENTRYSIZE( g_dfaces ));
	totalmemory += ArrayUsage( "clipnodes",		g_numclipnodes,	ENTRIES( g_dclipnodes ),	ENTRYSIZE( g_dclipnodes ));
	totalmemory += ArrayUsage( "leaves",		g_numleafs,	ENTRIES( g_dleafs ),	ENTRYSIZE( g_dleafs ));
	totalmemory += ArrayUsage( "marksurfaces",	g_nummarksurfaces,	ENTRIES( g_dmarksurfaces ),	ENTRYSIZE( g_dmarksurfaces ));
	totalmemory += ArrayUsage( "surfedges",		g_numsurfedges,	ENTRIES( g_dsurfedges ),	ENTRYSIZE( g_dsurfedges ));
	totalmemory += ArrayUsage( "edges",		g_numedges,	ENTRIES( g_dedges ),	ENTRYSIZE( g_dedges ));

	totalmemory += GlobUsage( "texdata",		g_texdatasize,	MAX_MAP_MIPTEX );
	totalmemory += GlobUsage( "lightdata",		g_lightdatasize,	MAX_MAP_LIGHTING );
	totalmemory += GlobUsage( "visdata",		g_visdatasize,	sizeof( g_dvisdata ));
	totalmemory += GlobUsage( "entdata",		g_entdatasize,	sizeof( g_dentdata ));

	if( g_found_extradata )
	{
		totalmemory += GlobUsage( "normals", g_normaldatasize, MAX_MAP_LIGHTING );
		totalmemory += GlobUsage( "deluxdata", g_deluxdatasize, MAX_MAP_LIGHTING );
		totalmemory += ArrayUsage( "cubemaps", g_numcubemaps, ENTRIES( g_dcubemaps ), ENTRYSIZE( g_dcubemaps ));
		totalmemory += ArrayUsage( "faceinfo", g_numfaceinfo, ENTRIES( g_dfaceinfo ), ENTRYSIZE( g_dfaceinfo ));
		totalmemory += ArrayUsage( "direct lights", g_numworldlights, ENTRIES( g_dworldlights ), ENTRYSIZE( g_dworldlights ));
		totalmemory += ArrayUsage( "ambient cubes", g_numleaflights, ENTRIES( g_dleaflights ), ENTRYSIZE( g_dleaflights ));
		totalmemory += GlobUsage( "occlusion", g_shadowdatasize, MAX_MAP_LIGHTING / 3 );
		totalmemory += GlobUsage( "vertexlight", g_vlightdatasize, MAX_MAP_LIGHTING );
		totalmemory += GlobUsage( "vislightdata", g_vislightdatasize, MAX_MAP_VISLIGHTDATA );
	}

	Msg( "=== Total BSP file data space used: %s ===\n", Q_memprint( totalmemory ));
}

int GetSurfaceExtent( const dtexinfo_t *tex )
{
	ASSERT( tex != NULL );

	if( FBitSet( tex->flags, TEX_EXTRA_LIGHTMAP ))
		return MAX_EXTRA_EXTENTS;

	if( tex->faceinfo == -1 )
		return MAX_SURFACE_EXTENT;

	int max_extent = g_dfaceinfo[tex->faceinfo].max_extent;

	// check bounds
	if( max_extent >= MIN_CUSTOM_SURFACE_EXTENT && max_extent <= MAX_CUSTOM_SURFACE_EXTENT )
		return max_extent;

	return MAX_SURFACE_EXTENT;
}

int GetSurfaceExtent( const dface_t *f )
{
	ASSERT( f != NULL );

	return GetSurfaceExtent( &g_texinfo[f->texinfo] );
}

int GetTextureStep( const dtexinfo_t *tex, bool force )
{
	ASSERT( tex != NULL );

	if( !force && FBitSet( tex->flags, TEX_WORLD_LUXELS ))
		return 1;

	if( FBitSet( tex->flags, TEX_EXTRA_LIGHTMAP ))
		return TEXTURE_EXTRA_STEP;

	if( tex->faceinfo == -1 )
		return TEXTURE_STEP;

	int texture_step = g_dfaceinfo[tex->faceinfo].texture_step;

	// check bounds
	if( texture_step >= MIN_CUSTOM_TEXTURE_STEP && texture_step <= MAX_CUSTOM_TEXTURE_STEP )
		return texture_step;

	return TEXTURE_STEP;
}

int GetTextureStep( const dface_t *f, bool force )
{
	ASSERT( f != NULL );

	return GetTextureStep( &g_texinfo[f->texinfo], force );
}

word GetSurfaceGroupId( const dtexinfo_t *tex )
{
	ASSERT( tex != NULL );

	if( tex->faceinfo == -1 )
		return 0;

	return (word)g_dfaceinfo[tex->faceinfo].groupid;
}

word GetSurfaceGroupId( const dface_t *f )
{
	ASSERT( f != NULL );

	return GetSurfaceGroupId( &g_texinfo[f->texinfo] );
}

const char *GetTextureByTexinfo( int texinfo )
{
	if( texinfo != -1 )
	{
		dtexinfo_t *info = &g_texinfo[texinfo];
		int ofs = ((dmiptexlump_t*)g_dtexdata)->dataofs[info->miptex];
		miptex_t *miptex = (miptex_t*)(&g_dtexdata[ofs]);

		return miptex->name;
	}

	return "";
}

const char *ContentsToString( int type )
{
	switch( type )
	{
	case CONTENTS_EMPTY:
		return "EMPTY";
	case CONTENTS_SOLID:
		return "SOLID";
	case CONTENTS_WATER:
		return "WATER";
	case CONTENTS_SLIME:
		return "SLIME";
	case CONTENTS_LAVA:
		return "LAVA";
	case CONTENTS_SKY:
		return "SKY";
	case CONTENTS_ORIGIN:
		return "ORIGIN";
	case CONTENTS_TRANSLUCENT:
		return "TRANSLUCENT";
	default:
		return "UNKNOWN";
	}
}

int GetFaceContents( const char *name )
{
	if( !Q_strnicmp( name, "SKY", 3 ))
		return CONTENTS_SKY;

	if( name[0] == '!' || name[0] == '*' )
	{
		if( !Q_strnicmp( name + 1, "lava", 4 ))
			return CONTENTS_LAVA;
		else if( !Q_strnicmp( name + 1, "slime", 5 ))
			return CONTENTS_SLIME;
		return CONTENTS_WATER; // otherwise it's water
	}

	if( !Q_strnicmp( name, "water", 5 ))
		return CONTENTS_WATER;

	return CONTENTS_SOLID;
}

void MakeAxial( float normal[3] )
{
	int	i, type;

	for( type = 0; type < 3; type++ )
	{
		if( fabs( normal[type] ) > 0.9999f )
			break;
	}

	// make positive and pure axial
	for( i = 0; i < 3 && type != 3; i++ )
	{
		if( i == type )
			normal[i] = 1.0f;
		else normal[i] = 0.0f;
	}
}

void LightMatrixFromTexMatrix( const dtexinfo_t *tx, float lmvecs[2][4] )
{
	vec_t	lmscale = TEXTURE_STEP;

	if( tx->faceinfo != -1 )
	{
		int	texture_step = g_dfaceinfo[tx->faceinfo].texture_step;

		// check bounds
		if( texture_step >= MIN_CUSTOM_TEXTURE_STEP && texture_step <= MAX_CUSTOM_TEXTURE_STEP )
			lmscale = texture_step;
	}

	// copy texmatrix into lightmap matrix fisrt
	for( int i = 0; i < 2; i++ )
	{
		for( int j = 0; j < 4; j++ )
		{
			lmvecs[i][j] = tx->vecs[i][j];
		}
	}

	if( !FBitSet( tx->flags, TEX_WORLD_LUXELS ))
		return; // just use texmatrix

	VectorNormalize2( lmvecs[0] );
	VectorNormalize2( lmvecs[1] );

	if( FBitSet( tx->flags, TEX_AXIAL_LUXELS ))
	{
		MakeAxial( lmvecs[0] );
		MakeAxial( lmvecs[1] );
	}

	// put the lighting origin at center the poly
	VectorScale( lmvecs[0], (1.0 / lmscale), lmvecs[0] );
	VectorScale( lmvecs[1], -(1.0 / lmscale), lmvecs[1] );

	lmvecs[0][3] = lmscale * 0.5;
	lmvecs[1][3] = -lmscale * 0.5;
}

/*
=================
MapPlaneTypeForNormal
=================
*/
int MapPlaneTypeForNormal( const vec3_t normal )
{
	vec_t	ax, ay, az;

	ax = fabs( normal[0] );
	ay = fabs( normal[1] );
	az = fabs( normal[2] );

	if(( ax > 1.0 - DIR_EPSILON ) && ( ay < DIR_EPSILON ) && ( az < DIR_EPSILON ))
		return PLANE_X;

	if(( ay > 1.0 - DIR_EPSILON ) && ( az < DIR_EPSILON ) && ( ax < DIR_EPSILON ))
		return PLANE_Y;

	if(( az > 1.0 - DIR_EPSILON ) && ( ax < DIR_EPSILON ) && ( ay < DIR_EPSILON ))
		return PLANE_Z;

	return PLANE_NONAXIAL;
}

/*
================
SnapNormal
================
*/
int SnapNormal( vec3_t normal )
{
	int	type = MapPlaneTypeForNormal( normal );
	bool	renormalize = false;

	// snap normal to nearest axial if possible
	if( type <= PLANE_LAST_AXIAL )
	{
		for( int i = 0; i < 3; i++ )
		{
			if( i == type )
				normal[i] = normal[i] > 0 ? 1 : -1;
			else normal[i] = 0;
		}
		renormalize = true;
	}
	else
	{
		for( int i = 0; i < 3; i++ )
		{
			if( fabs( fabs( normal[i] ) - 0.707106 ) < DIR_EPSILON )
			{
				normal[i] = normal[i] > 0 ? 0.707106 : -0.707106;
				renormalize = true;
			}
		}
	}

	if( renormalize )
		VectorNormalize( normal );

	return type;
}

void StripTrailing( char *e )
{
	char	*s;

	s = e + Q_strlen( e ) - 1;

	while( s >= e && *s <= 32 )
	{
		*s = 0;
		s--;
	}
}

/*
================
InsertLinkBefore
================
*/
void InsertLinkBefore( epair_t *e, entity_t *mapent )
{
	e->next = NULL;

	if( mapent->epairs != NULL )
	{
		e->prev = mapent->tail;
		mapent->tail->next = e;
		mapent->tail = e;
	}
	else
	{
		mapent->epairs = mapent->tail = e;
		e->prev = NULL;
	}
}

/*
================
UnlinkEpair
================
*/
void UnlinkEpair( epair_t *e, entity_t *mapent )
{
	if( e->prev ) e->prev->next = e->next;
	else mapent->epairs = e->next;

	if( e->next ) e->next->prev = e->prev;
	else mapent->tail = e->prev;

	e->prev = e->next = e;
}

/*
=================
ParseEpair
=================
*/
epair_t *ParseEpair( void )
{
	const char	*ext;
	epair_t		*e;

	e = (epair_t *)Mem_Alloc( sizeof( epair_t ), C_EPAIR );

	if( Q_strlen( token ) >= ( MAX_KEY - 1 ))
		COM_FatalError( "ParseEpair: 'key' token too long\n" );

	e->key = copystring( token );
	GetToken( false );

	if( Q_strlen( token ) >= ( MAX_VALUE - 1 ))
		COM_FatalError( "ParseEpair: 'value' token too long\n" );
	ext = COM_FileExtension( token );

	if( !Q_stricmp( ext, "wav" ) || !Q_stricmp( ext, "mdl" ) || !Q_stricmp( ext, "bsp" )) 
		COM_FixSlashes( token );
	e->value = copystring( token );

	// strip trailing spaces
	StripTrailing( e->key );
	StripTrailing( e->value );

	return e;
}

/*
=================
ParseEpair
=================
*/
void CopyEpairs( entity_t *dst, entity_t *src )
{
	epair_t	*ep, *ep2;

	for( ep = src->epairs; ep; ep = ep->next )
	{
		ep2 = (epair_t *)Mem_Alloc( sizeof( epair_t ), C_EPAIR );
		ep2->key = copystring( ep->key );
		ep2->value = copystring( ep->value );
		InsertLinkBefore( ep2, dst );
	}
}

/*
================
ParseEntity
================
*/
bool ParseEntity( void )
{
	entity_t	*mapent;
	epair_t	*e;

	if( !GetToken( true ))
		return false;

	if( Q_strcmp( token, "{" ))
	{
		if( g_numentities == 0 )
			COM_FatalError( "ParseEntity: '{' not found\n" );
		else return false;	// probably entity string is broken at the end
	}

	if( g_numentities == MAX_MAP_ENTITIES )
		COM_FatalError( "MAX_MAP_ENTITIES limit exceeded\n" );

	mapent = &g_entities[g_numentities];
	g_numentities++;

	do
	{
		if( !GetToken( true ))
			COM_FatalError( "ParseEntity: EOF without closing brace\n" );

		if( !Q_strcmp( token, "}" ))
			break;

		e = ParseEpair();
		InsertLinkBefore( e, mapent );

	} while( 1 );

	return true;
}

/*
================
ParseEntities

Parses the dentdata string into entities
================
*/
void ParseEntities( void )
{
	ParseFromMemory( g_dentdata, g_entdatasize );
	g_numentities = 0;

	while( ParseEntity( ));
}

/*
================
FreeEntity

release all the entity data
================
*/
void FreeEntity( entity_t *mapent )
{
	epair_t	*ep, *next;

	for( ep = mapent->epairs; ep != NULL; ep = next )
	{
		next = ep->next;
		freestring( ep->key );
		freestring( ep->value );
		Mem_Free( ep, C_EPAIR );
	}

	if( mapent->cache ) Mem_Free( mapent->cache );
	mapent->epairs = mapent->tail = NULL;
	mapent->cache = NULL;
}

/*
================
FreeEntities

release all the dynamically allocated data
================
*/
void FreeEntities( void )
{
	for( int i = 0; i < g_numentities; i++ )
	{
		FreeEntity( &g_entities[i] );
	}
}

/*
================
UnparseEntities

Generates the dentdata string from all the entities
================
*/
void UnparseEntities( void )
{
	char	*buf, *end;
	char	line[2048];
	char	key[1024], value[1024];
	epair_t	*ep;

	buf = g_dentdata;
	end = buf;
	*end = 0;

	for( int i = 0; i < g_numentities; i++ )
	{
		entity_t	*ent = &g_entities[i];
		if( !ent->epairs ) continue;	// ent got removed

		Q_strcat( end, "{\n" );
		end += 2;

		for( ep = ent->epairs; ep; ep = ep->next )
		{
			Q_strncpy( key, ep->key, sizeof( key ));
			StripTrailing( key );
			Q_strncpy( value, ep->value, sizeof( value ));
			StripTrailing( value );

			Q_snprintf( line, sizeof( line ), "\"%s\" \"%s\"\n", key, value );
			Q_strcat( end, line );
			end += Q_strlen( line );
		}

		Q_strcat( end, "}\n" );
		end += 2;

		if( end > ( buf + MAX_MAP_ENTSTRING ))
			COM_FatalError( "Entity text too long\n" );
	}

	g_entdatasize = end - buf + 1; // write term
}

void RemoveKey( entity_t *ent, const char *key )
{
	epair_t	*ep;

	for( ep = ent->epairs; ep; ep = ep->next )
	{
		if( !Q_strcmp( ep->key, key ))
		{
			freestring( ep->key );
			freestring( ep->value );
			UnlinkEpair( ep, ent );
			Mem_Free( ep, C_EPAIR );
			return;
		}
	}
}

void RenameKey( entity_t *ent, const char *key, const char *name )
{
	epair_t	*ep;

	for( ep = ent->epairs; ep; ep = ep->next )
	{
		if( !Q_strcmp( ep->key, key ))
		{
			freestring( ep->key );
			ep->key = copystring( name );
			return;
		}
	}
	// otherwise we using SetKeyValue as well
}

void SetKeyValue( entity_t *ent, const char *key, const char *value )
{
	epair_t	*ep;

	for( ep = ent->epairs; ep; ep = ep->next )
	{
		if( !Q_strcmp( ep->key, key ))
		{
			freestring( ep->value );
			ep->value = copystring( value );
			return;
		}
	}

	ep = (epair_t *)Mem_Alloc( sizeof( epair_t ), C_EPAIR );
	ep->key = copystring( key );
	ep->value = copystring( value );
	InsertLinkBefore( ep, ent );
}

char *ValueForKey( entity_t *ent, const char *key, bool check )
{
	epair_t	*ep;

	for( ep = ent->epairs; ep; ep = ep->next )
	{
		if( !Q_strcmp( ep->key, key ))
			return ep->value;
	}

	if( check )
		return NULL;
	return "";
}

vec_t FloatForKey( entity_t *ent, const char *key )
{
	return atof( ValueForKey( ent, key ));
}

int IntForKey( entity_t *ent, const char *key )
{
	return atoi( ValueForKey( ent, key ));
}

bool BoolForKey( entity_t *ent, const char *key )
{
	if( atoi( ValueForKey( ent, key )))
		return true;
	return false;
}

int GetVectorForKey( entity_t *ent, const char *key, vec3_t vec )
{
	double	v1, v2, v3;
	int	count;
	char	*k;

	k = ValueForKey( ent, key );

	// scanf into doubles, then assign, so it is vec_t size independent
	v1 = v2 = v3 = 0;

	count = sscanf( k, "%lf %lf %lf", &v1, &v2, &v3 );

	vec[0] = v1;
	vec[1] = v2;
	vec[2] = v3;

	return count;
}

void SetVectorForKey( entity_t *ent, const char *key, vec3_t vec, bool precise )
{
	char	string[64];

	if( precise ) Q_snprintf( string, sizeof( string ), "%.2f %.2f %.2f", vec[0], vec[1], vec[2] );
	else Q_snprintf( string, sizeof( string ), "%i %i %i", (int)vec[0], (int)vec[1], (int)vec[2] );

	SetKeyValue( ent, key, string );
}

/*
================
EntityForModel

returns entity addy for given modelnum
================
*/
entity_t *EntityForModel( const int modnum )
{
	char	name[16];

	Q_snprintf( name, sizeof( name ), "*%i", modnum );

	// search the entities for one using modnum
	for( int i = 0; i < g_numentities; i++ )
	{
		const char *s = ValueForKey( &g_entities[i], "model" );
		if( !Q_strcmp( s, name )) return &g_entities[i];
	}

	return &g_entities[0];
}

// =====================================================================================
//  ModelForEntity
//      returns model addy for given entity
// =====================================================================================
dmodel_t *ModelForEntity( entity_t *ent )
{
	const char *s = ValueForKey( ent, "model" );

	if( *s != '*' ) return NULL;	// non-bmodel
	int modnum = atoi( s + 1 );

	if( modnum < 1 || modnum >= g_nummodels )
		return NULL;

	return &g_dmodels[modnum];
}

/*
==================
FindTargetEntity
==================
*/
entity_t *FindTargetEntity( const char *target )
{
	for( int i = 0; i < g_numentities; i++ )
	{
		char *n = ValueForKey( &g_entities[i], "targetname" );
		if( !Q_strcmp( n, target ))
			return &g_entities[i];
	}
	return NULL;
}

/*
===============================================================================

	ENTITY LINKING

===============================================================================
*/
/*
===============
ClearLink

ClearLink is used for new headnodes
===============
*/
void ClearLink( link_t *l )
{
	l->prev = l->next = l;
}

/*
===============
RemoveLink

remove link from chain
===============
*/
void RemoveLink( link_t *l )
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

/*
===============
InsertLinkBefore

kept trigger and solid entities seperate
===============
*/
void InsertLinkBefore( link_t *l, link_t *before )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/*
===============
CreateAreaNode

builds a uniformly subdivided tree for the given world size
===============
*/
areanode_t *CreateAreaNode( aabb_tree_t *tree, int depth, int maxdepth, vec3_t mins, vec3_t maxs )
{
	vec3_t		mins1, maxs1;
	vec3_t		mins2, maxs2;
	areanode_t	*anode;
	vec3_t		size;
 
	anode = &tree->areanodes[tree->numareanodes];
	tree->numareanodes++;

	ClearLink( &anode->solid_edicts );
	
	if( depth == Q_min( maxdepth, AREA_MAX_DEPTH ))
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract( maxs, mins, size );
	if( size[0] > size[1] )
		anode->axis = 0;
	else anode->axis = 1;
	
	anode->dist = 0.5f * ( maxs[anode->axis] + mins[anode->axis] );
	VectorCopy( mins, mins1 );	
	VectorCopy( mins, mins2 );	
	VectorCopy( maxs, maxs1 );	
	VectorCopy( maxs, maxs2 );	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	anode->children[0] = CreateAreaNode( tree, depth+1, maxdepth, mins2, maxs2 );
	anode->children[1] = CreateAreaNode( tree, depth+1, maxdepth, mins1, maxs1 );

	return anode;
}