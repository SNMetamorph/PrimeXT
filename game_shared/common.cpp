/*
common.cpp - common game routines
Copyright (C) 2011 Uncle Mike

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
typedef unsigned char byte;
#include <ctype.h>
#include <stdio.h>
#include <vector.h>
#include <stringlib.h>

/*
============
COM_FileBase

Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
============
*/
void COM_FileBase( const char *in, char *out )
{
	int len, start, end;

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
============
COM_ExtractFilePath
============
*/
void COM_ExtractFilePath( const char *path, char *dest )
{
	const char	*src;
	src = path + Q_strlen( path ) - 1;

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
============
COM_StripExtension
============
*/
void COM_StripExtension( const char *in, char *out, int destsize )
{
	Q_strncpy( out, in, destsize );
	COM_StripExtension( out );
}
/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension( char *path, const char *extension )
{
	const char *src;

	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	src = path + Q_strlen( path ) - 1;

	while( *src != '/' && src != path )
	{
		// it has an extension
		if( *src == '.' ) return;                 
		src--;
	}
	Q_strcat( path, extension );
}

/*
============
COM_FixSlashes

Changes all '/' characters into '\' characters, in place.
============
*/
void COM_FixSlashes(char *pname)
{
	while (*pname)
	{
		if (*pname == '\\')
			*pname = '/';
		pname++;
	}
}

/*
==============
COM_ParseFile

safe version text parser
==============
*/
char *COM_ParseFileExt( char *data, char *token, long token_size, bool allowNewLines )
{
	bool newline = false;
	int c, len;

	if( !token || !token_size )
		return NULL;
	
	len = 0;
	token[0] = 0;
	
	if( !data )
		return NULL;
		
// skip whitespace
skipwhite:
	while(( c = ((byte)*data)) <= ' ' )
	{
		if( c == 0 )
			return NULL;	// end of file;
		if( c == '\n' )
			newline = true;
		data++;
	}

	if( newline && !allowNewLines )
		return data;

	newline = false;
	
	// skip // comments
	if( c == '/' && data[1] == '/' )
	{
		while( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if( c == '\"' )
	{
		data++;
		while( 1 )
		{
			c = (byte)*data++;
			if( c == '\"' || !c )
			{
				if( len < token_size )
					token[len] = 0;
				return data;
			}

			if( len < token_size )
				token[len] = c;
			len++;
		}
	}

	// parse single characters
	if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' || c == '|' )
	{
		if( len < token_size )
			token[len] = c;
		len++;

		if( len < token_size )
			token[len] = 0;
		else token[0] = 0;	// string is too long

		return data + 1;
	}

	// parse a regular word
	do
	{
		if( len < token_size )
			token[len] = c;
		data++;
		len++;
		c = ((byte)*data);

		if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' || c == '|' )
			break;
	} while( c > 32 );
	
	if( len < token_size )
		token[len] = 0;
	else token[0] = 0;	// string is too long

	return data;
}

/*
=================
COM_SkipBracedSection

The next token should be an open brace.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
char *COM_SkipBracedSection( char *pfile )
{
	char	token[256];
	int	depth = 0;

	do {
		pfile = COM_ParseFile( pfile, token );

		if( token[1] == 0 )
		{
			if( token[0] == '{' )
				depth++;
			else if( token[0] == '}' )
				depth--;
		}
	} while( depth && pfile != NULL );

	return pfile;
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
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( char *buffer )
{
	char *p = buffer;

	while( *p && *p!='\n')
	{
		if( !isspace( *p ) || isalnum( *p ))
			return 1;
		p++;
	}

	return 0;
}

/*
=================
COM_HashKey

returns hash key for string
=================
*/
unsigned int COM_HashKey( const char *string, unsigned int hashSize )
{
	unsigned int	hashKey = 0;

	for( int i = 0; string[i]; i++ )
		hashKey = (hashKey + i) * 37 + Q_tolower( string[i] );

	return (hashKey % hashSize);
}
