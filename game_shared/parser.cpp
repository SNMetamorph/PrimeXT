/*
parser.cpp - extended text parser for Q3A-style shaders
Copyright (C) 2013 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <vector.h>
#include <stringlib.h>

// g-cont. Damn! stupid valve coders!!!
#ifdef CLIENT_DLL
#include "cdll_int.h"
#include "render_api.h"
#else
#include "eiface.h"
#include "physint.h"
#include "physcallback.h"
#include "edict.h"
#endif

#include "enginecallback.h"

/*
============================================================================

PARSING

============================================================================
*/

// multiple character punctuation tokens
const char *punctuation_table[] = { "+=", "-=", "*=", "/=", "&=", "|=", "++", "--", "&&", "||", "<=", ">=", "==", "!=", NULL };

struct
{
	char	token[MAX_TOKEN_CHARS];
	char	name[256];
	int	lines;
} parse;

/*
=================
COM_BeginParse

starts a new parse session
=================
*/
void COM_BeginParse( const char *name )
{
	Q_strncpy( parse.name, name, sizeof( parse.name ));
	parse.lines = 0;
}

/*
=================
COM_ParseError

print line error and filename
=================
*/
void COM_ParseError( char *szFmt, ... )
{
	va_list         args;
	static char     buffer[2048];

	va_start( args, szFmt );
	Q_vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	Msg( "^1Error:^7 %s, line %d: %s\n", parse.name, parse.lines, buffer );
}

/*
=================
COM_ParseWarning

print line warning and filename
=================
*/
void COM_ParseWarning( char *szFmt, ... )
{
	va_list         args;
	static char     buffer[2048];

	va_start( args, szFmt );
	Q_vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	Msg( "^3Warning:^7 %s, line %d: %s\n", parse.name, parse.lines, buffer );
}

