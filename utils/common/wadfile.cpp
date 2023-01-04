/*
wadfile.cpp - reading & writing wad file lumps
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

#include "conprint.h"
#include <fcntl.h>
#include <stdio.h>
#include "cmdlib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "port.h"

#ifndef ALLOW_WADS_IN_PACKS
#define FS_Tell( x )	tell( x )
#define FS_Seek( x, y, z )	lseek( x, y, z )
#define FS_Read( x, y, z )	read( x, y, z )
#define FS_Write( x, y, z )	write( x, y, z )
#define FS_Close( x )	close( x )
#endif

static int replace_level = REP_IGNORE;

/*
=============================================================================

WADSYSTEM PRIVATE COMMON FUNCTIONS

=============================================================================
*/
// associate extension with wad type
static const wadtype_t wad_types[] =
{
{ "pal", TYP_PALETTE}, // palette
{ "dds", TYP_DDSTEX }, // DDS image
{ "lmp", TYP_GFXPIC	}, // quake1, hl pic
{ "fnt", TYP_QFONT	}, // hl qfonts
{ "mip", TYP_MIPTEX	}, // hl/q1 texture
{ NULL,  TYP_NONE	}  // terminator
};

// suffix converts to img_type and back
const wadtype_t wad_hints[] =
{
{ "",	 IMG_DIFFUSE	}, // no suffix
{ "_mask", IMG_ALPHAMASK	}, // alpha-channel stored to another lump
{ "_norm", IMG_NORMALMAP	}, // indexed normalmap
{ "_spec", IMG_GLOSSMAP	}, // grayscale\color specular
{ "_gpow", IMG_GLOSSPOWER	}, // grayscale gloss power
{ "_hmap", IMG_HEIGHTMAP	}, // heightmap (can be converted to normalmap)
{ "_luma", IMG_LUMA		}, // self-illuminate parts on the diffuse
{ "_adec", IMG_DECAL_ALPHA	}, // classic HL-decal (with alpha-channel)
{ "_cdec", IMG_DECAL_COLOR	}, // paranoia decal (base 127 127 127)
{ NULL,    0		}  // terminator
};

void SetReplaceLevel( int level )
{
	if( level < REP_IGNORE ) level = REP_IGNORE;
	if( level > REP_FORCE ) level = REP_FORCE;

	replace_level = level;
}

int GetReplaceLevel( void )
{
	return replace_level;
}

/*
===========
W_CheckHandle

return filehandle
===========
*/
static bool W_CheckHandle( wfile_t *wad )
{
	if( !wad ) return false;
#ifdef ALLOW_WADS_IN_PACKS
	return (wad->handle != NULL) ? true : false;
#else
	return (wad->handle >= 0) ? true : false;
#endif
}

/*
===========
W_GetHandle

return filehandle
===========
*/
int W_GetHandle( wfile_t *wad )
{
	if( !wad ) return -1;
#ifndef ALLOW_WADS_IN_PACKS
	return wad->handle;
#endif
	return -1;
}

/*
===========
W_TypeFromExt

Extracts file type from extension
===========
*/
int W_TypeFromExt( const char *lumpname )
{
	const char	*ext = COM_FileExtension( lumpname );
	const wadtype_t	*type;

	// we not known about filetype, so match only by filename
	if( !Q_strcmp( ext, "*" ) || !Q_strcmp( ext, "" ))
		return TYP_ANY;
	
	for( type = wad_types; type->ext; type++ )
	{
		if( !Q_stricmp( ext, type->ext ))
			return type->type;
	}

	return TYP_NONE;
}

/*
===========
W_ExtFromType

Convert type to extension
===========
*/
const char *W_ExtFromType( int lumptype )
{
	const wadtype_t	*type;

	// we not known about filetype, so match only by filename
	if( lumptype == TYP_NONE || lumptype == TYP_ANY )
		return "";

	for( type = wad_types; type->ext; type++ )
	{
		if( lumptype == type->type )
			return type->ext;
	}

	return "";
}

