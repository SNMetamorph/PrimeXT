/*
bspfile.cpp - load, convert & write bsp
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

#include "bsp31migrate.h"
#include "filesystem.h"

#define VALVE_FORMAT	220

map_type	g_maptype = MAP_UNKNOWN;
sub_type	g_subtype = MAP_NORMAL;

//=============================================================================
bool		g_found_extradata = false;

int		g_nummodels;
dmodel_t		g_dmodels[MAX_MAP_MODELS];

int		g_visdatasize;
byte		g_dvisdata[MAX_MAP_VISIBILITY];

int		g_lightdatasize;
byte		*g_dlightdata;

int		g_deluxdatasize;
byte		*g_ddeluxdata;

int		g_shadowdatasize;
byte		*g_dshadowdata;

int		g_texdatasize;
byte*		g_dtexdata;                                  // (dmiptexlump_t)

int		g_entdatasize;
char		g_dentdata[MAX_MAP_ENTSTRING];

int		g_numleafs;
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
int		g_dsurfedges[MAX_MAP_SURFEDGES];

int		g_numfaceinfo;
dfaceinfo_t	g_dfaceinfo[MAX_MAP_FACEINFO];

int		g_numnormals;
dnormal_t		g_dnormals[MAX_MAP_NORMS];

int		g_numcubemaps;
dcubemap_t	g_dcubemaps[MAX_MAP_CUBEMAPS];

int		g_numleaflights;
dleafsample_t	g_dleaflights[MAX_MAP_LEAFLIGHTS];

int		g_numworldlights;
dworldlight_t	g_dworldlights[MAX_MAP_WORLDLIGHTS];

int		g_vlightdatasize;
byte		*g_dvlightdata; // (dvlightlump_t)

int		g_numfacedata;
dfacedata_t	g_dfacedata[MAX_MAP_FACES];

int		g_numTNbasis;
dTNbasis_t	g_dTNbasis[MAX_MAP_TNBASIS];

int		g_bumpdatasize;
byte		*g_dbumpdata;

int		g_numentities;
entity_t		g_entities[MAX_MAP_ENTITIES];

char		g_mapinfo[8192];
char		g_mapname[1024];
int		g_mapversion;
byte		*mod_base;

void PrintMapInfo( void )
{
	size_t	infolen = Q_strlen( g_mapinfo );
	char	*ptr = &g_mapinfo[infolen-2];

	if( *ptr == ',' ) *ptr = '.';

	Msg( "Map name: %s", g_mapname );
	Msg( "\nMap type: " );

	switch( g_maptype )
	{
	case MAP_XASH31:
		Msg( "^2XashXT BSP31^7" );
		break;
	default:
		COM_FatalError( "%s unknown map format\n", g_mapname );
		break;
	}

	if( g_subtype != MAP_NORMAL )
		Msg( "\nSub type: " );
	else Msg( "\n" );

	switch( g_subtype )
	{
	case MAP_HLFX06:
		Msg( "^4HLFX 0.6^7\n" );
		break;
	case MAP_XASHXT_OLD:
		Msg( "^4XashXT 0.5^7\n" );
		break;
	case MAP_P2SAVIOR:
		Msg( "^4Paranoia2: Savior^7\n" );
		break;
	case MAP_DEPRECATED:
		Msg( "^1intermediate deprecated version^7\n" );
		break;
	case MAP_XASH3D_EXT:
		Msg( "^4Xash3D extended^7\n" );
		break;
	}

	if( g_mapinfo[0] ) Msg( "Map info: %s", g_mapinfo );

	Msg( "\n\n" );
}

//=============================================================================

int CopyLump( int lump, void *dest, int size, dheader31_t *header )
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

int CopyLump( int lump, void *dest, int size, dheader31_hlfx_t *header )
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
	if( lump == HLFX31_LUMP_BUMP )
		dest = g_dbumpdata = (byte *)Mem_Alloc( length );
	memcpy( dest, (byte *)header + ofs, length );

	return length / size;
}

static int CopyExtraLump( int lump, void *dest, int size, const dheader31_t *header )
{
	dextrahdr_t *extrahdr = (dextrahdr_t *)((byte *)header + sizeof( dheader31_t ));

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
	memcpy( dest, (byte *)header + ofs, length );

	return length / size;
}

//=============================================================================
static void AddLump( int lumpnum, void *data, size_t len, dheader30_hlfx_t *header, long handle )
{
	dlump_t	*lump = &header->lumps[lumpnum];

	lump->fileofs = tell( handle );
	lump->filelen = len;

	SafeWrite( handle, data, (len + 3) & ~3 );
}

static void AddLump( int lumpnum, void *data, size_t len, dheader_t *header, long handle )
{
	dlump_t	*lump = &header->lumps[lumpnum];

	lump->fileofs = tell( handle );
	lump->filelen = len;

	SafeWrite( handle, data, (len + 3) & ~3 );
}

static void AddExtraLump( int lumpnum, void *data, int len, dextrahdr_t *header, long handle )
{
	dlump_t* lump = &header->lumps[lumpnum];

	lump->fileofs = tell( handle );
	lump->filelen = len;

	SafeWrite( handle, data, (len + 3) & ~3 );
}

void AddLumpClipnodes( int lumpnum, dheader_t *header, long handle )
{
	dlump_t *lump = &header->lumps[lumpnum];
	lump->fileofs = tell( handle );

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
		SafeWrite( handle, g_dclipnodes, (lump->filelen + 3) & ~3 );
	}
	else
	{
		// copy clipnodes into 32-bit array
		lump->filelen = g_numclipnodes * sizeof( dclipnode32_t );
		SafeWrite( handle, g_dclipnodes32, (lump->filelen + 3) & ~3 );
	}
}

//============================================
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
	memset( &g_entities, 0, sizeof( g_entities ));
	g_numentities = 0;

	while( ParseEntity( ));
}

/*
================
FreeEntities

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

void ReleaseBSPFile( void )
{
	FreeEntities();

	g_maptype = MAP_UNKNOWN;
	g_subtype = MAP_NORMAL;

	g_nummodels = 0;
	memset( g_dmodels, 0, sizeof( g_dmodels ));

	g_visdatasize = 0;
	memset( g_dvisdata, 0, sizeof( g_dvisdata ));

	g_lightdatasize = 0;
	Mem_Free( g_dlightdata );
	g_dlightdata = NULL;

	g_deluxdatasize = 0;
	Mem_Free( g_ddeluxdata );
	g_ddeluxdata = NULL;

	g_texdatasize = 0;
	Mem_Free( g_dtexdata ); 
	g_dtexdata = NULL;

	g_shadowdatasize = 0;
	Mem_Free( g_dshadowdata );
	g_dshadowdata = NULL;

	g_vlightdatasize = 0;
	Mem_Free( g_dvlightdata );
	g_dvlightdata = NULL;

	g_bumpdatasize = 0;
	Mem_Free( g_dbumpdata );
	g_dbumpdata = NULL;

	g_entdatasize = 0;
	memset( g_dentdata, 0, sizeof( g_dentdata ));

	g_numleafs = 0;
	memset( g_dleafs, 0, sizeof( g_dleafs ));

	g_numplanes = 0;
	memset( g_dplanes, 0, sizeof( g_dplanes ));

	g_numvertexes = 0;
	memset( g_dvertexes, 0, sizeof( g_dvertexes ));

	g_numnormals = 0;
	memset( g_dnormals, 0, sizeof( g_dnormals ));

	g_numnodes = 0;
	memset( g_dnodes, 0, sizeof( g_dnodes ));

	g_numtexinfo = 0;
	memset( g_texinfo, 0, sizeof( g_texinfo ));

	g_numfaces = 0;
	memset( g_dfaces, 0, sizeof( g_dfaces ));

	g_numclipnodes = 0;
	memset( g_dclipnodes, 0, sizeof( g_dclipnodes ));
	memset( g_dclipnodes32, 0, sizeof( g_dclipnodes32 ));

	g_numedges = 0;
	memset( g_dedges, 0, sizeof( g_dedges ));

	g_nummarksurfaces = 0;
	memset( g_dmarksurfaces, 0, sizeof( g_dmarksurfaces ));

	g_numsurfedges = 0;
	memset( g_dsurfedges, 0, sizeof( g_dsurfedges ));

	g_numentities = 0;
	memset( g_entities, 0, sizeof( g_entities ));

	g_numfaceinfo = 0;
	memset( g_dfaceinfo, 0, sizeof( g_dfaceinfo ));

	g_numcubemaps = 0;
	memset( g_dcubemaps, 0, sizeof( g_dcubemaps ));

	g_numleaflights = 0;
	memset( g_dleaflights, 0, sizeof( g_dleaflights ));

	g_numworldlights = 0;
	memset( g_dworldlights, 0, sizeof( g_dworldlights ));

	g_mapinfo[0] = '\0';
	g_mapname[0] = '\0';
	g_mapversion = 0;

	Mem_Free( mod_base );
	mod_base = NULL;
}

// =====================================================================================
//  WriteBSPFile
//      Swaps the bsp file in place, so it should not be referenced again
// =====================================================================================
void WriteBSPFile( const char *filename )
{
	dheader30_hlfx_t	outheader; // long header from HLFX
	dheader_t		*header;
	dheader30_hlfx_t	*hlfxhdr;
	dextrahdr_t	outextrahdr;
	dextrahdr_t	*extrahdr;
	long		bspfile;

	header = (dheader_t *)&outheader;
	hlfxhdr = &outheader;
	memset( &outheader, 0, sizeof( dheader30_hlfx_t ));
	extrahdr = &outextrahdr;
	memset( extrahdr, 0, sizeof( dextrahdr_t ));

	header->version = BSPVERSION;
	extrahdr->id = IDEXTRAHEADER;
	extrahdr->version = EXTRA_VERSION;
	hlfxhdr->magicID = HLFX_BSP_MAGIC_ID;
	
	COM_CreatePath( (char *)filename );
	bspfile = SafeOpenWrite( filename );

	if( g_subtype == MAP_HLFX06 )
	{
		SafeWrite( bspfile, hlfxhdr, sizeof( dheader30_hlfx_t ));	// overwritten later
	}
	else
	{
		SafeWrite( bspfile, header, sizeof( dheader_t ));		// overwritten later
		SafeWrite( bspfile, extrahdr, sizeof( dextrahdr_t ));	// overwritten later
	}

	UnparseEntities();

	//	LUMP TYPE		DATA		LENGTH					HEADER  BSPFILE   
	AddLump( LUMP_PLANES,	g_dplanes,	g_numplanes * sizeof( dplane_t ),		header, bspfile );
	AddLump( LUMP_LEAFS,	g_dleafs,		g_numleafs * sizeof( dleaf_t ),		header, bspfile );
	AddLump( LUMP_VERTEXES,	g_dvertexes,	g_numvertexes * sizeof( dvertex_t ),		header, bspfile );
	AddLump( LUMP_NODES,	g_dnodes,		g_numnodes * sizeof( dnode_t ),		header, bspfile );
	AddLump( LUMP_TEXINFO,	g_texinfo,	g_numtexinfo * sizeof( dtexinfo_t ),		header, bspfile );
	AddLump( LUMP_FACES,	g_dfaces,		g_numfaces * sizeof( dface_t ),		header, bspfile );
	AddLump( LUMP_MARKSURFACES,	g_dmarksurfaces,	g_nummarksurfaces * sizeof( dmarkface_t ),	header, bspfile );
	AddLump( LUMP_SURFEDGES,	g_dsurfedges,	g_numsurfedges * sizeof( dsurfedge_t ),		header, bspfile );
	AddLump( LUMP_EDGES,	g_dedges,		g_numedges * sizeof( dedge_t ),		header, bspfile );
	AddLump( LUMP_MODELS,	g_dmodels,	g_nummodels * sizeof( dmodel_t ),		header, bspfile );

	AddLumpClipnodes( LUMP_CLIPNODES, header, bspfile );	// clipnodes can using 16-bit or 32-bit indexes

	AddLump( LUMP_LIGHTING,	g_dlightdata,	g_lightdatasize,				header, bspfile );
	AddLump( LUMP_VISIBILITY,	g_dvisdata,	g_visdatasize,				header, bspfile );
	AddLump( LUMP_ENTITIES,	g_dentdata,	g_entdatasize,				header, bspfile );
	AddLump( LUMP_TEXTURES,	g_dtexdata,	g_texdatasize,				header, bspfile );

	if( g_subtype == MAP_HLFX06 )
	{
		AddLump( HLFX30_LUMP_FACEINFO,	g_dfacedata,	g_numfacedata * sizeof( dfacedata_t ),	hlfxhdr, bspfile );
		AddLump( HLFX30_LUMP_BUMP,		g_dbumpdata,	g_bumpdatasize,			hlfxhdr, bspfile );
		AddLump( HLFX30_LUMP_TNBASIS,		g_dTNbasis,	g_numTNbasis * sizeof( dTNbasis_t ),	hlfxhdr, bspfile );
	}
	else
	{
		AddExtraLump( LUMP_VERTNORMALS,  g_dnormals,      g_numnormals * sizeof( dnormal_t ),		extrahdr, bspfile );
		AddExtraLump( LUMP_LIGHTVECS,    g_ddeluxdata,    g_deluxdatasize,				extrahdr, bspfile );
		AddExtraLump( LUMP_CUBEMAPS,     g_dcubemaps,     g_numcubemaps * sizeof( dcubemap_t ),		extrahdr, bspfile );
		AddExtraLump( LUMP_FACEINFO,     g_dfaceinfo,     g_numfaceinfo * sizeof( dfaceinfo_t ),		extrahdr, bspfile );
		AddExtraLump( LUMP_WORLDLIGHTS,  g_dworldlights,  g_numworldlights * sizeof( dworldlight_t ),	extrahdr, bspfile );
		AddExtraLump( LUMP_SHADOWMAP,    g_dshadowdata,   g_shadowdatasize,				extrahdr, bspfile );
		AddExtraLump( LUMP_LEAF_LIGHTING,g_dleaflights,   g_numleaflights * sizeof( dleafsample_t ),	extrahdr, bspfile );
		AddExtraLump( LUMP_VERTEX_LIGHT, g_dvlightdata,   g_vlightdatasize,				extrahdr, bspfile );
	}

	lseek( bspfile, 0, SEEK_SET );

	if( g_subtype == MAP_HLFX06 )
	{
		SafeWrite( bspfile, hlfxhdr, sizeof( dheader30_hlfx_t ));
	}
	else
	{
		SafeWrite( bspfile, header, sizeof( dheader_t ));
		SafeWrite( bspfile, extrahdr, sizeof( dextrahdr_t ));
	}
	close( bspfile );

	ReleaseBSPFile();
}

/*
=================
Mod_FindModelOrigin

routine to detect bmodels with origin-brush
=================
*/
static void Mod_FindModelOrigin( const char *modelname, vec3_t origin )
{
	VectorClear( origin );

	if( !g_numentities ) return;

	// skip the world
	for( int i = 1; i < g_numentities; i++ )
	{
		entity_t	*mapent = &g_entities[i];

		if( !Q_stricmp( modelname, ValueForKey( mapent, "model" )))
		{
			GetVectorForKey( mapent, "origin", origin );
			return;
		}
	}
}

