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

// scriplib.c

#include "cmdlib.h"
#include "mathlib.h"
#include "scriplib.h"
#include "stringlib.h"
#include "filesystem.h"
#include <stdarg.h>
#include <windows.h>

/*
=============================================================================

			PARSING STUFF

=============================================================================
*/
#define MAX_INCLUDES	16

typedef struct
{
	string		filename;
	const char	*buffer;
	const char	*script_p;
	const char	*end_p;
	int		line;
	bool		separate_stack;
} script_t;

script_t	scriptstack[MAX_INCLUDES];
script_t	*script;
int	scriptline;
int	oldscriptline;
int	scriptdepth;

char	token[MAXTOKEN];
char	g_TXcommand;	// QuArK 'TX' comment
int	g_DXspecial;	// Doom2Gold 'DX' comment

bool	endofscript;
bool	tokenready; // only true if UnGetToken was just called

/*
==============
AddScriptToStack
==============
*/
void AddScriptToStack( const char *filename )
{
	size_t	size;

	script++;
	if( script == &scriptstack[MAX_INCLUDES] )
		COM_FatalError( "script file exceeded MAX_INCLUDES\n" );
	Q_strcpy( script->filename, filename );

	script->buffer = (char *)COM_LoadFile( script->filename, &size, true );

	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;
	script->separate_stack = false;
}

/*
==============
IncludeScriptFile
==============
*/
#ifdef ALLOW_WADS_IN_PACKS
void IncludeScriptFile( const char *filename )
{
	size_t	size;

	script++;
	if( script == &scriptstack[MAX_INCLUDES] )
		COM_FatalError( "script file exceeded MAX_INCLUDES\n" );
	Q_strcpy( script->filename, filename );
	MsgDev( D_REPORT, "entering the script %s\n", script->filename );
	script->buffer = (char *)FS_LoadFile( script->filename, &size, false );

	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;
	script->separate_stack = true;
}
#endif

/*
==============
LoadScriptFile
==============
*/
void LoadScriptFile( const char *filename )
{
	script = scriptstack;
	AddScriptToStack( filename );

	endofscript = false;
	tokenready = false;
}


/*
==============
ParseFromMemory
==============
*/
void ParseFromMemory( const char *buffer, size_t size )
{
	script = scriptstack;
	script++;

	if( script == &scriptstack[MAX_INCLUDES] )
		COM_FatalError( "script file exceeded MAX_INCLUDES\n" );
	Q_strcpy( script->filename, "memory buffer" );

	script->buffer = buffer;
	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;
	script->separate_stack = false;

	endofscript = false;
	tokenready = false;
}

/*
==============
UnGetToken

Signals that the current token was not used, and should be reported
for the next GetToken.  Note that

GetToken (true);
UnGetToken ();
GetToken (false);

could cross a line boundary.
==============
*/
void UnGetToken( void )
{
	tokenready = true;
}


bool EndOfScript( bool crossline )
{
	bool	finished = script->separate_stack;

	if( !crossline )
		COM_FatalError( "line %i is incomplete\n", scriptline );

	if( !Q_strcmp( script->filename, "memory buffer" ))
	{
		endofscript = true;
		return false;
	}

	Mem_Free( (char *)script->buffer, C_FILESYSTEM );

	if( script == scriptstack + 1 )
	{
		endofscript = true;
		return false;
	}

	MsgDev( D_REPORT, "leave the script %s\n", script->filename );
	script--;
	scriptline = script->line;

	// don't iterpret included files as 'solid' script
	if( finished )
		return false;

	return GetToken( crossline );
}

/*
==============
GetToken
==============
*/
bool GetToken( bool crossline )
{
	char *token_p;

	if( tokenready )
	{
		// is a token already waiting?
		tokenready = false;
		return true;
	}

	if( script->script_p >= script->end_p )
		return EndOfScript( crossline );

//
// skip space
//
skipspace:
	while( *script->script_p <= 32 && *script->script_p >= 0 )
	{
		if( script->script_p >= script->end_p )
			return EndOfScript( crossline );

		if( *script->script_p++ == '\n' )
		{
			if( !crossline )
				COM_FatalError( "line %i is incomplete\n", scriptline );
			scriptline = script->line++;
		}
	}

	if( script->script_p >= script->end_p )
		return EndOfScript( crossline );

	// ; # // comments
	if( *script->script_p == ';' || *script->script_p == '#' || ( *script->script_p == '/' && *((script->script_p) + 1) == '/' ))
	{
		if( !crossline )
			COM_FatalError( "line %i is incomplete\n", scriptline );

		if( *script->script_p == '/' )
			script->script_p++;

		if( script->script_p[1] == 'T' && script->script_p[2] == 'X' )
			g_TXcommand = script->script_p[3]; // TX#"-style comment

		if( script->script_p[1] == 'D' && script->script_p[2] == 'X' )
			g_DXspecial = atoi( script->script_p + 3 ); // DX#####"-style comment

		while( *script->script_p++ != '\n' )
		{
			if( script->script_p >= script->end_p )
				return EndOfScript( crossline );
		}

		scriptline = script->line++;
		goto skipspace;
	}

	// /* */ comments
	if( script->script_p[0] == '/' && script->script_p[1] == '*' )
	{
		if( !crossline )
			COM_FatalError( "line %i is incomplete\n", scriptline );

		script->script_p += 2;

		while( script->script_p[0] != '*' || script->script_p[1] != '/' )
		{
			script->script_p++;
			if( script->script_p >= script->end_p )
				return EndOfScript( crossline );
		}

		script->script_p += 2;
		goto skipspace;
	}

	// copy token
	token_p = token;

	if( *script->script_p == '"' )
	{
		// quoted token
		script->script_p++;
		while( *script->script_p != '"' )
		{
			*token_p++ = *script->script_p++;
			if( script->script_p == script->end_p )
				break;

			if( token_p == &token[MAXTOKEN] )
				COM_FatalError( "token too large on line %i\n", scriptline );
		}
		script->script_p++;
	}
	else
	{
		// regular token
		while(( *script->script_p > 32 || *script->script_p < 0 ) && *script->script_p != ';' )
		{
			*token_p++ = *script->script_p++;
			if( script->script_p == script->end_p )
				break;
			if( token_p == &token[MAXTOKEN] )
				COM_FatalError( "token too large on line %i\n", scriptline );
		}
	}

	*token_p = 0; // null terminate

	if( !Q_stricmp( token, "$include" ))
	{
		GetToken( false );
		MsgDev( D_REPORT, "entering the script %s\n", token );
		AddScriptToStack( token );
		return GetToken( crossline );
	}

	return true;
}