/*
===========
W_HintFromSuf

Convert name suffix into image type
===========
*/
char W_HintFromSuf( const char *lumpname )
{
	char		barename[64];
	char		suffix[8];
	size_t		namelen;
	const wadtype_t	*hint;

	// trying to extract hint from the name
	Q_strncpy( barename, lumpname, sizeof( barename ));
	namelen = Q_strlen( barename );

	if( namelen <= HINT_NAMELEN )
		return IMG_DIFFUSE;

	Q_strncpy( suffix, barename + namelen - HINT_NAMELEN, sizeof( suffix ));

	// we not known about filetype, so match only by filename
	for( hint = wad_hints; hint->ext; hint++ )
	{
		if( !Q_stricmp( suffix, hint->ext ))
			return hint->type;
	}

	// no any special type was found
	return IMG_DIFFUSE;
}

/*
===========
W_FindLump

Serach for already existed lump
===========
*/
dlumpinfo_t *W_FindLump( wfile_t *wad, const char *name, int matchtype )
{
	char		img_type = IMG_DIFFUSE;
	char		barename[64], suffix[8];
	int		left, right;
	size_t		namelen;
	const wadtype_t	*hint;

	if( !wad || !wad->lumps || matchtype == TYP_NONE )
		return NULL;

	// trying to extract hint from the name
	Q_strncpy( barename, name, sizeof( barename ));
	namelen = Q_strlen( barename );

	if( namelen > HINT_NAMELEN )
	{
		Q_strncpy( suffix, barename + namelen - HINT_NAMELEN, sizeof( suffix ));

		// we not known about filetype, so match only by filename
		for( hint = wad_hints; hint->ext; hint++ )
		{
			if( !Q_stricmp( suffix, hint->ext ))
			{
				img_type = hint->type;
				break;
			}
		}

		if( img_type != IMG_DIFFUSE )
			barename[namelen - HINT_NAMELEN] = '\0'; // kill the suffix
	}

	// look for the file (binary search)
	left = 0;
	right = wad->numlumps - 1;

	while( left <= right )
	{
		int	middle = (left + right) / 2;
		int	diff = Q_stricmp( wad->lumps[middle].name, barename );

		if( !diff )
		{
			if( wad->lumps[middle].img_type > img_type )
				diff = 1;
			else if( wad->lumps[middle].img_type < img_type )
				diff = -1;
			else if(( matchtype == TYP_ANY ) || ( matchtype == wad->lumps[middle].type ))
				return &wad->lumps[middle]; // found
			else if( wad->lumps[middle].type < matchtype )
				diff = 1;
			else if( wad->lumps[middle].type > matchtype )
				diff = -1;
			else break; // not found
		}

		// if we're too far in the list
		if( diff > 0 ) right = middle - 1;
		else left = middle + 1;
	}

	return NULL;
}

/*
====================
W_AddFileToWad

Add a file to the list of files contained into a package
and sort LAT in alpha-bethical order
====================
*/
static dlumpinfo_t *W_AddFileToWad( const char *name, wfile_t *wad, dlumpinfo_t *newlump )
{
	int		left, right;
	dlumpinfo_t	*plump;

	// look for the slot we should put that file into (binary search)
	left = 0;
	right = wad->numlumps - 1;

	while( left <= right )
	{
		int	middle = ( left + right ) / 2;
		int	diff = Q_stricmp( wad->lumps[middle].name, name );

		if( !diff )
		{
			if( wad->lumps[middle].img_type > newlump->img_type )
				diff = 1;
			else if( wad->lumps[middle].img_type < newlump->img_type )			
				diff = -1;
			else if( wad->lumps[middle].type < newlump->type )
				diff = 1;
			else if( wad->lumps[middle].type > newlump->type )
				diff = -1;
			else MsgDev( D_WARN, "Wad %s contains the file %s several times\n", wad->filename, name );
		}

		// if we're too far in the list
		if( diff > 0 ) right = middle - 1;
		else left = middle + 1;
	}

	// we have to move the right of the list by one slot to free the one we need
	plump = &wad->lumps[left];
	memmove( plump + 1, plump, ( wad->numlumps - left ) * sizeof( *plump ));
	wad->numlumps++;

	*plump = *newlump;
	memcpy( plump->name, name, sizeof( plump->name ));

	return plump;
}