/*
===============================================================================

			MAP LOADING

===============================================================================
*/
/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels( const dlump_t *l )
{
	dmodel_t	*out = g_dmodels;
	dmodel_t	*in;
	int	i, j;
	
	in = (dmodel_t *)(mod_base + l->fileofs);

	if( l->filelen % sizeof( *in ))
		COM_FatalError( "Mod_LoadSubmodels: funny lump size\n" );
	g_nummodels = l->filelen / sizeof( *in );

	if( g_nummodels < 1 ) COM_FatalError( "Map %s without models\n", g_mapname );
	if( g_nummodels > MAX_MAP_MODELS ) COM_FatalError( "Map %s has too many models\n", g_mapname );

	for( i = 0; i < g_nummodels; i++, in++, out++ )
	{
		for( j = 0; j < 3; j++ )
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = in->mins[j];
			out->maxs[j] = in->maxs[j];
			out->origin[j] = in->origin[j];
		}

		if( i != 0 && VectorCompare( vec3_origin, out->origin ))
			Mod_FindModelOrigin( va( "*%i", i ), out->origin );

		for( j = 0; j < MAX_MAP_HULLS; j++ )
			out->headnode[j] = in->headnode[j];

		if( in->visleafs >= 65535 )
			COM_FatalError( "Mod_LoadSubmodels: visleafs %i exceeds an unsigned short limit\n", in->visleafs );

		out->visleafs = in->visleafs;
		out->firstface = in->firstface;
		out->numfaces = in->numfaces;
	}
}

