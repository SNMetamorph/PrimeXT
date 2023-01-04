/*
filesystem.cpp - simple version of game engine filesystem for tools
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

#include "port.h"
#if XASH_WIN32
#include <windows.h>
#include <direct.h>
//#include <io.h>
#else
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#define O_BINARY 0
#endif

#include <fcntl.h>
#include <stdio.h>
#include "conprint.h"
#include "cmdlib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "mathlib.h"
#include <sys/stat.h>

/*
=============================================================================

FILEMATCH COMMON SYSTEM

=============================================================================
*/
int matchpattern( const char *str, const char *cmp, bool caseinsensitive )
{
	int	c1, c2;

	while( *cmp )
	{
		switch( *cmp )
		{
		case 0:   return 1; // end of pattern
		case '?': // match any single character
			if( *str == 0 || *str == '/' || *str == '\\' || *str == ':' )
				return 0; // no match
			str++;
			cmp++;
			break;
		case '*': // match anything until following string
			if( !*str ) return 1; // match
			cmp++;
			while( *str )
			{
				if( *str == '/' || *str == '\\' || *str == ':' )
					break;
				// see if pattern matches at this offset
				if( matchpattern( str, cmp, caseinsensitive ))
					return 1;
				// nope, advance to next offset
				str++;
			}
			break;
		default:
			if( *str != *cmp )
			{
				if( !caseinsensitive )
					return 0; // no match
				c1 = Q_tolower( *str );
				c2 = Q_tolower( *cmp );
				if( c1 != c2 ) return 0; // no match
			}

			str++;
			cmp++;
			break;
		}
	}

	// reached end of pattern but not end of input?
	return (*str) ? 0 : 1;
}

void stringlistinit( stringlist_t *list )
{
	memset( list, 0, sizeof( *list ));
}

void stringlistfreecontents( stringlist_t *list )
{
	int	i;

	for( i = 0; i < list->numstrings; i++ )
	{
		if( list->strings[i] )
			Mem_Free( list->strings[i], C_STRING );
		list->strings[i] = NULL;
	}

	if( list->strings )
		Mem_Free( list->strings, C_STRING );

	list->numstrings = 0;
	list->maxstrings = 0;
	list->strings = NULL;
}

void stringlistappend( stringlist_t *list, char *text )
{
	size_t	textlen;
	char	**oldstrings;

	if( !Q_stricmp( text, "." ) || !Q_stricmp( text, ".." ))
		return; // ignore the virtual directories

	if( list->numstrings >= list->maxstrings )
	{
		oldstrings = list->strings;
		list->maxstrings += 4096;
		list->strings = (char **)Mem_Alloc( list->maxstrings * sizeof( *list->strings ), C_STRING );
		if( list->numstrings ) memcpy( list->strings, oldstrings, list->numstrings * sizeof( *list->strings ));
		if( oldstrings ) Mem_Free( oldstrings, C_STRING );
	}

	textlen = Q_strlen( text ) + 1;
	list->strings[list->numstrings] = (char *)Mem_Alloc( textlen, C_STRING );
	memcpy( list->strings[list->numstrings], text, textlen );
	list->numstrings++;
}

void stringlistsort( stringlist_t *list )
{
	char	*temp;
	int	i, j;

	// this is a selection sort (finds the best entry for each slot)
	for( i = 0; i < list->numstrings - 1; i++ )
	{
		for( j = i + 1; j < list->numstrings; j++ )
		{
			if( Q_strcmp( list->strings[i], list->strings[j] ) > 0 )
			{
				temp = list->strings[i];
				list->strings[i] = list->strings[j];
				list->strings[j] = temp;
			}
		}
	}
}