/*
===========
W_ReadLump

reading lump into temp buffer
===========
*/
byte *W_ReadLump( wfile_t *wad, dlumpinfo_t *lump, size_t *lumpsizeptr )
{
	size_t	oldpos, size = 0;
	byte	*buf;

	// assume error
	if( lumpsizeptr ) *lumpsizeptr = 0;

	// no wads loaded
	if( !wad || !lump ) return NULL;

	oldpos = FS_Tell( wad->handle ); // don't forget restore original position

	if( FS_Seek( wad->handle, lump->filepos, SEEK_SET ) == -1 )
	{
		MsgDev( D_ERROR, "W_ReadLump: %s is corrupted\n", lump->name );
		FS_Seek( wad->handle, oldpos, SEEK_SET );
		return NULL;
	}

	buf = (byte *)Mem_Alloc( lump->disksize, C_FILESYSTEM );
	size = FS_Read( wad->handle, buf, lump->disksize );

	if( size < lump->disksize )
	{
		MsgDev( D_WARN, "W_ReadLump: %s is probably corrupted\n", lump->name );
		FS_Seek( wad->handle, oldpos, SEEK_SET );
		Mem_Free( buf, C_FILESYSTEM );
		return NULL;
	}

	if( lumpsizeptr ) *lumpsizeptr = lump->disksize;
	FS_Seek( wad->handle, oldpos, SEEK_SET );

	return buf;
}

/*
===========
W_WriteLump

compress and write lump
===========
*/
bool W_WriteLump( wfile_t *wad, dlumpinfo_t *lump, const void *data, size_t datasize )
{
	if( !wad || !lump ) return false;

	if( !data || !datasize )
	{
		MsgDev( D_WARN, "W_WriteLump: ignore blank lump %s - nothing to save\n", lump->name );
		return false;
	}

	if( wad->mode == O_RDONLY )
	{
		MsgDev( D_ERROR, "W_WriteLump: %s opened in readonly mode\n", wad->filename ); 
		return false;
	}

	lump->size = lump->disksize = static_cast<int>(datasize);

	if( FS_Write( wad->handle, data, datasize ) == datasize )
		return true;		
	return false;
}

static bool W_SysOpen( wfile_t *wad, const char *filename, const char *mode, bool ext_path )
{
#ifndef ALLOW_WADS_IN_PACKS
	int	mod, opt;

	// parse the mode string
	switch( mode[0] )
	{
	case 'r':
		mod = O_RDONLY;
		opt = 0;
		break;
	case 'w':
		mod = O_WRONLY;
		opt = O_CREAT|O_TRUNC;
		break;
	case 'a':
		mod = O_WRONLY;
		opt = O_CREAT;
		break;
	default:
		MsgDev( D_ERROR, "W_Open(%s, %s): invalid mode\n", filename, mode );
		return false;
	}

	for( int ind = 1; mode[ind] != '\0'; ind++ )
	{
		switch( mode[ind] )
		{
		case '+':
			mod = O_RDWR;
			break;
		case 'b':
			opt |= O_BINARY;
			break;
		default:
			MsgDev( D_ERROR, "W_Open: %s: unknown char in mode (%c)\n", filename, mode[ind] );
			break;
		}
	}

	wad->handle = open( filename, mod|opt, 0666 );
	return true;
#else
	// NOTE: FS_Open is load wad file from the first pak in the list (while ext_path is false)
	if( ext_path ) wad->handle = FS_Open( filename, "rb", false );
	else wad->handle = FS_Open( COM_FileWithoutPath( filename ), "rb", false );
	return true;
#endif
}