bool GetTokenAppend( char *buffer, bool crossline )
{
	bool	result;
	int	i;

	// get the token
	result = GetToken( crossline );

	if( !result || !buffer || token[0] == '\0' )
		return result;

	if( token[0] == '}' )
		scriptdepth--;

	// append?
	if( oldscriptline != scriptline )
	{
		Q_strcat( buffer, "\n" );
		for( i = 0; i < scriptdepth; i++ )
			Q_strcat( buffer, "\t" );
	}
	else Q_strcat( buffer, " " );

	oldscriptline = scriptline;
	Q_strcat( buffer, token );

	if( token[0] == '{' )
		scriptdepth++;

	return result;
}

/*
==============
TokenAvailable

Returns true if there is another token on the line
==============
*/
bool TokenAvailable( void )
{
	const char *search_p;

	search_p = script->script_p;

	if( search_p >= script->end_p )
		return false;

	while( *search_p <= 32 )
	{
		if( *search_p == '\n' )
			return false;

		search_p++;

		if( search_p == script->end_p )
			return false;

	}

	// ; # // comments
	if( *search_p == ';' || *search_p == '#' || ( *search_p == '/' && *((search_p) + 1) == '/' ))
	{
		while( *search_p == '/' )
			search_p++;

		if( *search_p == 'T' && *((search_p) + 1) == 'X' )
			g_TXcommand = *((search_p) + 2); // TX#"-style comment

		if( *search_p == 'D' && *((search_p) + 1) == 'X' )
			g_DXspecial = atoi( ((search_p) + 2) ); // DX#####"-style comment

		return false;
	}

	return true;
}

/*
==============
TryToken

check token for available on current line
==============
*/
bool TryToken( void )
{
	if( !TokenAvailable( ))
		return false;

	GetToken( false );
	return true;
}

void CheckToken( const char *match )
{
	GetToken( true );

	if( Q_strcmp( token, match ))
		COM_FatalError( "missing '%s' at line %d\n", match, scriptline );
}

void Parse1DMatrix( int x, vec_t *m )
{
	int	i;

	CheckToken( "(" );

	for( i = 0; i < x; i++ )
	{
		GetToken( false );
		m[i] = atof( token );
	}

	CheckToken( ")" );
}

void Parse1DMatrixAppend( char *buffer, int x, vec_t *m )
{
	if( !GetTokenAppend( buffer, true ) || Q_strcmp( token, "(" ))
		COM_FatalError( "missing '(' at line %d\n", scriptline );

	for( int i = 0; i < x; i++ )
	{
		if( !GetTokenAppend( buffer, false ))
			COM_FatalError( "line %d is incomplete\n", scriptline );
		m[i] = atof( token );
	}

	if( !GetTokenAppend( buffer, true ) || Q_strcmp( token, ")" ))
		COM_FatalError( "missing ')' at line %d\n", scriptline );
}

void Parse2DMatrix( int y, int x, vec_t *m )
{
	int	i;

	CheckToken( "(" );

	for( i = 0; i < y; i++ )
	{
		Parse1DMatrix( x, m + i * x );
	}

	CheckToken( ")" );
}

void Parse3DMatrix( int z, int y, int x, vec_t *m )
{
	int	i;

	CheckToken( "(" );

	for( i = 0; i < z; i++ )
	{
		Parse2DMatrix( y, x, m + i * x * y );
	}

	CheckToken( ")" );
}

/*
=================
SkipBracedSection

Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
void SkipBracedSection( int depth )
{
	do {
		if( !GetToken( true ))
			break;
		if( !Q_stricmp( token, "{" ))
			depth++;
		else if( !Q_stricmp( token, "}" ))
			depth--;
	} while( depth );
}

/*
=================
SkipAtRestLine

Skips until a newline.
=================
*/
void SkipAtRestLine( void )
{
	while( TryToken( ));
}

bool GetTokenizerStatus( char **pFilename, int *pLine )
{
	// is this the default state?
	if( !script )
		return false;

	if( script->script_p >= script->end_p )
		return false;

	if( pFilename )
		*pFilename = script->filename;

	if( pLine )
		*pLine = script->line;

	return true;
}

void TokenError( const char *fmt, ... )
{
	static char	output[1024];
	va_list		args;

	char		*pFilename;
	int		iLineNumber;

	if( GetTokenizerStatus( &pFilename, &iLineNumber ))
	{
		va_start( args, fmt );
		vsprintf( output, fmt, args );

		COM_FatalError( "%s(%d): - %s", pFilename, iLineNumber, output );
	}
	else
	{
		va_start( args, fmt );
		vsprintf( output, fmt, args );
		COM_FatalError( "%s", output );
	}
}