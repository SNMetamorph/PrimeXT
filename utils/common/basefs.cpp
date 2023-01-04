/*
filesystem.c - game filesystem based on DP fs
Copyright (C) 2007 Uncle Mike

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
#include <fcntl.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "bspfile.h"
#include "build.h"

#if !XASH_WIN32
#include <sys/types.h>
#include <dirent.h>
#endif

#define FILE_COPY_SIZE		(1024 * 1024)
#define FILE_BUFF_SIZE		(65535)
#define MAX_SYSPATH			1024	// system filepath

/*
========================================================================
PAK FILES

The .pak files are just a linear collapse of a directory tree
========================================================================
*/
// header
#define IDPACKV1HEADER	(('K'<<24)+('C'<<16)+('A'<<8)+'P')	// little-endian "PACK"
#define MAX_FILES_IN_PACK	65536		// pak

typedef struct
{
	int		ident;
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct
{
	char		name[56];		// total 64 bytes
	int		filepos;
	int		filelen;
} dpackfile_t;

// filesystem flags
#define FS_GAMEDIR_PATH		1	// just a marker for gamedir path

struct file_s
{
	int		handle;			// file descriptor
	size_t	real_length;	// uncompressed file size (for files opened in "read" mode)
	size_t	position;		// current position in the file
	size_t	offset;			// offset into the package (0 if external file)
	int		ungetc;			// single stored character from ungetc, cleared to EOF when read contents buffer
	time_t	filetime;				// pak, wad or real filetime
	size_t	buff_ind, buff_len;		// buffer current index and length
	byte	buff[FILE_BUFF_SIZE];	// intermediate buffer
};

typedef struct pack_s
{
	char		filename[256];
	int		handle;
	int		numfiles;
	time_t		filetime;	// common for all packed files
	dpackfile_t	*files;
} pack_t;

typedef struct searchpath_s
{
	char		filename[256];
	pack_t		*pack;
	wfile_t		*wad;
	int		flags;
	struct searchpath_s *next;
} searchpath_t;

searchpath_t		*fs_searchpaths = NULL;	// chain
searchpath_t		fs_directpath;		// static direct path
char			fs_rootdir[MAX_SYSPATH];	// engine root directory
char			fs_basedir[MAX_SYSPATH];	// base directory of game
char			fs_gamedir[MAX_SYSPATH];	// game current directory
char			fs_falldir[MAX_SYSPATH];	// game falling directory
bool			fs_ext_path = false;	// attempt to read\write from ./ or ../ pathes 

static searchpath_t *FS_FindFile(const char *name, int *index, bool gamedironly);
static dpackfile_t *FS_AddFileToPack(const char *name, pack_t *pack, int offset, int size);
static byte *W_LoadFile(const char *path, size_t *filesizeptr, bool gamedironly);
int FS_FileTime(const char *filename, bool gamedironly);
static bool FS_SysDirectoryExists(const char *filepath);
static void FS_Purge(file_t *file);

void FS_AllowDirectPaths( bool enable )
{
	fs_ext_path = enable;
}

/*
=============================================================================

OTHER PRIVATE FUNCTIONS

=============================================================================
*/
/*
====================
FS_AddFileToPack

Add a file to the list of files contained into a package
====================
*/
static dpackfile_t *FS_AddFileToPack( const char *name, pack_t *pack, int offset, int size )
{
	int		left, right, middle;
	dpackfile_t	*pfile;

	// look for the slot we should put that file into (binary search)
	left = 0;
	right = pack->numfiles - 1;

	while( left <= right )
	{
		int diff;

		middle = (left + right) / 2;
		diff = Q_stricmp( pack->files[middle].name, name );

		// If we found the file, there's a problem
		if( !diff ) MsgDev( D_WARN, "Package %s contains the file %s several times\n", pack->filename, name );

		// If we're too far in the list
		if( diff > 0 ) right = middle - 1;
		else left = middle + 1;
	}

	// We have to move the right of the list by one slot to free the one we need
	pfile = &pack->files[left];
	memmove( pfile + 1, pfile, (pack->numfiles - left) * sizeof( *pfile ));
	pack->numfiles++;

	Q_strncpy( pfile->name, name, sizeof( pfile->name ));
	pfile->filepos = offset;
	pfile->filelen = size;

	return pfile;
}

/*
=================
FS_LoadPackPAK

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPackPAK( const char *packfile, int *error )
{
	dpackheader_t	header;
	int		packhandle;
	int		i, numpackfiles;
	pack_t		*pack;
	dpackfile_t	*info;

	packhandle = open(packfile, O_RDONLY | O_BINARY, 0666);

	if( packhandle < 0 )
	{
		MsgDev( D_NOTE, "%s couldn't open\n", packfile );
		if( error ) *error = PAK_LOAD_COULDNT_OPEN;
		return NULL;
	}

	read( packhandle, (void *)&header, sizeof( header ));

	if( header.ident != IDPACKV1HEADER )
	{
		MsgDev( D_NOTE, "%s is not a packfile. Ignored.\n", packfile );
		if( error ) *error = PAK_LOAD_BAD_HEADER;
		close( packhandle );
		return NULL;
	}

	if( header.dirlen % sizeof( dpackfile_t ))
	{
		MsgDev( D_ERROR, "%s has an invalid directory size. Ignored.\n", packfile );
		if( error ) *error = PAK_LOAD_BAD_FOLDERS;
		close( packhandle );
		return NULL;
	}

	numpackfiles = header.dirlen / sizeof( dpackfile_t );

	if( numpackfiles > MAX_FILES_IN_PACK )
	{
		MsgDev( D_ERROR, "%s has too many files ( %i ). Ignored.\n", packfile, numpackfiles );
		if( error ) *error = PAK_LOAD_TOO_MANY_FILES;
		close( packhandle );
		return NULL;
	}

	if( numpackfiles <= 0 )
	{
		MsgDev( D_NOTE, "%s has no files. Ignored.\n", packfile );
		if( error ) *error = PAK_LOAD_NO_FILES;
		close( packhandle );
		return NULL;
	}

	info = (dpackfile_t *)Mem_Alloc( sizeof( *info ) * numpackfiles, C_FILESYSTEM );
	lseek( packhandle, header.dirofs, SEEK_SET );

	if( header.dirlen != read( packhandle, (void *)info, header.dirlen ))
	{
		MsgDev( D_NOTE, "%s is an incomplete PAK, not loading\n", packfile );
		if( error ) *error = PAK_LOAD_CORRUPTED;
		close( packhandle );
		Mem_Free( info, C_FILESYSTEM );
		return NULL;
	}

	pack = (pack_t *)Mem_Alloc( sizeof( pack_t ), C_FILESYSTEM );
	Q_strncpy( pack->filename, packfile, sizeof( pack->filename ));
	pack->files = (dpackfile_t *)Mem_Alloc( numpackfiles * sizeof( dpackfile_t ), C_FILESYSTEM );
	pack->filetime = COM_FileTime( packfile );
	pack->handle = packhandle;
	pack->numfiles = 0;

	// parse the directory
	for( i = 0; i < numpackfiles; i++ )
		FS_AddFileToPack( info[i].name, pack, info[i].filepos, info[i].filelen );

	if( error ) *error = PAK_LOAD_OK;
	Mem_Free( info, C_FILESYSTEM );

	return pack;
}

/*
====================
FS_AddWad_Fullpath
====================
*/
static bool FS_AddWad_Fullpath( const char *wadfile, bool *already_loaded, int flags )
{
	searchpath_t	*search;
	wfile_t		*wad = NULL;
	const char	*ext = COM_FileExtension( wadfile );
	int		errorcode = WAD_LOAD_COULDNT_OPEN;

	for( search = fs_searchpaths; search; search = search->next )
	{
		if( search->wad && !Q_stricmp( search->wad->filename, wadfile ))
		{
			if( already_loaded ) *already_loaded = true;
			return true; // already loaded
		}
	}
          
	if( already_loaded ) *already_loaded = false;
	if( !Q_stricmp( ext, "wad" )) wad = W_Open( wadfile, "rb", &errorcode, fs_ext_path );
	else MsgDev( D_ERROR, "\"%s\" doesn't have a wad extension\n", wadfile );

	if( wad )
	{
		search = (searchpath_t *)Mem_Alloc( sizeof( searchpath_t ), C_FILESYSTEM );
		search->wad = wad;
		search->next = fs_searchpaths;
		search->flags |= flags;
		fs_searchpaths = search;

		MsgDev( D_REPORT, "Adding wadfile: %s (%i files)\n", wadfile, wad->numlumps );
		return true;
	}
	else
	{
		if( errorcode != WAD_LOAD_NO_FILES )
			MsgDev( D_ERROR, "FS_AddWad_Fullpath: unable to load wad \"%s\"\n", wadfile );
		return false;
	}
}

/*
================
FS_AddPak_Fullpath

Adds the given pack to the search path.
The pack type is autodetected by the file extension.

Returns true if the file was successfully added to the
search path or if it was already included.

If keep_plain_dirs is set, the pack will be added AFTER the first sequence of
plain directories.
================
*/
static bool FS_AddPak_Fullpath( const char *pakfile, bool *already_loaded, int flags )
{
	searchpath_t	*search;
	pack_t		*pak = NULL;
	const char	*ext = COM_FileExtension( pakfile );
	int		i, errorcode = PAK_LOAD_COULDNT_OPEN;
	
	for( search = fs_searchpaths; search; search = search->next )
	{
		if( search->pack && !Q_stricmp( search->pack->filename, pakfile ))
		{
			if( already_loaded ) *already_loaded = true;
			return true; // already loaded
		}
	}

	if( already_loaded ) *already_loaded = false;

	if( !Q_stricmp( ext, "pak" )) pak = FS_LoadPackPAK( pakfile, &errorcode );
	else MsgDev( D_ERROR, "\"%s\" does not have a pack extension\n", pakfile );

	if( pak )
	{
		string	fullpath;

		search = (searchpath_t *)Mem_Alloc( sizeof( searchpath_t ), C_FILESYSTEM );
		search->pack = pak;
		search->next = fs_searchpaths;
		search->flags |= flags;
		fs_searchpaths = search;

		MsgDev( D_REPORT, "Adding pakfile: %s (%i files)\n", pakfile, pak->numfiles );

		// time to add in search list all the wads that contains in current pakfile (if do)
		for( i = 0; i < pak->numfiles; i++ )
		{
			if( !Q_stricmp( COM_FileExtension( pak->files[i].name ), "wad" ))
			{
				Q_sprintf( fullpath, "%s/%s", pakfile, pak->files[i].name );
				FS_AddWad_Fullpath( fullpath, NULL, flags );
			}
		}

		return true;
	}
	else
	{
		if( errorcode != PAK_LOAD_NO_FILES )
			MsgDev( D_ERROR, "FS_AddPak_Fullpath: unable to load pak \"%s\"\n", pakfile );
		return false;
	}
}

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void FS_AddGameDirectory( const char *dir, int flags )
{
	stringlist_t	list;
	searchpath_t	*search;
	char		fullpath[256];
	int		i;

	stringlistinit( &list );
	listdirectory( &list, dir, true );
	stringlistsort( &list );

	// add any PAK package in the directory
	for( i = 0; i < list.numstrings; i++ )
	{
		if( !Q_stricmp( COM_FileExtension( list.strings[i] ), "pak" ))
		{
			Q_sprintf( fullpath, "%s%s", dir, list.strings[i] );
			FS_AddPak_Fullpath( fullpath, NULL, flags );
		}
	}

	FS_AllowDirectPaths( true );

	// add any WAD package in the directory
	for( i = 0; i < list.numstrings; i++ )
	{
		if( !Q_stricmp( COM_FileExtension( list.strings[i] ), "wad" ))
		{
			Q_sprintf( fullpath, "%s%s", dir, list.strings[i] );
			FS_AddWad_Fullpath( fullpath, NULL, flags );
		}
	}

	stringlistfreecontents( &list );
	FS_AllowDirectPaths( false );

	// add the directory to the search path
	// (unpacked files have the priority over packed files)
	search = (searchpath_t *)Mem_Alloc( sizeof( searchpath_t ), C_FILESYSTEM );
	Q_strncpy( search->filename, dir, sizeof ( search->filename ));
	search->next = fs_searchpaths;
	search->flags = flags;
	fs_searchpaths = search;


}

/*
================
FS_AddGameHierarchy
================
*/
void FS_AddGameHierarchy( const char *dir, int flags )
{
	// Add the common game directory
	if (dir && *dir) {
		FS_AddGameDirectory(va("%s/%s/", fs_rootdir, dir), flags);
	}
}

/*
================
FS_ClearSearchPath
================
*/
void FS_ClearSearchPath( void )
{
	while( fs_searchpaths )
	{
		searchpath_t	*search = fs_searchpaths;

		if( !search ) break;

		fs_searchpaths = search->next;

		if( search->pack )
		{
			if( search->pack->files ) 
				Mem_Free( search->pack->files, C_FILESYSTEM );
			Mem_Free( search->pack, C_FILESYSTEM );
		}

		if( search->wad )
			W_Close( search->wad );

		Mem_Free( search, C_FILESYSTEM );
	}

	fs_searchpaths = NULL;
}

/*
================
FS_ParseLiblistGam
================
*/
static bool FS_ParseLiblistGam( const char *filename, const char *gamedir )
{
	char	*afile, *pfile;
	char	token[256];

	afile = (char *)FS_LoadFile( filename, NULL, false );
	if( !afile ) return false;
	
	Q_strncpy( fs_basedir, "valve", sizeof( fs_basedir ));
	COM_FileBase( gamedir, fs_gamedir );

	pfile = afile;

	while(( pfile = COM_ParseFile( pfile, token )) != NULL )
	{
		if( !Q_stricmp( token, "gamedir" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( Q_stricmp( token, fs_basedir ) || Q_stricmp( token, fs_gamedir ))
				Q_strncpy( fs_gamedir, token, sizeof( fs_gamedir ));
		}
		if( !Q_stricmp( token, "fallback_dir" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( Q_stricmp( token, fs_basedir ) || Q_stricmp( token, fs_falldir ))
				Q_strncpy( fs_falldir, token, sizeof( fs_falldir ));
		}
	}

	if( afile != NULL )
		Mem_Free( afile, C_FILESYSTEM );

	return true;
}

/*
================
FS_ParseGameInfo
================
*/
static bool FS_ParseGameInfo( const char *filename, const char *gamedir )
{
	char	*afile, *pfile;
	char	token[256];

	afile = (char *)FS_LoadFile( filename, NULL, false );
	if( !afile ) return false;

	// setup default values
	COM_FileBase( gamedir, fs_gamedir );

	pfile = afile;

	while(( pfile = COM_ParseFile( pfile, token )) != NULL )
	{
		if( !Q_stricmp( token, "basedir" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( Q_stricmp( token, fs_basedir ) || Q_stricmp( token, fs_gamedir ))
				Q_strncpy( fs_basedir, token, sizeof( fs_basedir ));
		}
		else if( !Q_stricmp( token, "fallback_dir" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( Q_stricmp( token, fs_basedir ) || Q_stricmp( token, fs_falldir ))
				Q_strncpy( fs_falldir, token, sizeof( fs_falldir ));
		}
		else if( !Q_stricmp( token, "gamedir" ))
		{
			pfile = COM_ParseFile( pfile, token );
			if( Q_stricmp( token, fs_basedir ) || Q_stricmp( token, fs_gamedir ))
				Q_strncpy( fs_gamedir, token, sizeof( fs_gamedir ));
		}
	}

	if( afile != NULL )
		Mem_Free( afile, C_FILESYSTEM );

	return true;
}

void FS_ReadGameInfo( const char *gamedir )
{
	char	liblist[256], gameinfo[256];

	Q_strncpy( gameinfo, "gameinfo.txt", sizeof( gameinfo ));
	Q_strncpy( liblist, "liblist.gam", sizeof( liblist ));

	// if user change liblist.gam update the gameinfo.txt
	if( FS_FileTime( liblist, false ) > FS_FileTime( gameinfo, false ))
		FS_ParseLiblistGam( liblist, gamedir );
	else FS_ParseGameInfo( gameinfo, gamedir );
}

/*
================
FS_Rescan
================
*/
void FS_Rescan( void )
{
	FS_ClearSearchPath();

	if (Q_stricmp(fs_basedir, fs_gamedir))
		FS_AddGameHierarchy(fs_basedir, 0);

	if (Q_stricmp(fs_basedir, fs_falldir) && Q_stricmp(fs_gamedir, fs_falldir))
		FS_AddGameHierarchy(fs_falldir, 0);

	FS_AddGameHierarchy(fs_gamedir, FS_GAMEDIR_PATH);
	if (FS_SysDirectoryExists(va("%s/%s/custom/", fs_rootdir, fs_gamedir))) {
		FS_AddGameHierarchy(va("%s/custom", fs_gamedir), 0);
	}
}

/*
================
FS_Init
================
*/
void FS_Init( const char *source )
{
	char	workdir[MAX_PATH];
	char	mapdir[MAX_PATH];
	char	hullfile[MAX_PATH];
	char	mapfile[MAX_PATH];
	char	*pathend;

	fs_searchpaths = NULL;

	// skip the unneeded separator
	if( source[0] == '.' && ( source[1] == '/' || source[1] == '\\' ))
		Q_snprintf( mapdir, sizeof( mapdir ), "%s.map", source + 2 );
	else Q_snprintf( mapdir, sizeof( mapdir ), "%s.map", source );

	// create full path to a map
	Q_strncpy( workdir, COM_ExpandArg( mapdir ), sizeof( workdir ));

	// search for 'maps' in path
	pathend = Q_stristr( workdir, "maps" );

	if( !pathend )
	{
		MsgDev( D_ERROR, "FS_Init: couldn't init game directory!\n" );
		return;
	}

	// create gamedir path
	Q_strncpy( fs_rootdir, workdir, pathend - workdir );
	int length = Q_strlen( fs_rootdir );

	if( fs_rootdir[length-1] == '.' && ( fs_rootdir[length-2] == '/' || fs_rootdir[length-2] == '\\' ))
		fs_rootdir[length-2] = '\0';

	MsgDev( D_REPORT, "workdir: %s\n", fs_rootdir );

	Q_snprintf( hullfile, sizeof( hullfile ), "%s/hulls.txt", fs_rootdir );
	FS_AddGameDirectory( va( "%s/", fs_rootdir ), 0 );
	FS_ReadGameInfo( fs_rootdir );

	MsgDev( D_REPORT, "gamedir: %s, basedir %s, falldir %s\n", fs_gamedir, fs_basedir, fs_falldir );
	COM_ExtractFilePath( fs_rootdir, fs_rootdir );
	COM_FileBase( source, mapfile );

	if( !Q_strcmp( fs_rootdir, "" ))
		Q_strncpy( fs_rootdir, "../", sizeof( fs_rootdir ));

	MsgDev( D_INFO, "rootdir %s\n", fs_rootdir );	// for debug
	MsgDev( D_INFO, "source: %s.map\n\n", mapfile );	// for debug
	FS_Rescan(); // create new filesystem

	MsgDev( D_REPORT, "FS_Init: done\n" );
	MsgDev( D_REPORT, "Current search path:\n" );

	for (searchpath_t *s = fs_searchpaths; s; s = s->next)
	{
		if (s->pack) {
			MsgDev(D_REPORT, "%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		}
		else if (s->wad) {
			MsgDev(D_REPORT, "%s (%i files)\n", s->wad->filename, s->wad->numlumps);
		}
		else {
			MsgDev(D_REPORT, "%s\n", s->filename);
		}
	}

	if (COM_FileExists(hullfile)) {
		CheckHullFile(hullfile);
	}
}

/*
================
FS_Shutdown
================
*/
void FS_Shutdown( void )
{
	FS_ClearSearchPath();
}

/*
====================
FS_SysOpen

Internal function used to create a file_t and open the relevant non-packed file on disk
====================
*/
static file_t *FS_SysOpen( const char *filepath, const char *mode )
{
	file_t	*file;
	int	mod, opt;
	uint	ind;

	// Parse the mode string
	switch( mode[0] )
	{
	case 'r':	// read
		mod = O_RDONLY;
		opt = 0;
		break;
	case 'w': // write
		mod = O_WRONLY;
		opt = O_CREAT | O_TRUNC;
		break;
	case 'a': // append
		mod = O_WRONLY;
		opt = O_CREAT | O_APPEND;
		break;
	case 'e': // edit
		mod = O_WRONLY;
		opt = O_CREAT;
		break;
	default:
		MsgDev( D_ERROR, "FS_SysOpen(%s, %s): invalid mode\n", filepath, mode );
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
			MsgDev( D_ERROR, "FS_SysOpen: %s: unknown char in mode (%c)\n", filepath, mode, mode[ind] );
			break;
		}
	}

	file = (file_t *)Mem_Alloc( sizeof( *file ), C_FILESYSTEM );
	file->filetime = COM_FileTime( filepath );
	file->ungetc = EOF;

	file->handle = open( filepath, mod|opt, 0666 );

	if( file->handle < 0 )
	{
		Mem_Free( file, C_FILESYSTEM );
		return NULL;
	}

	file->real_length = lseek( file->handle, 0, SEEK_END );

	// For files opened in append mode, we start at the end of the file
	if( mod & O_APPEND ) file->position = file->real_length;
	else lseek( file->handle, 0, SEEK_SET );

	return file;
}

/*
====================
FS_SysDirectoryExists

Check, using system API, is specified directory exists or not
====================
*/
static bool FS_SysDirectoryExists(const char *filepath)
{
#if XASH_WIN32
	DWORD dwAttrib = GetFileAttributes(filepath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
	DIR *dir = opendir(filepath);
	if (dir)
	{
		closedir(dir);
		return true;
	}
	return false;
#endif
}

/*
===========
FS_OpenPackedFile

Open a packed file using its package file descriptor
===========
*/
file_t *FS_OpenPackedFile( pack_t *pack, int pack_ind )
{
	dpackfile_t	*pfile;
	int		dup_handle;
	file_t		*file;

	pfile = &pack->files[pack_ind];

	if( lseek( pack->handle, pfile->filepos, SEEK_SET ) == -1 )
		return NULL;

	dup_handle = dup( pack->handle );

	if( dup_handle < 0 )
		return NULL;

	file = (file_t *)Mem_Alloc( sizeof( *file ), C_FILESYSTEM );
	file->handle = dup_handle;
	file->real_length = pfile->filelen;
	file->offset = pfile->filepos;
	file->position = 0;
	file->ungetc = EOF;

	return file;
}

/*
====================
FS_FindFile

Look for a file in the packages and in the filesystem

Return the searchpath where the file was found (or NULL)
and the file index in the package if relevant
====================
*/
static searchpath_t *FS_FindFile( const char *name, int *index, bool gamedironly )
{
	searchpath_t	*search;

	if( !COM_CheckString( name ))
		return NULL;

	// search through the path, one element at a time
	for( search = fs_searchpaths; search; search = search->next )
	{
		if (gamedironly && !FBitSet(search->flags, FS_GAMEDIR_PATH))
			continue;

		// is the element a pak file?
		if( search->pack )
		{
			int	left, right, middle;
			pack_t	*pak;

			pak = search->pack;

			// look for the file (binary search)
			left = 0;
			right = pak->numfiles - 1;
			while( left <= right )
			{
				int	diff;

				middle = (left + right) / 2;
				diff = Q_stricmp( pak->files[middle].name, name );

				// Found it
				if( !diff )
				{
					if( index ) *index = middle;
					return search;
				}

				// if we're too far in the list
				if( diff > 0 )
					right = middle - 1;
				else left = middle + 1;
			}
		}
		else if( search->wad )
		{
			dlumpinfo_t	*lump;	
			int			type = W_TypeFromExt( name );
			bool		anywadname = true;
			string		wadname, wadfolder;
			string		shortname;

			// quick reject by filetype
			if( type == TYP_NONE ) 
				continue;

			COM_ExtractFilePath( name, wadname );
			wadfolder[0] = '\0';

			if( Q_strlen( wadname ))
			{
				COM_FileBase( wadname, wadname );
				Q_strncpy( wadfolder, wadname, sizeof( wadfolder ));
				COM_DefaultExtension( wadname, ".wad" );
				anywadname = false;
			}

			// make wadname from wad fullpath
			COM_FileBase( search->wad->filename, shortname );
			COM_DefaultExtension( shortname, ".wad" );

			// quick reject by wadname
			if( !anywadname && Q_stricmp( wadname, shortname ))
				continue;

			// NOTE: we can't using long names for wad,
			// because we using original wad names[16];
			COM_FileBase( name, shortname );

			lump = W_FindLump( search->wad, shortname, type );

			if( lump )
			{
				if( index )
					*index = lump - search->wad->lumps;
				return search;
			}
		}
		else
		{
			char	netpath[MAX_SYSPATH];

			Q_sprintf( netpath, "%s%s", search->filename, name );

			if( COM_FileExists( netpath ))
			{
				if( index != NULL ) *index = -1;
				return search;
			}
		}
	}

	if( fs_ext_path )
	{
		// clear searchpath
		search = &fs_directpath;
		memset( search, 0, sizeof( searchpath_t ));

		if( COM_FileExists( name ))
		{
			if( index != NULL )
				*index = -1;
			return search;
		}
	}

	if( index != NULL )
		*index = -1;

	return NULL;
}


/*
===========
FS_OpenReadFile

Look for a file in the search paths and open it in read-only mode
===========
*/
file_t *FS_OpenReadFile( const char *filename, const char *mode, bool gamedironly )
{
	searchpath_t	*search;
	int		pack_ind;

	search = FS_FindFile( filename, &pack_ind, gamedironly );

	// not found?
	if( search == NULL )
		return NULL; 

	if( search->pack )
		return FS_OpenPackedFile( search->pack, pack_ind );
	else if( search->wad )
		return NULL; // let W_LoadFile get lump correctly
	else if( pack_ind < 0 )
	{
		char	path [MAX_SYSPATH];

		// found in the filesystem?
		Q_sprintf( path, "%s%s", search->filename, filename );
		return FS_SysOpen( path, mode );
	} 

	return NULL;
}

/*
=============================================================================

MAIN PUBLIC FUNCTIONS

=============================================================================
*/
/*
====================
FS_Open

Open a file. The syntax is the same as fopen
====================
*/
file_t *FS_Open( const char *filepath, const char *mode, bool gamedironly )
{
	// some stupid mappers used leading '/' or '\' in path to models or sounds
#if XASH_WIN32 // schizophrenia
	if( filepath[0] == '/' || filepath[0] == '\\' )
		filepath++;

	if( filepath[0] == '/' || filepath[0] == '\\' )
		filepath++;
#endif

	// if the file is opened in "write", "append", or "read/write" mode
	if( mode[0] == 'w' || mode[0] == 'a'|| mode[0] == 'e' || Q_strchr( mode, '+' ))
	{
		char	real_path[MAX_SYSPATH];

		// open the file on disk directly
		Q_sprintf( real_path, "%s", filepath );
		COM_CreatePath( real_path );// Create directories up to the file
		return FS_SysOpen( real_path, mode );
	}
	
	// else, we look at the various search paths and open the file in read-only mode
	return FS_OpenReadFile( filepath, mode, gamedironly );
}

/*
====================
FS_Close

Close a file
====================
*/
int FS_Close( file_t *file )
{
	if( !file ) return 0;

	if( close( file->handle ))
		return EOF;

	Mem_Free( file, C_FILESYSTEM );
	return 0;
}

/*
====================
FS_Read

Read up to "buffersize" bytes from a file
====================
*/
size_t FS_Read( file_t *file, void *buffer, size_t buffersize )
{
	int64_t count, done;
	int	nb;

	// nothing to copy
	if( buffersize == 0 ) 
		return 1;

	// Get rid of the ungetc character
	if( file->ungetc != EOF )
	{
		((char*)buffer)[0] = file->ungetc;
		buffersize--;
		file->ungetc = EOF;
		done = 1;
	}
	else {
		done = 0;
	}

	// first, we copy as many bytes as we can from "buff"
	if( file->buff_ind < file->buff_len )
	{
		count = file->buff_len - file->buff_ind;
		done += (buffersize > count) ? count : buffersize;
		memcpy(buffer, &file->buff[file->buff_ind], done);
		file->buff_ind += done;

		buffersize -= done;
		if( buffersize == 0 )
			return done;
	}

	// NOTE: at this point, the read buffer is always empty
	// we must take care to not read after the end of the file
	count = file->real_length - file->position;

	// if we have a lot of data to get, put them directly into "buffer"
	if( buffersize > sizeof( file->buff ) / 2 )
	{
		if( count > buffersize )
			count = buffersize;
		lseek( file->handle, file->offset + file->position, SEEK_SET );
		nb = read (file->handle, &((byte *)buffer)[done], count );

		if( nb > 0 )
		{
			done += nb;
			file->position += nb;
			// purge cached data
			FS_Purge( file );
		}
	}
	else
	{
		if (count > sizeof(file->buff))
			count = sizeof(file->buff);

		lseek( file->handle, file->offset + file->position, SEEK_SET );
		nb = read( file->handle, file->buff, count );

		if( nb > 0 )
		{
			file->buff_len = nb;
			file->position += nb;

			// copy the requested data in "buffer" (as much as we can)
			count = (buffersize > file->buff_len) ? file->buff_len : buffersize;
			memcpy( &((byte *)buffer)[done], file->buff, count );
			file->buff_ind = count;
			done += count;
		}
	}

	return done;
}

/*
====================
FS_Seek

Move the position index in a file
====================
*/
int FS_Seek( file_t *file, int64_t offset, int whence )
{
	// compute the file offset
	switch( whence )
	{
	case SEEK_CUR:
		offset += file->position - file->buff_len + file->buff_ind;
		break;
	case SEEK_SET:
		break;
	case SEEK_END:
		offset += file->real_length;
		break;
	default: 
		return -1;
	}
	
	if( offset < 0 || offset > file->real_length )
		return -1;

	// if we have the data in our read buffer, we don't need to actually seek
	if( file->position - file->buff_len <= offset && offset <= file->position )
	{
		file->buff_ind = offset + file->buff_len - file->position;
		return 0;
	}

	// Purge cached data
	FS_Purge( file );

	if( lseek( file->handle, file->offset + offset, SEEK_SET ) == -1 )
		return -1;
	file->position = offset;

	return 0;
}

/*
====================
FS_Tell

Give the current position in a file
====================
*/
size_t FS_Tell( file_t *file )
{
	if (!file) {
		return 0;
	}
	return file->position - file->buff_len + file->buff_ind;
}

/*
====================
FS_Getc

Get the next character of a file
====================
*/
int FS_Getc( file_t *file )
{
	char	c;

	if( FS_Read( file, &c, 1 ) != 1 )
		return EOF;

	return c;
}

/*
====================
FS_UnGetc

Put a character back into the read buffer (only supports one character!)
====================
*/
int FS_UnGetc( file_t *file, byte c )
{
	// If there's already a character waiting to be read
	if( file->ungetc != EOF )
		return EOF;

	file->ungetc = c;
	return c;
}

/*
====================
FS_Gets

Same as fgets
====================
*/
int FS_Gets( file_t *file, byte *string, size_t bufsize )
{
	int	c, end = 0;

	while( 1 )
	{
		c = FS_Getc( file );

		if( c == '\r' || c == '\n' || c < 0 )
			break;

		if( end < bufsize - 1 )
			string[end++] = c;
	}
	string[end] = 0;

	// remove \n following \r
	if( c == '\r' )
	{
		c = FS_Getc( file );

		if( c != '\n' )
			FS_UnGetc( file, (byte)c );
	}

	return c;
}

/*
====================
FS_Eof

indicates at reached end of file
====================
*/
bool FS_Eof( file_t *file )
{
	if( !file ) return true;
	return (( file->position - file->buff_len + file->buff_ind ) == file->real_length ) ? true : false;
}

/*
====================
FS_Purge

Erases any buffered input or output data
====================
*/
void FS_Purge( file_t *file )
{
	file->buff_len = 0;
	file->buff_ind = 0;
	file->ungetc = EOF;
}

/*
====================
FS_Write

Write "datasize" bytes into a file
====================
*/
int FS_Write( file_t *file, const void *data, size_t datasize )
{
	int	result;

	if( !file ) return 0;

	// if necessary, seek to the exact file position we're supposed to be
	if( file->buff_ind != file->buff_len )
		lseek( file->handle, file->buff_ind - file->buff_len, SEEK_CUR );

	// purge cached data
	FS_Purge( file );

	// write the buffer and update the position
	result = write( file->handle, data, datasize );
	file->position = lseek( file->handle, 0, SEEK_CUR );

	if( file->real_length < file->position )
		file->real_length = file->position;

	if( result < 0 )
		return 0;
	return result;
}

/*
============
FS_LoadFile

Filename are relative to the xash directory.
Always appends a 0 byte.
============
*/
byte *FS_LoadFile( const char *path, size_t *filesizeptr, bool gamedironly )
{
	file_t	*file;
	byte	*buf = NULL;
	size_t	filesize = 0;

	file = FS_Open( path, "rb", gamedironly );

	if( file )
	{
		filesize = file->real_length;
		buf = (byte *)Mem_Alloc( filesize + 1, C_FILESYSTEM );
		buf[filesize] = '\0';
		FS_Read( file, buf, filesize );
		FS_Close( file );
	}
	else
	{
		buf = W_LoadFile( path, &filesize, gamedironly );
	}

	if( filesizeptr )
		*filesizeptr = filesize;

	return buf;
}

/*
==================
FS_FileLength

return size of file in bytes
==================
*/
size_t FS_FileLength( file_t *f )
{
	if (!f) {
		return 0;
	}
	return f->real_length;
}

/*
=============================================================================

OTHERS PUBLIC FUNCTIONS

=============================================================================
*/
/*
==================
FS_FileExists

Look for a file in the packages and in the filesystem
==================
*/
bool FS_FileExists( const char *filename, bool gamedironly )
{
	if( FS_FindFile( filename, NULL, gamedironly ))
		return true;
	return false;
}

/*
==================
FS_FileTime

return time of creation file in seconds
==================
*/
int FS_FileTime( const char *filename, bool gamedironly )
{
	searchpath_t	*search;
	int		pack_ind;
	
	search = FS_FindFile( filename, &pack_ind, gamedironly );
	if( !search ) return -1; // doesn't exist

	if( search->pack ) // grab pack filetime
		return search->pack->filetime;
	else if( search->wad ) // grab wad filetime
		return search->wad->filetime;
	else if( pack_ind < 0 )
	{
		char	path [MAX_SYSPATH];

		// found in the filesystem?
		Q_sprintf( path, "%s%s", search->filename, filename );
		return COM_FileTime( path );
	}

	return -1; // doesn't exist
}

/*
===========
FS_Search

Allocate and fill a search structure with information on matching filenames.
===========
*/
search_t *FS_Search( const char *pattern, int caseinsensitive, int gamedironly )
{
	search_t		*search = NULL;
	searchpath_t	*searchpath;
	pack_t		*pak;
	wfile_t		*wad;
	int		i, basepathlength, numfiles, numchars;
	int		resultlistindex, dirlistindex;
	const char	*slash, *backslash, *colon, *separator;
	string		netpath, temp;
	stringlist_t	resultlist;
	stringlist_t	dirlist;
	char		*basepath;

	for( i = 0; pattern[i] == '.' || pattern[i] == ':' || pattern[i] == '/' || pattern[i] == '\\'; i++ );

	if( i > 0 )
	{
		MsgDev( D_INFO, "FS_Search: don't use punctuation at the beginning of a search pattern!\n");
		return NULL;
	}

	stringlistinit( &resultlist );
	stringlistinit( &dirlist );
	slash = Q_strrchr( pattern, '/' );
	backslash = Q_strrchr( pattern, '\\' );
	colon = Q_strrchr( pattern, ':' );
	separator = Q_max( slash, backslash );
	separator = Q_max( separator, colon );
	basepathlength = separator ? (separator + 1 - pattern) : 0;
	basepath = (char *)Mem_Alloc( basepathlength + 1, C_FILESYSTEM );
	if( basepathlength ) memcpy( basepath, pattern, basepathlength );
	basepath[basepathlength] = 0;

	// search through the path, one element at a time
	for( searchpath = fs_searchpaths; searchpath; searchpath = searchpath->next )
	{
		if( gamedironly && !FBitSet( searchpath->flags, FS_GAMEDIR_PATH ))
			continue;

		// is the element a pak file?
		if( searchpath->pack )
		{
			// look through all the pak file elements
			pak = searchpath->pack;
			for( i = 0; i < pak->numfiles; i++ )
			{
				Q_strncpy( temp, pak->files[i].name, sizeof( temp ));
				while( temp[0] )
				{
					if( matchpattern( temp, (char *)pattern, true ))
					{
						for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
						{
							if( !Q_strcmp( resultlist.strings[resultlistindex], temp ))
								break;
						}

						if( resultlistindex == resultlist.numstrings )
							stringlistappend( &resultlist, temp );
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
		else if( searchpath->wad )
		{
			string	wadpattern, wadname, temp2;
			int	type = W_TypeFromExt( pattern );
			qboolean anywadname = true;
			string wadfolder;

			// quick reject by filetype
			if( type == TYP_NONE ) 
				continue;

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
			COM_FileBase( searchpath->wad->filename, temp2 );
			COM_DefaultExtension( temp2, ".wad" );

			// quick reject by wadname
			if( !anywadname && Q_stricmp( wadname, temp2 ))
				continue;

			// look through all the wad file elements
			wad = searchpath->wad;

			for( i = 0; i < wad->numlumps; i++ )
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
						for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
						{
							if( !Q_strcmp( resultlist.strings[resultlistindex], temp ))
								break;
						}

						if( resultlistindex == resultlist.numstrings )
						{
							// build path: wadname/lumpname.ext
							Q_snprintf( temp2, sizeof(temp2), "%s/%s", wadfolder, temp );
							COM_DefaultExtension( temp2, va(".%s", W_ExtFromType( wad->lumps[i].type )));
							stringlistappend( &resultlist, temp2 );
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
		else
		{
			// get a directory listing and look at each name
			Q_sprintf( netpath, "%s%s", searchpath->filename, basepath );
			stringlistinit( &dirlist );
			listdirectory( &dirlist, netpath );

			for( dirlistindex = 0; dirlistindex < dirlist.numstrings; dirlistindex++ )
			{
				Q_sprintf( temp, "%s%s", basepath, dirlist.strings[dirlistindex] );

				if( matchpattern( temp, (char *)pattern, true ))
				{
					for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
					{
						if( !Q_strcmp( resultlist.strings[resultlistindex], temp ))
							break;
					}

					if( resultlistindex == resultlist.numstrings )
						stringlistappend( &resultlist, temp );
				}
			}

			stringlistfreecontents( &dirlist );
		}
	}

	if( resultlist.numstrings )
	{
		stringlistsort( &resultlist );
		numfiles = resultlist.numstrings;
		numchars = 0;

		for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
			numchars += (int)Q_strlen( resultlist.strings[resultlistindex]) + 1;
		search = (search_t *)Mem_Alloc( sizeof( search_t ) + numchars + numfiles * sizeof( char* ), C_FILESYSTEM );
		search->filenames = (char **)((char *)search + sizeof( search_t ));
		search->filenamesbuffer = (char *)((char *)search + sizeof( search_t ) + numfiles * sizeof( char* ));
		search->numfilenames = (int)numfiles;
		numfiles = numchars = 0;

		for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
		{
			size_t	textlen;

			search->filenames[numfiles] = search->filenamesbuffer + numchars;
			textlen = Q_strlen(resultlist.strings[resultlistindex]) + 1;
			memcpy( search->filenames[numfiles], resultlist.strings[resultlistindex], textlen );
			numfiles++;
			numchars += (int)textlen;
		}
	}

	stringlistfreecontents( &resultlist );

	Mem_Free( basepath, C_FILESYSTEM );

	return search;
}

/*
=============================================================================

FILESYSTEM IMPLEMENTATION

=============================================================================
*/
/*
===========
W_LoadFile

loading lump into the tmp buffer
===========
*/
static byte *W_LoadFile( const char *path, size_t *lumpsizeptr, bool gamedironly )
{
	searchpath_t	*search;
	int		index;

	search = FS_FindFile( path, &index, gamedironly );
	if( search && search->wad )
		return W_ReadLump( search->wad, &search->wad->lumps[index], lumpsizeptr ); 
	return NULL;
}