/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities( const dlump_t *l )
{
	if( l->filelen > MAX_MAP_ENTSTRING )
		COM_FatalError( "Map %s exceeds MAX_MAP_ENTSTRING\n", g_mapname );

	memcpy( g_dentdata, mod_base + l->fileofs, l->filelen );
	g_entdatasize = l->filelen;

	ParseEntities();

	// grab mapversion
	g_mapversion = IntForKey( &g_entities[0], "mapversion" );
}

/*
=================
Mod_LoadTexInfo
=================
*/
static void Mod_LoadTexInfo( const dlump_t *l )
{
	dtexinfo_t	*out = g_texinfo;
	dtexinfo_t	*in;
	int		i, j;
	
	in = (dtexinfo_t *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		COM_FatalError( "Mod_LoadTexInfo: funny lump size in %s\n", g_mapname );

	g_numtexinfo = l->filelen / sizeof( *in );

	for( i = 0; i < g_numtexinfo; i++, in++, out++ )
	{
		for( j = 0; j < 8; j++ )
			out->vecs[0][j] = in->vecs[0][j];

		out->miptex = in->miptex;
		out->flags = in->flags;

		// tell the engine about hi-res lightmaps
		SetBits( out->flags, TEX_EXTRA_LIGHTMAP );

		if( g_subtype == MAP_XASH3D_EXT )
			out->faceinfo = in->faceinfo;
		else out->faceinfo = -1;
	}
}

/*
=================
Mod_LoadDeluxemap
=================
*/
static void Mod_LoadDeluxemap( void )
{
	char	path[64];
	size_t	filesize;
	byte	*in;

	if( g_subtype == MAP_XASH3D_EXT || g_subtype == MAP_HLFX06 )
		return; // Xash3D ext format have built-in deluxemap

	Q_strncpy( path, g_mapname, sizeof( path ));
	COM_StripExtension( path );
	COM_DefaultExtension( path, ".dlit" );
	if( !COM_FileExists( path )) return; // missed

	in = (byte *)COM_LoadFile( path, &filesize );

	if( *(uint *)in != IDDELUXEMAPHEADER || *((uint *)in + 1) != DELUXEMAP_VERSION )
	{
		MsgDev( D_ERROR, "Mod_LoadDeluxemap: %s is not a deluxemap file\n", path );
		g_ddeluxdata = NULL;
		g_deluxdatasize = 0;
		Mem_Free( in );
		return;
	}

	g_deluxdatasize = filesize;

	// skip header bytes
	g_deluxdatasize -= 8;

	if( g_deluxdatasize != g_lightdatasize )
	{
		MsgDev( D_ERROR, "Mod_LoadDeluxemap: %s has mismatched size (%i should be %i)\n", path, g_deluxdatasize, g_lightdatasize );
		g_ddeluxdata = NULL;
		g_deluxdatasize = 0;
		Mem_Free( in );
		return;
	}

	Q_strncat( g_mapinfo, "deluxemap included, ", sizeof( g_mapinfo ));
	MsgDev( D_REPORT, "Mod_LoadDeluxemap: %s loaded\n", path );
	g_ddeluxdata = (byte *)Mem_Alloc( g_deluxdatasize );
	memcpy( g_ddeluxdata, in + 8, g_deluxdatasize );
	Mem_Free( in );
}

/*
=================
Mod_LoadLightVecs
=================
*/
static void Mod_LoadLightVecs( const dlump_t *l )
{
	byte	*in;

	in = (byte *)(mod_base + l->fileofs);
	g_deluxdatasize = l->filelen;
	if( !l->filelen ) return;

	if( g_deluxdatasize != g_lightdatasize )
	{
		MsgDev( D_ERROR, "Mod_LoadLightVecs: has mismatched size (%i should be %i)\n", g_deluxdatasize, g_lightdatasize );
		g_ddeluxdata = NULL;
		g_deluxdatasize = 0;
		return;
	}

	Q_strncat( g_mapinfo, "deluxemap included, ", sizeof( g_mapinfo ));
	g_ddeluxdata = (byte *)Mem_Alloc( g_deluxdatasize );
	memcpy( g_ddeluxdata, in, g_deluxdatasize );
}

/*
=================
Mod_LoadClipnodes
=================
*/
static void Mod_LoadClipnodes( const dlump_t *l, const dlump_t *l2, const dlump_t *l3 )
{
	dclipnode_t	*in, *in2, *in3;
	dclipnode32_t	*out = g_dclipnodes32;
	int		i, count, count2, count3;

	in = (dclipnode_t *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in )) COM_FatalError( "Mod_LoadClipnodes1: funny lump size\n" );
	count = l->filelen / sizeof( *in );

	in2 = (dclipnode_t *)(mod_base + l2->fileofs);
	if( l2->filelen % sizeof( *in2 )) COM_FatalError( "Mod_LoadClipnodes2: funny lump size\n" );
	count2 = l2->filelen / sizeof( *in2 );

	in3 = (dclipnode_t *)(mod_base + l3->fileofs);
	if( l3->filelen % sizeof( *in3 )) COM_FatalError( "Mod_LoadClipnodes3: funny lump size\n" );
	count3 = l3->filelen / sizeof( *in3 );

	g_numclipnodes = 0;

	for( i = 0; i < count; i++, out++, in++ )
	{
		out->planenum = in->planenum;
		out->children[0] = in->children[0];
		out->children[1] = in->children[1];
		g_numclipnodes++;
	}

	// merge offsets so we have shared array of clipnodes again
	for( i = 0; i < count2; i++, out++, in2++ )
	{
		out->planenum = in2->planenum;
		out->children[0] = in2->children[0];
		out->children[1] = in2->children[1];

		if( out->children[0] >= 0 )
			out->children[0] += count;
		if( out->children[1] >= 0 )
			out->children[1] += count;
		g_numclipnodes++;
	}

	// merge offsets so we have shared array of clipnodes again
	for( i = 0; i < count3; i++, out++, in3++ )
	{
		out->planenum = in3->planenum;
		out->children[0] = in3->children[0];
		out->children[1] = in3->children[1];

		if( out->children[0] >= 0 )
			out->children[0] += (count + count2);
		if( out->children[1] >= 0 )
			out->children[1] += (count + count2);
		g_numclipnodes++;
	}

	// update headnode offsets
	for( i = 0; i < g_nummodels; i++ )
	{
		g_dmodels[i].headnode[2] += count;
		g_dmodels[i].headnode[3] += (count + count2);
	}

	if( g_numclipnodes != ( count + count2 + count3 ))
		COM_FatalError( "Mod_LoadClipnodes: mismatch node count (%i should be %i)\n", g_numclipnodes, ( count + count2 + count3 ));
}

