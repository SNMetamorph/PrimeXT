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

/*
================
Con_Print

print into win32 console
================
*/
void Con_Print( const char *pMsg )
{
	const char	*msg = pMsg;
	char		buffer[32768];
	char		logbuf[32768];
	char		*b = buffer;
	char		*c = logbuf;	
	int		i = 0;

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
			pTemp = tmpBuf;
		}
	}

	// check for last portion
	if(( pTemp - tmpBuf ) > 0 )
	{
		// dump accumulated text
		*pTemp = 0; // terminate string
		WriteFile( hOut, tmpBuf, strlen( tmpBuf ), &cbWritten, 0 );
		pTemp = tmpBuf;
	}
}

/*
================
Con_Printf

print into win32 console
================
*/
void Con_Printf( const char *pMsg, ... )
{
	char	buffer[32768];
	va_list	args;
	int	result;

	va_start( args, pMsg );
	result = _vsnprintf( buffer, 32767, pMsg, args );
	va_end( args );

	Con_Print( buffer );
}