/*
=================
COM_SkipWhiteSpace

skip the white space
=================
*/
static char *COM_SkipWhiteSpace( char *data, bool *hasNewLines )
{
	int	c;

	while(( c = *data ) <= ' ' )
	{
		if( !c ) return NULL;

		if( c == '\n' )
		{
			parse.lines++;
			*hasNewLines = true;
		}

		data++;
	}

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
void COM_SkipBracedSection( char **program )
{
	char	*token;
	int	depth = 0;

	do {
		token = COM_Parse( program );

		if( token[1] == 0 )
		{
			if( token[0] == '{' )
				depth++;
			else if( token[0] == '}' )
				depth--;
		}
	} while( depth && *program );
}

/*
=================
COM_SkipRestOfLine
=================
*/
void COM_SkipRestOfLine( char **data )
{
	char	*p;
	int	c;

	p = *data;

	while(( c = *p++ ) != 0 )
	{
		if( c == '\n' )
		{
			parse.lines++;
			break;
		}
	}

	*data = p;
}

/*
==============
COM_ParseExt

Parse a token out of a string
Will never return NULL, just empty strings

If "allowLineBreaks" is qtrue then an empty
string will be returned if the next token is
a newline.
==============
*/
char *COM_ParseExt( char **data_p, bool allowLineBreaks )
{
	int		c = 0, len;
	bool		hasNewLines = false;
	char		*data;
	const char	**punc;

	if( !data_p )
		HOST_ERROR( "COM_ParseExt: data_p == NULL\n" );

	data = *data_p;
	parse.token[0] = 0;
	len = 0;

	// make sure incoming data is valid
	if( !data )
	{
		*data_p = NULL;
		return parse.token;
	}

	// skip whitespace
	while( 1 )
	{
		data = COM_SkipWhiteSpace( data, &hasNewLines );

		if( !data )
		{
			*data_p = NULL;
			return parse.token;
		}

		if( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return parse.token;
		}

		c = *data;

		// skip double slash comments
		if( c == '/' && data[1] == '/' )
		{
			data += 2;
			while( *data && *data != '\n' )
			{
				data++;
			}
		}
		// skip /* */ comments
		else if( c == '/' && data[1] == '*' )
		{
			data += 2;
			while( *data && ( *data != '*' || data[1] != '/' ))
			{
				data++;
			}

			if( *data )
			{
				data += 2;
			}
		}
		else
		{
			// a real token to parse
			break;
		}
	}

	// handle quoted strings
	if( c == '\"' )
	{
		data++;

		while( 1 )
		{
			c = *data++;

			if(( c == '\\' ) && ( *data == '\"' ))
			{
				// allow quoted strings to use \" to indicate the " character
				data++;
			}
			else if( c == '\"' || !c )
			{
				parse.token[len] = 0;
				*data_p = (char *)data;
				return parse.token;
			}
			else if( *data == '\n' )
			{
				parse.lines++;
			}

			if( len < ( MAX_TOKEN_CHARS - 1 ))
			{
				parse.token[len] = c;
				len++;
			}
		}
	}

	// check for a number
	// is this parsing of negative numbers going to cause expression problems
	if(( c >= '0' && c <= '9' ) || ( c == '-' && data[1] >= '0' && data[1] <= '9' ) || ( c == '.' && data[1] >= '0' && data[1] <= '9' ) || ( c == '-' && data[1] == '.' && data[2] >= '0' && data[2] <= '9' ))
	{
		do
		{
			if( len < ( MAX_TOKEN_CHARS - 1 ))
			{
				parse.token[len] = c;
				len++;
			}
			data++;

			c = *data;
		} while(( c >= '0' && c <= '9' ) || c == '.' );

		// parse the exponent
		if( c == 'e' || c == 'E' )
		{
			if( len < ( MAX_TOKEN_CHARS - 1 ))
			{
				parse.token[len] = c;
				len++;
			}

			data++;
			c = *data;

			if( c == '-' || c == '+' )
			{
				if( len < ( MAX_TOKEN_CHARS - 1 ))
				{
					parse.token[len] = c;
					len++;
				}

				data++;
				c = *data;
			}

			do
			{
				if( len < ( MAX_TOKEN_CHARS - 1 ))
				{
					parse.token[len] = c;
					len++;
				}

				data++;
				c = *data;

			} while( c >= '0' && c <= '9' );
		}

		if( len == MAX_TOKEN_CHARS )
			len = 0;
		parse.token[len] = 0;

		*data_p = (char *)data;

		return parse.token;
	}

	// check for a regular word
	// we still allow forward and back slashes in name tokens for pathnames
	// and also colons for drive letters
	if(( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c == '_' ) || ( c == '/' ) || ( c == '\\' ) || ( c == '$' ) || ( c == '*' ))
	{
		do
		{
			if( len < ( MAX_TOKEN_CHARS - 1 ))
			{
				parse.token[len] = c;
				len++;
			}

			data++;
			c = *data;
		} while(( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c == '_' ) || ( c == '-' ) || ( c >= '0' && c <= '9' ) || ( c == '/' ) || ( c == '\\' ) || ( c == ':' ) || ( c == '.' ) || ( c == '$' ) || ( c == '*' ) || ( c == '@' ));

		if( len == MAX_TOKEN_CHARS )
			len = 0;
		parse.token[len] = 0;

		*data_p = (char *)data;
		return parse.token;
	}

	// check for multi-character punctuation token
	for( punc = punctuation_table; *punc; punc++ )
	{
		int	j, l;

		l = Q_strlen( *punc );

		for( j = 0; j < l; j++ )
		{
			if( data[j] != (*punc)[j] )
				break;
		}

		if( j == l )
		{
			// a valid multi-character punctuation
			memcpy( parse.token, *punc, l );
			parse.token[l] = 0;
			data += l;
			*data_p = (char *)data;

			return parse.token;
		}
	}

	// single character punctuation
	parse.token[0] = *data;
	parse.token[1] = 0;
	data++;

	*data_p = (char *)data;

	return parse.token;
}

/*
=================
COM_CompressText

remove whitespaces, comments and tabulation
=================
*/
int COM_CompressText( char *data_p )
{
	char	*in, *out;
	bool	newline = false;
	bool	whitespace = false;
	int	c;

	in = out = data_p;

	if( in )
	{
		while(( c = *in ) != 0 )
		{
			// skip double slash comments
			if( c == '/' && in[1] == '/' )
			{
				while( *in && *in != '\n' )
				{
					in++;
				}
				// skip /* */ comments
			}
			else if( c == '/' && in[1] == '*' )
			{
				while( *in && (*in != '*' || in[1] != '/' ))
					in++;

				if( *in ) in += 2;
				// record when we hit a newline
			}
			else if( c == '\n' || c == '\r' )
			{
				newline = true;
				in++;
				// record when we hit whitespace
			}
			else if( c == ' ' || c == '\t' )
			{
				whitespace = true;
				in++;
				// an actual token
			}
			else
			{
				// if we have a pending newline, emit it (and it counts as whitespace)
				if( newline )
				{
					*out++ = '\n';
					newline = false;
					whitespace = false;
				}

				if( whitespace )
				{
					*out++ = ' ';
					whitespace = false;
				}

				// copy quoted strings unmolested
				if( c == '"' )
				{
					*out++ = c;
					in++;

					while( 1 )
					{
						c = *in;

						if( c && c != '"' )
						{
							*out++ = c;
							in++;
						}
						else
						{
							break;
						}
					}

					if( c == '"' )
					{
						*out++ = c;
						in++;
					}
				}
				else
				{
					*out = c;
					out++;
					in++;
				}
			}
		}
	}

	*out = 0;

	return (out - data_p);
}
