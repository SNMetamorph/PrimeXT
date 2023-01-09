/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.

This file is part of Quake 2 Tools source code.

Quake 2 Tools source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake 2 Tools source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake 2 Tools source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "port.h"
#if XASH_WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <winbase.h>
#else
#include <time.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include "stringlib.h"
#include "cmdlib.h"
#include <stdarg.h>
#include <stdlib.h>

static char	**com_argv;
static int	com_argc = 0;

/*
============
COM_InitCmdlib

must been initialized first
============
*/
void COM_InitCmdlib( char **argv, int argc )
{
	com_argv = argv;
	com_argc = argc;
}

/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm( const char *parm )
{
	for( int i = 1; i < com_argc; i++ )
	{
		if( !com_argv[i] ) continue;
		if( !Q_stricmp( parm, com_argv[i] ))
			return i;
	}

	return 0;
}

/*
================
COM_GetParmExt

Returns the argument for specified parm
================
*/
bool COM_GetParmExt( char *parm, char *out, size_t size )
{
	int	argc = COM_CheckParm( parm );

	if( !argc || !out || !com_argv[argc + 1] )
		return false;

	Q_strncpy( out, com_argv[argc+1], size );

	return true;
}

/*
============
COM_GetLastParm

get last parm from commandline
============
*/
bool COM_GetLastParmExt( char *out, size_t size )
{
	// NOTE: all the tools with single input file
	// have similar format of cmdline syntax like this:
	// toolname.exe -arg -parm 1 -arg files/folder/myfile.ext
	// so path to filename is always last in args list even
	// if some other args contain errors we can grab it

	if( com_argc <= 1 || size < 0 ) return false;

	Q_strncpy( out, com_argv[com_argc - 1], size );
	return true;
}

/*
============
COM_FatalError

Stop the app with error
============
*/
NO_RETURN void COM_FatalError( const char *error, ... )
{
	char	message[8192];
	va_list	argptr;

	va_start( argptr, error );
	vsnprintf( message, sizeof( message ), error, argptr );
	va_end( argptr );

	Msg( "^1Fatal Error:^7 %s", message );
	exit( 1 );
}

/*
============
COM_Assert

assertation
============
*/
NO_RETURN void COM_Assert( const char *error, ... )
{
	char	message[8192];
	va_list	argptr;

	va_start( argptr, error );
	vsnprintf( message, sizeof( message ), error, argptr );
	va_end( argptr );

	Msg( "^1assert failed at:^7 %s", message );
	exit( 1 );
}

void Q_getwd( char *out, size_t size )
{
	getcwd( out, size );
#if XASH_WIN32
	Q_strncat( out, "\\", size );
#else
	Q_strncat( out, "/", size );
#endif
}

/*
============
COM_Error

Legacy from old cmdlib
============
*/
char *COM_ExpandArg( const char *path )
{
	static char	full[1024];

	if( path[0] != '/' && path[0] != '\\' && path[1] != ':' )
	{
		Q_getwd( full, sizeof( full ));
		Q_strncat( full, path, sizeof( full ));
	}
	else Q_strncpy( full, path, sizeof( full ));

	return full;
}

/*
============
COM_FixSlashes

Changes all '/' characters into '\' characters, in place.
============
*/
void COM_FixSlashes( char *pname )
{
	while( *pname )
	{
		if( *pname == '\\' )
			*pname = '/';
		pname++;
	}
}

/*
============
COM_FindLastSlashEntry

============
*/
const char *COM_FindLastSlashEntry(const char *text)
{
	size_t length = strlen(text);
	size_t i = length - 1;
	while (length > 0)
	{
		if (text[i] == '\\' || text[i] == '/') {
			return text + i;
		}
		if (i == 0) {
			break;
		}
		else {
			i--;
		}
	}
	return nullptr;
}

/*
=============
COM_CheckString

=============
*/
int COM_CheckString( const char *string )
{
	if( !string || (byte)*string <= ' ' )
		return 0;
	return 1;
}

/*
================
I_FloatTime

g-cont. the prefix 'I' was come from Doom code heh
================
*/

double GAME_EXPORT I_FloatTime( void )
{
#if XASH_WIN32
	static LARGE_INTEGER g_PerformanceFrequency;
	static LARGE_INTEGER g_ClockStart;
	LARGE_INTEGER CurrentTime;

	if( !g_PerformanceFrequency.QuadPart )
	{
		QueryPerformanceFrequency( &g_PerformanceFrequency );
		QueryPerformanceCounter( &g_ClockStart );
	}

	QueryPerformanceCounter( &CurrentTime );
	return (double)( CurrentTime.QuadPart - g_ClockStart.QuadPart ) / (double)( g_PerformanceFrequency.QuadPart );
#elif XASH_LINUX
	static int64 g_PerformanceFrequency;
	static int64 g_ClockStart;
	int64 CurrentTime;
	struct timespec ts;

	if( !g_PerformanceFrequency )
	{
		struct timespec res;
		if( !clock_getres(CLOCK_MONOTONIC, &res) )
			g_PerformanceFrequency = 1000000000LL/res.tv_nsec;
	}
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double) ts.tv_sec + (double) ts.tv_nsec/1000000000.0;
#else
#error "Implement me!"
#endif
}

void COM_SetClipboardText(const char *text)
{
#if XASH_WIN32
	size_t length = Q_strlen(text);
	LPTSTR clipboardString = nullptr;
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, length + 1);

	clipboardString = reinterpret_cast<LPTSTR>(GlobalLock(hMem));
	memcpy(clipboardString, text, length);
	clipboardString[length] = '\0';
	GlobalUnlock(hMem);

	OpenClipboard(NULL);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, hMem);
	CloseClipboard();
#else
	// TODO implement this for other platforms
#endif
}

