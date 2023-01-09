/*
cmdlib.h - system functions, backend
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

// cmdlib.h

#ifndef CMDLIB_H
#define CMDLIB_H

#include <basetypes.h>
#include <cstddef>
#include "conprint.h"

// bit routines
#define BIT( n )			(1<<( n ))
#define SetBits( iBitVector, bits )	((iBitVector) = (iBitVector) | (bits))
#define ClearBits( iBitVector, bits )	((iBitVector) = (iBitVector) & ~(bits))
#define FBitSet( iBitVector, bit )	((iBitVector) & (bit))

#define bound( min, num, max )	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define ASSERT( exp )		if(!( exp )) COM_Assert( "%s:%i\n", __FILE__, __LINE__ )

NO_RETURN void COM_Assert( const char *error, ... );
NO_RETURN void COM_FatalError( const char *error, ... );
void COM_InitCmdlib( char **argv, int argc );
bool COM_GetLastParmExt( char *out, size_t size );
#define COM_GetLastParm( a )	COM_GetLastParmExt( a, sizeof( a ))
int COM_CheckParm( const char *parm );
bool COM_GetParmExt( char *parm, char *out, size_t size );
#define COM_GetParm( a, b )	COM_GetParmExt( a, b, sizeof( b ))
int COM_CheckString( const char *string );
void Q_getwd( char *out, size_t size );
char *COM_ExpandArg( const char *path );	// from scripts
void COM_FixSlashes( char *pname );
const char *COM_FindLastSlashEntry(const char *text);
double I_FloatTime( void );
void COM_SetClipboardText(const char *text);

// normal file
typedef struct file_s	file_t;

int		SafeOpenWrite( const char *filename );
int		SafeOpenRead( const char *filename );
void	SafeReadExt( int handle, void *buffer, int count, const char *file, const int line );
void	SafeWriteExt( int handle, void *buffer, int count, const char *file, const int line );

#define SafeRead( file, buffer, count )		SafeReadExt( file, buffer, count, __FILE__, __LINE__ )
#define SafeWrite( file, buffer, count )	SafeWriteExt( file, buffer, count, __FILE__, __LINE__ )

#define IMAGE_EXISTS( path )			( FS_FileExists( va( "%s.tga", path ), false ) || FS_FileExists( va( "%s.dds", path ), false ))

//
// zone.cpp
//
enum
{
	C_COMMON = 0,
	C_TEMPORARY,
	C_SAFEALLOC,
	C_FILESYSTEM,
	C_WINDING,
	C_BRUSHSIDE,
	C_BSPBRUSH,
	C_LEAFNODE,
	C_SURFACE,
	C_BSPTREE,
	C_PORTAL,
	C_STRING,
	C_EPAIR,
	C_PATCH,
	C_MAXSTAT,
};

void *Mem_Alloc( size_t sizeInBytes, unsigned int target = C_COMMON );
void *Mem_Realloc( void *ptr, size_t sizeInBytes, unsigned int target = C_COMMON );
void Mem_Free( void *ptr, unsigned int target = C_COMMON );
size_t Mem_Size( void *ptr );
void Mem_Check( void );
void Mem_Peak( void );

//
// basefs.c
//
void FS_Init(const char *source);
byte *FS_LoadFile(const char *path, size_t *filesizeptr, bool gamedironly);
bool FS_FileExists(const char *filename, bool gamedironly);
void FS_Shutdown(void);
file_t *FS_Open(const char *filepath, const char *mode, bool gamedironly);
size_t FS_Read(file_t *file, void *buffer, size_t buffersize);
int FS_Write(file_t *file, const void *data, size_t datasize);
size_t FS_Tell(file_t *file);
int FS_Seek(file_t *file, int64_t offset, int whence);
int FS_Gets(file_t *file, byte *string, size_t bufsize);
void FS_AllowDirectPaths(bool enable);
size_t FS_FileLength(file_t *f);
int FS_Close(file_t *file);

#endif