/*
=============
LoadBsp31

load XashXT level file format
=============
*/
void LoadBsp31( void )
{
	dheader31_t *header = (dheader31_t *)mod_base;
	dextrahdr_t *extrahdr = (dextrahdr_t *)((byte *)mod_base + sizeof( dheader31_t ));
	dheader31_hlfx_t *hlfxhdr = (dheader31_hlfx_t *)mod_base;

	g_maptype = MAP_XASH31;

	if( extrahdr->id == IDEXTRAHEADER )
	{
		switch( extrahdr->version )
		{
		case 1:
			g_subtype = MAP_XASHXT_OLD;
			break;			
		case EXTRA_VERSION_OLD:
			g_subtype = MAP_P2SAVIOR;
			break;
		case 3:
			g_subtype = MAP_DEPRECATED;
			break;
		case EXTRA_VERSION:
			g_subtype = MAP_XASH3D_EXT;
			break;
		}
	}
	else if( hlfxhdr->magicID == HLFX_BSP_MAGIC_ID )
	{
		g_subtype = MAP_HLFX06;
	}

	// load into heap
	Mod_LoadEntities( &header->lumps[LUMP_ENTITIES] );

	if( g_mapversion != VALVE_FORMAT )
		MsgDev( D_WARN, "%s not a Valve 220 format\n", g_mapname );

	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );
	g_numplanes = CopyLump( LUMP_PLANES, g_dplanes, sizeof( dplane_t ), header );
	g_numvertexes = CopyLump( LUMP_VERTEXES, g_dvertexes, sizeof( dvertex_t ), header );
	g_numedges = CopyLump( LUMP_EDGES, g_dedges, sizeof( dedge_t ), header );
	g_numsurfedges = CopyLump( LUMP_SURFEDGES, g_dsurfedges, sizeof( dsurfedge_t ), header );
	g_visdatasize = CopyLump( LUMP_VISIBILITY, g_dvisdata, 1, header );
	Mod_LoadTexInfo( &header->lumps[LUMP_TEXINFO] );
	g_texdatasize = CopyLump( LUMP_TEXTURES, g_dtexdata, 1, header );
	g_lightdatasize = CopyLump( LUMP_LIGHTING, g_dlightdata, 1, header );
	g_numfaces = CopyLump( LUMP_FACES, g_dfaces, sizeof( dface_t ), header );
	g_nummarksurfaces = CopyLump( LUMP_MARKSURFACES, g_dmarksurfaces, sizeof( dmarkface_t ), header );
	g_numleafs = CopyLump( LUMP_LEAFS, g_dleafs, sizeof( dleaf_t ), header );
	g_numnodes = CopyLump( LUMP_NODES, g_dnodes, sizeof( dnode_t ), header );
	Mod_LoadClipnodes( &header->lumps[LUMP_CLIPNODES], &header->lumps[LUMP_CLIPNODES2], &header->lumps[LUMP_CLIPNODES3] );
	Mod_LoadDeluxemap ();

	if( g_subtype == MAP_XASH3D_EXT )
	{
		g_found_extradata = true;

		// g-cont. copy the extra lumps
		g_numnormals = CopyExtraLump( LUMP_VERTNORMALS, g_dnormals, sizeof( dnormal_t ), header );
		g_deluxdatasize = CopyExtraLump( LUMP_LIGHTVECS, g_ddeluxdata, 1, header );
		g_numcubemaps = CopyExtraLump( LUMP_CUBEMAPS, g_dcubemaps, sizeof( dcubemap_t ), header );
		g_numfaceinfo = CopyExtraLump( LUMP_FACEINFO, g_dfaceinfo, sizeof( dfaceinfo_t ), header );
		g_numworldlights = CopyExtraLump( LUMP_WORLDLIGHTS, g_dworldlights, sizeof( dworldlight_t ), header );
		g_numleaflights = CopyExtraLump( LUMP_LEAF_LIGHTING, g_dleaflights, sizeof( dleafsample_t ), header );
		g_shadowdatasize = CopyExtraLump( LUMP_SHADOWMAP, g_dshadowdata, 1, header );
		g_vlightdatasize = CopyExtraLump( LUMP_VERTEX_LIGHT, g_dvlightdata, 1, header );
	}
	else if( g_subtype == MAP_P2SAVIOR )
	{
		// P2: Savior regular format
		g_numcubemaps = CopyExtraLump( LUMP_CUBEMAPS, g_dcubemaps, sizeof( dcubemap_t ), header );
		g_numworldlights = CopyExtraLump( LUMP_WORLDLIGHTS, g_dworldlights, sizeof( dworldlight_t ), header );
	}
	else if( g_subtype == MAP_HLFX06 )
	{
		g_numfacedata = CopyLump( HLFX31_LUMP_FACEINFO, g_dfacedata, sizeof( dfacedata_t ), hlfxhdr );
		g_bumpdatasize = CopyLump( HLFX31_LUMP_BUMP, g_dbumpdata, 1, hlfxhdr );
		g_numTNbasis = CopyLump( HLFX31_LUMP_TNBASIS, g_dTNbasis, sizeof( dTNbasis_t ), hlfxhdr );
	}
	// preform some operations here...
}