/*
=============================================================================

WADSYSTEM PUBLIC BASE FUNCTIONS

=============================================================================
*/
/*
===========
W_Open

open the wad for reading & writing
===========
*/
wfile_t *W_Open( const char *filename, const char *mode, int *error, bool ext_path )
{
	dwadinfo_t	header;
	wfile_t		*wad = (wfile_t *)Mem_Alloc( sizeof( wfile_t ), C_FILESYSTEM );
	const char	*comment = "Generated by Xash3D MakeWad tool.\0";
	int		ind, mod, opt, lumpcount;

	// parse the mode string
	switch( mode[0] )
	{
	case 'r':
		mod = O_RDONLY;
		opt = 0;
		break;
	case 'w':
		mod = O_WRONLY;
		opt = O_CREAT|O_TRUNC;
		break;
	case 'a':
		mod = O_WRONLY;
		opt = O_CREAT;
		break;
	default:
		MsgDev( D_ERROR, "W_Open(%s, %s): invalid mode\n", filename, mode );
		return NULL;
	}

	for( ind = 1; mode[ind] != '\0'; ind++ )
	{
		switch( mode[ind] )
		{
		case '+':
			mod = O_RDWR;
			break;
		case 'b':
			opt |= O_BINARY;
			break;
		default:
			MsgDev( D_ERROR, "W_Open: %s: unknown char in mode (%c)\n", filename, mode, mode[ind] );
			break;
		}
	}

#ifdef ALLOW_WADS_IN_PACKS
	// NOTE: FS_Open is load wad file from the first pak in the list (while ext_path is false)
	if( ext_path ) wad->handle = FS_Open( filename, "rb", false );
	else wad->handle = FS_Open( COM_FileWithoutPath( filename ), "rb", false );
#else
	wad->handle = open( filename, mod|opt, 0666 );
#endif
	if( !W_CheckHandle( wad ))
	{
		MsgDev( D_ERROR, "W_Open: couldn't open %s\n", filename );
		if( error ) *error = WAD_LOAD_COULDNT_OPEN;
		W_Close( wad );
		return NULL;
	}

	// copy wad name
	Q_strncpy( wad->filename, filename, sizeof( wad->filename ));
	wad->filetime = COM_FileTime( filename );
	FS_Seek( wad->handle, 0, SEEK_END );
	size_t wadsize = FS_Tell( wad->handle );
	FS_Seek( wad->handle, 0, SEEK_SET );

	// if the file is opened in "write", "append", or "read/write" mode
	if( mod == O_WRONLY || !wadsize )
	{
		dwadinfo_t hdr;

		wad->numlumps = 0;		// blank wad
		wad->lumps = NULL;		//
		wad->mode = O_WRONLY;

		// save space for header
		hdr.ident = IDWAD3HEADER;
		hdr.numlumps = wad->numlumps;
		hdr.infotableofs = sizeof( dwadinfo_t );
		FS_Write( wad->handle, &hdr, sizeof( hdr ));
		FS_Write( wad->handle, comment, Q_strlen( comment ) + 1 );
		wad->infotableofs = FS_Tell( wad->handle );
	}
	else if( mod == O_RDWR || mod == O_RDONLY )
	{
		if( mod == O_RDWR )
			wad->mode = O_APPEND;
		else wad->mode = O_RDONLY;

		if( FS_Read( wad->handle, &header, sizeof( dwadinfo_t )) != sizeof( dwadinfo_t ))
		{
			MsgDev( D_ERROR, "W_Open: %s can't read header\n", filename );
			if( error ) *error = WAD_LOAD_BAD_HEADER;
			W_Close( wad );
			return NULL;
		}

		if( header.ident != IDWAD3HEADER )
		{
			if( header.ident != IDWAD2HEADER )
			{
				MsgDev( D_ERROR, "W_Open: %s is not a WAD3 file\n", filename );
				if( error ) *error = WAD_LOAD_BAD_HEADER;
			}
			else if( error )
				*error = WAD_LOAD_NO_FILES;// shut up the warnings

			W_Close( wad );
			return NULL;
		}

		lumpcount = header.numlumps;

		if( lumpcount >= MAX_FILES_IN_WAD && wad->mode == O_APPEND )
		{
			MsgDev( D_WARN, "W_Open: %s is full (%i lumps)\n", filename, lumpcount );
			if( error ) *error = WAD_LOAD_TOO_MANY_FILES;
			wad->mode = O_RDONLY; // set read-only mode
		}
		else if( lumpcount <= 0 && wad->mode == O_RDONLY )
		{
			MsgDev( D_ERROR, "W_Open: %s has no lumps\n", filename );
			if( error ) *error = WAD_LOAD_NO_FILES;
			W_Close( wad );
			return NULL;
		}
		else if( error ) *error = WAD_LOAD_OK;

		wad->infotableofs = header.infotableofs; // save infotableofs position

		if( FS_Seek( wad->handle, wad->infotableofs, SEEK_SET ) == -1 )
		{
			MsgDev( D_ERROR, "W_Open: %s can't find lump allocation table\n", filename );
			if( error ) *error = WAD_LOAD_BAD_FOLDERS;
			W_Close( wad );
			return NULL;
		}

		size_t lat_size = lumpcount * sizeof( dlumpinfo_t );

		// NOTE: lumps table can be reallocated for O_APPEND mode
		dlumpinfo_t *srclumps = (dlumpinfo_t *)Mem_Alloc( lat_size, C_FILESYSTEM );

		if( FS_Read( wad->handle, srclumps, lat_size ) != lat_size )
		{
			MsgDev( D_ERROR, "W_ReadLumpTable: %s has corrupted lump allocation table\n", wad->filename );
			if( error ) *error = WAD_LOAD_CORRUPTED;
			Mem_Free( srclumps, C_FILESYSTEM );
			W_Close( wad );
			return NULL;
		}

		// starting to add lumps
		wad->lumps = (dlumpinfo_t *)Mem_Alloc( lat_size, C_FILESYSTEM );
		wad->numlumps = 0;

		// sort lumps for binary search
		for( int i = 0; i < lumpcount; i++ )
		{
			char	name[16];
			int	k;

			// cleanup lumpname
			Q_strncpy( name, srclumps[i].name, sizeof( name ));

			// check for '*' symbol issues (quake1)
			k = Q_strlen( Q_strrchr( name, '*' ));
			if( k ) name[Q_strlen( name ) - k] = '!'; // quake1 issues (can't save images that contain '*' symbol)

			// fixups bad image types (some quake wads)
			if( srclumps[i].img_type < 0 || srclumps[i].img_type > IMG_DECAL_COLOR )
				srclumps[i].img_type = IMG_DIFFUSE;

			W_AddFileToWad( name, wad, &srclumps[i] );
		}

		// release source lumps
		Mem_Free( srclumps, C_FILESYSTEM );

		// if we are in append mode - we need started from infotableofs poisition
		// overwrite lumptable as well, we have her copy in wad->lumps
		if( wad->mode == O_APPEND )
			FS_Seek( wad->handle, wad->infotableofs, SEEK_SET );
	}

	// and leave the file open
	return wad;
}

