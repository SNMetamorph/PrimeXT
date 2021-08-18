/*
conprint.cpp - extended printf function that allows
colored printing scheme from Quake3
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <windows.h>
#include <stdio.h>
#include <basetypes.h>
#include "stringlib.h"
#include "conprint.h"

#define IsColorString( p )		( p && *( p ) == '^' && *(( p ) + 1) && *(( p ) + 1) >= '0' && *(( p ) + 1 ) <= '9' )
#define ColorIndex( c )		((( c ) - '0' ) & 7 )

static unsigned short g_color_table[8] =
{
FOREGROUND_INTENSITY,				// black
FOREGROUND_RED|FOREGROUND_INTENSITY,			// red
FOREGROUND_GREEN|FOREGROUND_INTENSITY,			// green
FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY,	// yellow
FOREGROUND_BLUE|FOREGROUND_INTENSITY,			// blue
FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY,	// cyan
FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_INTENSITY,	// magenta
FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE,		// default color (white)
};

static int devloper_level = DEFAULT_DEVELOPER;
static bool ignore_log = false;
static FILE *logfile = NULL;

void SetDeveloperLevel( int level )
{
	if( level < D_INFO ) return; // debug messages disabled
	if( level > D_NOTE ) level = D_NOTE;
	devloper_level = level;
}

int GetDeveloperLevel( void )
{
	return devloper_level;
}

/*
===============================================================================

SYSTEM LOG

===============================================================================
*/
void Sys_InitLog( const char *logname )
{
	logfile = fopen( logname, "w" );
	if( !logfile ) MsgDev( D_ERROR, "Sys_InitLog: can't create log file %s\n", logname );
}

void Sys_InitLogAppend( const char *logname )
{
	logfile = fopen( logname, "a+" );
	if( !logfile ) MsgDev( D_ERROR, "Sys_InitLog: can't create log file %s\n", logname );
}

void Sys_IgnoreLog( bool ignore )
{
	ignore_log = ignore;
}

void Sys_CloseLog( void )
{
	if( !logfile ) return;
	fclose( logfile );
	logfile = NULL;
}

void Sys_PrintLog( const char *pMsg )
{
	if( !pMsg || ignore_log )
		return;

	if( !logfile ) return;
	fprintf( logfile, "%s", pMsg );
	fflush( logfile );
}

/*
================
Sys_Print

print into win32 console
================
*/
void Sys_Print( const char *pMsg )
{
	char tmpBuf[8192];
	HANDLE hOut = GetStdHandle( STD_OUTPUT_HANDLE );
	unsigned long cbWritten;
	char *pTemp = tmpBuf;

	while( pMsg && *pMsg )
	{
		if( IsColorString( pMsg ))
		{
			if(( pTemp - tmpBuf ) > 0 )
			{
				// dump accumulated text before change color
				*pTemp = 0; // terminate string
				WriteFile( hOut, tmpBuf, strlen( tmpBuf ), &cbWritten, 0 );
				Sys_PrintLog( tmpBuf );
				pTemp = tmpBuf;
			}

			// set new color
			SetConsoleTextAttribute( hOut, g_color_table[ColorIndex( *(pMsg + 1))] );
			pMsg += 2; // skip color info
		}
		else if(( pTemp - tmpBuf ) < sizeof( tmpBuf ) - 1 )
		{
			*pTemp++ = *pMsg++;
		}
		else
		{
			// temp buffer is full, dump it now
			*pTemp = 0; // terminate string
			WriteFile( hOut, tmpBuf, strlen( tmpBuf ), &cbWritten, 0 );
			Sys_PrintLog( tmpBuf );
			pTemp = tmpBuf;
		}
	}

	// check for last portion
	if(( pTemp - tmpBuf ) > 0 )
	{
		// dump accumulated text
		*pTemp = 0; // terminate string
		WriteFile( hOut, tmpBuf, strlen( tmpBuf ), &cbWritten, 0 );
		Sys_PrintLog( tmpBuf );
		pTemp = tmpBuf;
	}
}

/*
================
Msg

formatted message
================
*/
void Msg( const char *pMsg, ... )
{
	va_list	argptr;
	char	text[8192];
	
	va_start( argptr, pMsg );
	Q_vsnprintf( text, sizeof( text ), pMsg, argptr );
	va_end( argptr );

	Sys_Print( text );
}

/*
================
MsgDev

formatted developer message
================
*/
void MsgDev( int level, const char *pMsg, ... )
{
	va_list	argptr;
	char	text[8192];

	if( devloper_level < level ) return;

	va_start( argptr, pMsg );
	Q_vsnprintf( text, sizeof( text ), pMsg, argptr );
	va_end( argptr );

	switch( level )
	{
	case D_WARN:
		Sys_Print( va( "^3Warning:^7 %s", text ));
		break;
	case D_ERROR:
		Sys_Print( va( "^1Error:^7 %s", text ));
		break;
	case D_INFO:
	case D_NOTE:
	case D_REPORT:
		Sys_Print( text );
		break;
	}
}

void MsgAnim( int level, const char *pMsg, ... )
{
	va_list	argptr;
	char	text[1024];
	char	empty[1024];

	if( devloper_level < level ) return;

	va_start( argptr, pMsg );
	Q_vsnprintf( text, sizeof( text ), pMsg, argptr );
	va_end( argptr );

	// fill clear string
	for( int j = 0; j < Q_strlen( text ); j++ )
		empty[j] = ' ';
	empty[j] = '\r';
	empty[j+1] = '\0';

	// do animation
	for( int i = 0; i < 8; i++ )
	{
		Sys_IgnoreLog( i < 7 );
		if( i & 1 ) Sys_Print( text );
		else Sys_Print( empty );
		Sleep( 150 );
	}
	Msg( "^7\n" );
}