/*
=============
LoadBSPFile
=============
*/
void LoadBSPFile( const char *infilename, const char *outfilename )
{
	MsgDev( D_REPORT, "Loading: %s\n", infilename );
	Q_strncpy( g_mapname, infilename, sizeof( g_mapname ));
	mod_base = (byte *)COM_LoadFile( g_mapname, NULL );

	if( *(uint *)mod_base == XT_BSPVERSION )
	{
		LoadBsp31();
		PrintMapInfo();
		WriteBSPFile( outfilename );
	}
	else
	{
		// not an BSP31
		Mem_Free( mod_base );
	}
}

int BspConvert( int argc, char **argv )
{
	char	source[1024], name[1024];
	char	output[1024];


	if( !COM_GetParmExt( "-file", source, sizeof( source )))
		Q_strncpy( source, "*.bsp", sizeof( source ));

	search_t *search = COM_Search( source, true );

	if( !search ) return 0;

	for( int i = 0; i < search->numfilenames; i++ )
	{
		COM_FileBase( search->filenames[i], name );
		Q_snprintf( output, sizeof( output ), "%s.bsp", name );
#if 0
		if( COM_FileExists( output ))
			continue;	// map already converted
#endif
		LoadBSPFile( search->filenames[i], output );
	}

	Mem_Free( search );
	Mem_Check();

	Msg( "press any key to exit\n" );
	system( "pause>nul" );

	return 0;
}