/*
===========
W_Close

finalize wad or just close
===========
*/
void W_Close( wfile_t *wad )
{
	if( !wad ) return;

	if( W_CheckHandle( wad ) && ( wad->mode == O_APPEND || wad->mode == O_WRONLY ))
	{
		dwadinfo_t	hdr;

		// write the lumpinfo
		size_t ofs = FS_Tell( wad->handle );
		FS_Write( wad->handle, wad->lumps, wad->numlumps * sizeof( dlumpinfo_t ));

		// write the header
		hdr.ident = IDWAD3HEADER;
		hdr.numlumps = wad->numlumps;
		hdr.infotableofs = static_cast<int>(ofs);

		FS_Seek( wad->handle, 0, SEEK_SET );
		FS_Write( wad->handle, &hdr, sizeof( hdr ));
	}

	if( wad->lumps )
		Mem_Free( wad->lumps, C_FILESYSTEM );

	if( W_CheckHandle( wad ))
		FS_Close( wad->handle );	
	Mem_Free( wad, C_FILESYSTEM ); // free himself
}

/*
===========
W_SaveLump

write new or replace existed lump
===========
*/
int W_SaveLump( wfile_t *wad, const char *lump, const void *data, size_t datasize, int type, char attribs )
{
	dlumpinfo_t *find, newlump;
	int lat_size, oldpos;
	char lumpname[64];

	if( !wad || !lump ) return -1;

	if( !data || !datasize )
	{
		MsgDev( D_WARN, "W_SaveLump: ignore blank lump %s - nothing to save\n", lump );
		return -1;
	}

	if( wad->mode == O_RDONLY )
	{
		MsgDev( D_ERROR, "W_SaveLump: %s opened in readonly mode\n", wad->filename ); 
		return -1;
	}

	if( wad->numlumps >= MAX_FILES_IN_WAD )
	{
		MsgDev( D_ERROR, "W_SaveLump: %s is full\n", wad->filename ); 
		return -1;
	}

	find = W_FindLump( wad, lump, type );

	if( find != NULL )
	{
		switch( replace_level )
		{
		case REP_IGNORE:
			MsgDev( D_ERROR, "W_SaveLump: %s already exist\n", lump ); 
			return -1;
		case REP_NORMAL:
		case REP_FORCE:
			if( FBitSet( find->attribs, ATTR_READONLY ))
			{
				// g-cont. i left this limitation as a protect of the replacement of compressed lumps
				MsgDev( D_ERROR, "W_ReplaceLump: %s is read-only\n", find->name );
				return -1;
			}

			if( datasize != find->size )
			{
				MsgDev( D_ERROR, "W_ReplaceLump: %s [%s] should be [%s]\n",
				lumpname, Q_memprint( datasize ), Q_memprint( find->size )); 
				return -1;
			}

			oldpos = FS_Tell( wad->handle ); // don't forget restore original position

			if( FS_Seek( wad->handle, find->filepos, SEEK_SET ) == -1 )
			{
				MsgDev( D_ERROR, "W_ReplaceLump: %s is corrupted\n", find->name );
				FS_Seek( wad->handle, oldpos, SEEK_SET );
				return -1;
			}

			if( FS_Write( wad->handle, data, datasize ) != find->disksize )
				MsgDev( D_WARN, "W_ReplaceLump: %s probably replaced with errors\n", find->name );

			// restore old position
			FS_Seek( wad->handle, oldpos, SEEK_SET );

			return wad->numlumps;
		}
	}

	// prepare lump name
	Q_strncpy( lumpname, lump, sizeof( lumpname ));

	// extract image hint
	char hint = W_HintFromSuf( lumpname );
	if( hint != IMG_DIFFUSE ) lumpname[Q_strlen( lumpname ) - HINT_NAMELEN] = '\0'; // kill the suffix

	if( Q_strlen( lumpname ) >= WAD3_NAMELEN )
	{
		// name is too long
		MsgDev( D_ERROR, "W_SaveLump: %s more than %i symbols\n", lumpname, WAD3_NAMELEN ); 
		return -1;
	}

	lat_size = sizeof( dlumpinfo_t ) * (wad->numlumps + 1);

	// reallocate lumptable
	wad->lumps = (dlumpinfo_t *)Mem_Realloc( wad->lumps, lat_size, C_FILESYSTEM );

	memset( &newlump, 0, sizeof( newlump ));

	// write header
	Q_strncpy( newlump.name, lumpname, WAD3_NAMELEN );
	newlump.filepos = FS_Tell( wad->handle );
	newlump.attribs = attribs;
	newlump.img_type = hint;
	newlump.type = type;

	if( !W_WriteLump( wad, &newlump, data, datasize ))
		return -1;

	// record entry and re-sort table
	W_AddFileToWad( lumpname, wad, &newlump );

	MsgDev( D_REPORT, "W_SaveLump: %s, size %s\n", newlump.name, Q_memprint( newlump.disksize ));

	return wad->numlumps;
}

