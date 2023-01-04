/*
filesystem.h - simple version of game engine filesystem for tools
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

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "wadfile.h"
#include <time.h>
#include <stdint.h>

#define FILE_BUFF_SIZE	(65535)
#define WAD3_NAMELEN	16
#define HINT_NAMELEN	5	// e.g. _mask, _norm
#define MAX_FILES_IN_WAD	65535	// real limit as above <2Gb size not a lumpcount

// replace lumps in a wad
#define REP_IGNORE		0
#define REP_NORMAL		1
#define REP_FORCE		2

// PAK errors
#define PAK_LOAD_OK			0
#define PAK_LOAD_COULDNT_OPEN		1
#define PAK_LOAD_BAD_HEADER		2
#define PAK_LOAD_BAD_FOLDERS		3
#define PAK_LOAD_TOO_MANY_FILES	4
#define PAK_LOAD_NO_FILES		5
#define PAK_LOAD_CORRUPTED		6

// WAD errors
#define WAD_LOAD_OK			0
#define WAD_LOAD_COULDNT_OPEN		1
#define WAD_LOAD_BAD_HEADER		2
#define WAD_LOAD_BAD_FOLDERS		3
#define WAD_LOAD_TOO_MANY_FILES	4
#define WAD_LOAD_NO_FILES		5
#define WAD_LOAD_CORRUPTED		6

typedef struct stringlist_s
{
	// maxstrings changes as needed, causing reallocation of strings[] array
	int	maxstrings;
	int	numstrings;
	char	**strings;
} stringlist_t;

typedef struct
{
	int	numfilenames;
	char	**filenames;
	char	*filenamesbuffer;
} search_t;

typedef struct wadtype_s
{
	char	*ext;
	int		type;
} wadtype_t;

typedef struct vfile_s
{
	byte		*buff;
	size_t		buffsize;
	size_t		length;
	size_t		offset;
} vfile_t;

#pragma pack(push, 1)
typedef struct
{
	int		ident;		// should be WAD3
	int		numlumps;		// num files
	int		infotableofs;	// LUT offset
} dwadinfo_t;

typedef struct
{
	int		filepos;	// file offset in WAD
	int		disksize;	// compressed or uncompressed
	int		size;		// uncompressed
	int8_t	type;
	int8_t	attribs;	// file attribs
	int8_t	img_type;	// IMG_*
	int8_t	pad;
	char	name[WAD3_NAMELEN];		// must be null terminated
} dlumpinfo_t;
#pragma pack(pop)

struct wfile_t
{
	char		filename[256];
	int		infotableofs;
	int		numlumps;
	int		mode;
#ifdef ALLOW_WADS_IN_PACKS
	file_t		*handle;
#else
	int		handle;
#endif
	time_t		filetime;	
	dlumpinfo_t	*lumps;
};

extern const wadtype_t wad_hints[];

search_t *COM_Search( const char *pattern, int caseinsensitive, wfile_t *source_wad = NULL );
search_t *FS_Search( const char *pattern, int caseinsensitive, int gamedironly );
byte *COM_LoadFile( const char *filepath, size_t *filesize, bool safe = true );
bool COM_SaveFile( const char *filepath, void *buffer, size_t filesize, bool safe = true );
int COM_FileTime( const char *filename );
bool COM_FolderExists( const char *path );
bool COM_FileExists( const char *path );
void COM_CreatePath( char *path );
void COM_FileBase( const char *in, char *out );
const char *COM_FileExtension( const char *in );
void COM_DefaultExtension( char *path, const char *extension );
void COM_ExtractFilePath( const char *path, char *dest );
void COM_ReplaceExtension( char *path, const char *extension );
const char *COM_FileWithoutPath( const char *in );
void COM_StripExtension( char *path );

// virtual filesystem
vfile_t *VFS_Create( const byte *buffer = NULL, size_t buffsize = 0 );
size_t VFS_Read( vfile_t *file, void *buffer, size_t buffersize );
size_t VFS_Write( vfile_t *file, const void *buf, size_t size );
size_t VFS_Insert( vfile_t *file, const void *buf, size_t size );
byte *VFS_GetBuffer( vfile_t *file );
size_t VFS_GetSize( vfile_t *file );
size_t VFS_Tell( vfile_t *file );
bool VFS_Eof( vfile_t *file );
size_t VFS_Print( vfile_t *file, const char *msg );
size_t VFS_IPrint( vfile_t *file, const char *msg );
size_t VFS_VPrintf( vfile_t *file, const char *format, va_list ap );
size_t VFS_VIPrintf( vfile_t *file, const char *format, va_list ap );
size_t VFS_Printf( vfile_t *file, const char *format, ... );
size_t VFS_IPrintf( vfile_t *file, const char *format, ... );
int VFS_Seek( vfile_t *file, size_t offset, int whence );
char VFS_Getc( vfile_t *file );
int VFS_Gets( vfile_t* file, byte *string, size_t bufsize );
void VFS_Close( vfile_t *file );

// wadfile routines
wfile_t *W_Open( const char *filename, const char *mode, int *error = NULL, bool ext_path = true );
byte *W_ReadLump( wfile_t *wad, dlumpinfo_t *lump, size_t *lumpsizeptr );
byte *W_LoadLump( wfile_t *wad, const char *lumpname, size_t *lumpsizeptr, int type );
int W_SaveLump( wfile_t *wad, const char *lump, const void *data, size_t datasize, int type, char attribs = ATTR_NONE );
void W_SearchForFile( wfile_t *wad, const char *pattern, stringlist_t *resultlist );
dlumpinfo_t *W_FindLump( wfile_t *wad, const char *name, int matchtype );
dlumpinfo_t *W_FindMiptex( wfile_t *wad, const char *name );
dlumpinfo_t *W_FindLmptex( wfile_t *wad, const char *name );
int W_TypeFromExt( const char *lumpname );
const char *W_ExtFromType( int lumptype );
char W_HintFromSuf( const char *lumpname );
int W_GetHandle( wfile_t *wad );
void W_Close( wfile_t *wad );

// compare routines
int matchpattern( const char *str, const char *cmp, bool caseinsensitive );

// search routines
void stringlistinit( stringlist_t *list );
void stringlistfreecontents( stringlist_t *list );
void stringlistappend( stringlist_t *list, char *text );
void stringlistsort( stringlist_t *list );
void listdirectory( stringlist_t *list, const char *path, bool tolower = false );

// replace lumps protect
void SetReplaceLevel( int level );
int GetReplaceLevel( void );

#endif//FILESYSTEM_H