void listdirectory( stringlist_s *list, const char *path, bool lowercase )
{
#if XASH_WIN32
	char pattern[4096];
	struct _finddata_t n_file;
	intptr_t hFile;
#else
	DIR *dir;
	struct dirent *entry;
#endif

#if XASH_WIN32
	Q_snprintf( pattern, sizeof( pattern ), "%s*", path );

	// ask for the directory listing handle
	hFile = _findfirst( pattern, &n_file );
	if (hFile == -1) 
		return;

	// start a new chain with the the first name
	stringlistappend( list, n_file.name );
	// iterate through the directory
	while (_findnext(hFile, &n_file) == 0) {
		stringlistappend(list, n_file.name);
	}
	_findclose( hFile );
#else
	if( !( dir = opendir( path ) ) )
		return;

	// iterate through the directory
	while( ( entry = readdir( dir ) ))
		stringlistappend( list, entry->d_name );
	closedir( dir );
#endif

	// convert names to lowercase because windows doesn't care, but pattern matching code often does
	if (lowercase)
	{
		for (int i = 0; i < list->numstrings; i++)
		{
			for (char *c = (char *)list->strings[i]; *c; c++)
			{
				if (*c >= 'A' && *c <= 'Z')
					*c += 'a' - 'A';
			}
		}
	}
}
/*
=============================================================================

FILESYSTEM PUBLIC BASE FUNCTIONS

=============================================================================
*/
/*
===========
FS_Search

Allocate and fill a search structure with information on matching filenames.
===========
*/
search_t *COM_Search( const char *pattern, int caseinsensitive, wfile_t *source_wad )
{
	search_t		*search = NULL;
	int		i, basepathlength, numfiles, numchars;
	int		resultlistindex, dirlistindex;
	const char	*slash, *backslash, *colon, *separator;
	char		temp[1024], root[1024];
	stringlist_t	resultlist, dirlist;
	char		*basepath;

	for( i = 0; pattern[i] == '.' || pattern[i] == ':' || pattern[i] == '/' || pattern[i] == '\\'; i++ );

	if( i > 0 )
	{
		MsgDev( D_ERROR, "COM_Search: don't use punctuation at the beginning of a search pattern!\n" );
		return NULL;
	}

	Q_getwd(root, sizeof(root));
	if (root[0] == '\0')
	{
		MsgDev(D_ERROR, "couldn't determine current directory\n");
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
	basepath = (char *)Mem_Alloc( basepathlength + 1 );
	if( basepathlength ) memcpy( basepath, pattern, basepathlength );
	basepath[basepathlength] = 0;

#ifndef IGNORE_SEARCH_IN_WADS
	W_SearchForFile( source_wad, pattern, &resultlist );
#endif
	// get a directory listing and look at each name
	stringlistinit( &dirlist );
	listdirectory( &dirlist, basepath );

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

	if( resultlist.numstrings )
	{
		stringlistsort( &resultlist );
		numfiles = resultlist.numstrings;
		numchars = 0;

		for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
			numchars += (int)Q_strlen( resultlist.strings[resultlistindex]) + 1;

		search = (search_t *)Mem_Alloc( sizeof( search_t ) + numchars + numfiles * sizeof( char* ));
		search->filenames = (char **)((char *)search + sizeof( search_t ));
		search->filenamesbuffer = (char *)((char *)search + sizeof( search_t ) + numfiles * sizeof( char* ));
		search->numfilenames = (int)numfiles;
		numfiles = numchars = 0;

		for( resultlistindex = 0; resultlistindex < resultlist.numstrings; resultlistindex++ )
		{
			search->filenames[numfiles] = search->filenamesbuffer + numchars;
			size_t textlen = Q_strlen(resultlist.strings[resultlistindex]) + 1;
			memcpy( search->filenames[numfiles], resultlist.strings[resultlistindex], textlen );
			numchars += (int)textlen;
			numfiles++;
		}
	}

	stringlistfreecontents( &resultlist );
	Mem_Free( basepath );

	return search;
}

byte *COM_LoadFile( const char *filepath, size_t *filesize, bool safe )
{
	int	handle;
	int	size;
	unsigned char *buf;

	handle = open( filepath, O_RDONLY|O_BINARY, 0666 );

	if( filesize ) *filesize = 0;
	if( handle < 0 )
	{
		if( safe ) COM_FatalError( "Couldn't open %s\n", filepath );
		return NULL;
	}

	size = lseek( handle, 0, SEEK_END );
	lseek( handle, 0, SEEK_SET );

	buf = (unsigned char *)Mem_Alloc( size + 1, C_FILESYSTEM );
	buf[size] = '\0';
	read( handle, (unsigned char *)buf, size );
	close( handle );

	if( filesize ) *filesize = size;
	return buf;	
}

bool COM_SaveFile( const char *filepath, void *buffer, size_t filesize, bool safe )
{
	int	handle;
	int	size;

	if( buffer == NULL || filesize <= 0 )
		return false;

	COM_CreatePath( (char *)filepath );

	handle = open( filepath, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, 0666 );
	if( handle < 0 )
	{
		if( safe ) COM_FatalError( "Couldn't write %s\n", filepath );
		return false;
	}

	size = write( handle, buffer, filesize );
	close( handle );

	if( size < 0 )
		return false;
	return true;
}

/*
==================
COM_CreatePath
==================
*/
void COM_CreatePath( char *path )
{
	char *ofs, save;

	for( ofs = path + 1; *ofs; ofs++ )
	{
		if( *ofs == '/' || *ofs == '\\' )
		{
			// create the directory
			save = *ofs;
			*ofs = 0;
#if XASH_WIN32
			mkdir( path );
#else
			mkdir( path, 0755);
#endif
			*ofs = save;
		}
	}
}

/*
==================
COM_FileExists
==================
*/
bool COM_FileExists( const char *path )
{
#if XASH_WIN32
	int desc;

	if(( desc = open( path, O_RDONLY|O_BINARY, 0666 )) < 0 )
		return false;

	close( desc );
	return true;
#else
	int ret;
	struct stat buf;

	ret = stat( path, &buf );

	if( ret < 0 )
		return false;

	return S_ISREG( buf.st_mode );
#endif
}

/*
============
COM_FileWithoutPath
============
*/
const char *COM_FileWithoutPath( const char *in )
{
	const char *separator, *backslash, *colon;

	separator = Q_strrchr( in, '/' );
	backslash = Q_strrchr( in, '\\' );

	if( !separator || separator < backslash )
		separator = backslash;

	colon = Q_strrchr( in, ':' );

	if( !separator || separator < colon )
		separator = colon;

	return separator ? separator + 1 : in;
}

/*
============
COM_ExtractFilePath
============
*/
void COM_ExtractFilePath( const char *path, char *dest )
{
	const char *src = path + Q_strlen( path ) - 1;

	// back up until a \ or the start
	while( src != path && !(*(src - 1) == '\\' || *(src - 1) == '/' ))
		src--;

	if( src != path )
	{
		memcpy( dest, path, src - path );
		dest[src - path - 1] = 0; // cutoff backslash
	}
	else Q_strcpy( dest, "" ); // file without path
}

/*
============
COM_FileExtension
============
*/
const char *COM_FileExtension( const char *in )
{
	const char *separator, *backslash, *colon, *dot;

	separator = Q_strrchr( in, '/' );
	backslash = Q_strrchr( in, '\\' );

	if( !separator || separator < backslash )
		separator = backslash;

	colon = Q_strrchr( in, ':' );

	if( !separator || separator < colon )
		separator = colon;

	dot = Q_strrchr( in, '.' );

	if( dot == NULL || ( separator && ( dot < separator )))
		return "";

	return dot + 1;
}

/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension(char *path, const char *extension)
{
	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	const char *src = path + Q_strlen(path) - 1;
	while (*src != '/' && *src != '\\' && src != path)
	{
		if (*src == '.') {
			return; // it has an extension, therefore we don't need to append it
		}
		src--;
	}
	Q_strcat(path, extension);
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension( char *path )
{
	size_t	length;

	length = Q_strlen( path ) - 1;

	while( length > 0 && path[length] != '.' )
	{
		length--;

		if( path[length] == '/' || path[length] == '\\' || path[length] == ':' )
			return; // no extension
	}

	if( length ) path[length] = 0;
}

/*
==================
COM_ReplaceExtension
==================
*/
void COM_ReplaceExtension( char *path, const char *extension )
{
	COM_StripExtension( path );
	COM_DefaultExtension( path, extension );
}

/*
============
COM_FileBase

Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
============
*/
void COM_FileBase( const char *in, char *out )
{
	int	len, start, end;

	len = Q_strlen( in );
	if( !len ) return;
	
	// scan backward for '.'
	end = len - 1;

	while( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;
	
	if( in[end] != '.' )
		end = len-1; // no '.', copy to end
	else end--; // found ',', copy to left of '.'

	// scan backward for '/'
	start = len - 1;

	while( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if( start < 0 || ( in[start] != '/' && in[start] != '\\' ))
		start = 0;
	else start++;

	// length of new sting
	len = end - start + 1;

	// Copy partial string
	Q_strncpy( out, &in[start], len + 1 );
	out[len] = 0;
}

/*
==================
COM_FolderExists
==================
*/
bool COM_FolderExists( const char *path )
{
#if XASH_WIN32
	DWORD	dwFlags = GetFileAttributes( path );

	return ( dwFlags != -1 ) && ( dwFlags & FILE_ATTRIBUTE_DIRECTORY );
#elif XASH_POSIX
	DIR *dir = opendir( path );

	if( dir )
	{
		closedir( dir );
		return true;
	}
	else if( (errno == ENOENT) || (errno == ENOTDIR) )
	{
		return false;
	}
	else
	{
		MsgDev( D_ERROR, "FS_SysFolderExists: problem while opening dir: %s\n", strerror( errno ) );
		return false;
	}
#else
#error "Implement me"
#endif
}

/*
====================
COM_FileTime

Internal function used to determine filetime
====================
*/
int COM_FileTime( const char *filename )
{
	struct stat buf;
	
	if( stat( filename, &buf ) == -1 )
		return -1;

	return buf.st_mtime;
}

/*
=============================================================================

			OBSOLETE FUNCTIONS

=============================================================================
*/
int SafeOpenWrite( const char *filename )
{
	int	handle = open( filename, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0666 );
	if (handle < 0) {
		COM_FatalError("couldn't open %s\n", filename);
	}
	return handle;
}

int SafeOpenRead( const char *filename )
{
	int	handle = open( filename, O_RDONLY|O_BINARY, 0666 );
	if (handle < 0) {
		COM_FatalError("couldn't open %s\n", filename);
	}
	return handle;
}

void SafeReadExt( int handle, void *buffer, int count, const char *file, const int line )
{
	int	read_count = read( handle, buffer, count );
	if (read_count != count) {
		COM_FatalError("file read failure ( %i != %i ) at %s:%i\n", read_count, count, file, line);
	}
}

void SafeWriteExt( int handle, void *buffer, int count, const char *file, const int line )
{
	int	write_count = write( handle, buffer, count );
	if (write_count != count) {
		COM_FatalError("file write failure ( %i != %i ) at %s:%i\n", write_count, count, file, line);
	}
}