/*
===========
W_LoadLump

loading lump into the tmp buffer
===========
*/
byte *W_LoadLump( wfile_t *wad, const char *lumpname, size_t *lumpsizeptr, int type )
{
	// assume error
	if( lumpsizeptr ) *lumpsizeptr = 0;

	if( !wad ) return NULL;
	dlumpinfo_t *lump = W_FindLump( wad, lumpname, type );
	return W_ReadLump( wad, lump, lumpsizeptr );
}

/*
===========
W_FindMiptex

find specified lump for .mip
===========
*/
dlumpinfo_t *W_FindMiptex( wfile_t *wad, const char *name )
{
	return W_FindLump( wad, name, TYP_MIPTEX );
}

/*
===========
W_FindLmptex

find specified lump for .lmp
===========
*/
dlumpinfo_t *W_FindLmptex( wfile_t *wad, const char *name )
{
	return W_FindLump( wad, name, TYP_GFXPIC );
}

/*
===========
W_SearchForFile

search in wad for filename
===========
*/
void W_SearchForFile( wfile_t *wad, const char *pattern, stringlist_t *resultlist )
{
	char		wadpattern[256], wadname[256], temp2[256];
	const char	*slash, *backslash, *colon, *separator;
	int			type = W_TypeFromExt( pattern );
	char		wadfolder[256], temp[1024];
	bool		anywadname = true;
	int		resultlistindex;

	if( !wad ) return;

	// quick reject by filetype
	if( type == TYP_NONE ) 
		return;

	COM_ExtractFilePath( pattern, wadname );
	COM_FileBase( pattern, wadpattern );
	wadfolder[0] = '\0';

	if( Q_strlen( wadname ))
	{
		COM_FileBase( wadname, wadname );
		Q_strncpy( wadfolder, wadname, sizeof( wadfolder ));
		COM_DefaultExtension( wadname, ".wad" );
		anywadname = false;
	}

	// make wadname from wad fullpath
	COM_FileBase( wad->filename, temp2 );
	COM_DefaultExtension( temp2, ".wad" );

	// quick reject by wadname
	if( !anywadname && Q_stricmp( wadname, temp2 ))
		return;

	// look through all the wad file elements
	for( int i = 0; i < wad->numlumps; i++ )
	{
		// if type not matching, we already have no chance ...
		if( type != TYP_ANY && wad->lumps[i].type != type )
			continue;

		// build the lumpname with image suffix (if present)
		Q_snprintf( temp, sizeof( temp ), "%s%s", wad->lumps[i].name, wad_hints[wad->lumps[i].img_type].ext );

		while( temp[0] )
		{
			if( matchpattern( temp, wadpattern, true ))
			{
				for( resultlistindex = 0; resultlistindex < resultlist->numstrings; resultlistindex++ )
				{
					if( !Q_strcmp( resultlist->strings[resultlistindex], temp ))
						break;
				}

				if( resultlistindex == resultlist->numstrings )
				{
					// build path: wadname/lumpname.ext
					Q_snprintf( temp2, sizeof( temp2 ), "%s/%s.%s", wadfolder, temp,
					W_ExtFromType( wad->lumps[i].type ));
					stringlistappend( resultlist, temp2 );
				}
			}

			// strip off one path element at a time until empty
			// this way directories are added to the listing if they match the pattern
			slash = Q_strrchr( temp, '/' );
			backslash = Q_strrchr( temp, '\\' );
			colon = Q_strrchr( temp, ':' );
			separator = temp;
			if( separator < slash )
				separator = slash;
			if( separator < backslash )
				separator = backslash;
			if( separator < colon )
				separator = colon;
			*((char *)separator) = 0;
		}
	}
}
