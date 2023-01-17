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

#include "port.h"
#include "basetypes.h"
#include "stringlib.h"
#include "conprint.h"
#include "mathlib.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#if XASH_POSIX
#include <termios.h>
#include <unistd.h>
#endif

#if XASH_ANDROID
#include <android/log.h>
#endif

#define IsColorString( p )		( p && *( p ) == '^' && *(( p ) + 1) && *(( p ) + 1) >= '0' && *(( p ) + 1 ) <= '9' )
#define ColorIndex( c )		((( c ) - '0' ) & 7 )

#if XASH_WIN32
static unsigned short g_color_table[8] =
{
FOREGROUND_INTENSITY,									// black
FOREGROUND_RED,											// red
FOREGROUND_GREEN,										// green
FOREGROUND_RED | FOREGROUND_GREEN,						// yellow
FOREGROUND_BLUE | FOREGROUND_INTENSITY,					// blue
FOREGROUND_GREEN | FOREGROUND_BLUE,						// cyan
FOREGROUND_RED | FOREGROUND_BLUE,						// magenta
FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,	// default color (white)
};
#endif

#define COLORIZE_CONSOLE

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

void Sys_PrintLog(const char *pMsg)
{
	if (!pMsg || ignore_log)
		return;

	time_t crt_time;
	const struct tm	*crt_tm;
	static char lastchar;

	time(&crt_time);
	crt_tm = localtime(&crt_time);

#if XASH_ANDROID
	__android_log_print(ANDROID_LOG_DEBUG, "PrimeXT", "%s", pMsg);
#endif

	if (!logfile)
		return;

	if (!lastchar || lastchar == '\n')
	{
		char logtime[32] = "";
		strftime(logtime, sizeof(logtime), "[%Y:%m:%d|%H:%M:%S]", crt_tm); // full time
		fprintf(logfile, "%s %s", logtime, pMsg);
	}
	else {
		fprintf(logfile, "%s", pMsg);
	}

	fflush(logfile);
	lastchar = pMsg[strlen(pMsg) - 1];
}

#if XASH_WIN32
static void Sys_PrintWin32(const char *pMsg)
{
	char tmpBuf[8192];
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD cbWritten;
	char *pTemp = tmpBuf;

	// always initially reset color to white
	SetConsoleTextAttribute(hOut, g_color_table[7]); 

	while (pMsg && *pMsg)
	{
		if (IsColorString(pMsg))
		{
			if ((pTemp - tmpBuf) > 0)
			{
				// dump accumulated text before change color
				*pTemp = 0; // terminate string
				WriteFile(hOut, tmpBuf, static_cast<DWORD>(strlen(tmpBuf)), &cbWritten, 0);
				Sys_PrintLog(tmpBuf);
				pTemp = tmpBuf;
			}

			// set new color
			SetConsoleTextAttribute(hOut, g_color_table[ColorIndex(*(pMsg + 1))]);
			pMsg += 2; // skip color info
		}
		else if ((pTemp - tmpBuf) < sizeof(tmpBuf) - 1)
		{
			*pTemp++ = *pMsg++;
		}
		else
		{
			// temp buffer is full, dump it now
			*pTemp = 0; // terminate string
			WriteFile(hOut, tmpBuf, static_cast<DWORD>(strlen(tmpBuf)), &cbWritten, 0);
			Sys_PrintLog(tmpBuf);
			pTemp = tmpBuf;
		}
	}

	// check for last portion
	if ((pTemp - tmpBuf) > 0)
	{
		// dump accumulated text
		*pTemp = 0; // terminate string
		WriteFile(hOut, tmpBuf, static_cast<DWORD>(strlen(tmpBuf)), &cbWritten, 0);
		Sys_PrintLog(tmpBuf);
		pTemp = tmpBuf;
	}
}
#endif

#if !XASH_WIN32
static void Sys_PrintPosix(const char *pMsg)
{
#ifdef COLORIZE_CONSOLE
	char colored[4096];
	const char *msg = pMsg;
	int len = 0;

	while (*msg && (len < 4090))
	{
		static char q3ToAnsi[8] =
		{
			'0', // COLOR_BLACK
			'1', // COLOR_RED
			'2', // COLOR_GREEN
			'3', // COLOR_YELLOW
			'4', // COLOR_BLUE
			'6', // COLOR_CYAN
			'5', // COLOR_MAGENTA
			0 // COLOR_WHITE
		};

		if (IsColorString(msg))
		{
			msg++;
			int color = q3ToAnsi[*msg++ % 8];
			colored[len++] = '\033';
			colored[len++] = '[';
			if (color)
			{
				colored[len++] = '3';
				colored[len++] = color;
			}
			else
				colored[len++] = '0';
			colored[len++] = 'm';
		}
		else
			colored[len++] = *msg++;
	}

	colored[len] = 0;
	printf("%s\033[0m", colored);

#elif !XASH_ANDROID
	printf("%s", pMsg);
	fflush(stdout);
#endif

	Sys_PrintLog(pMsg);
}
#endif

/*
================
Sys_Print

print into win32 console
================
*/
void Sys_Print( const char *pMsg )
{
#if XASH_WIN32
	Sys_PrintWin32(pMsg);
#else
	Sys_PrintPosix(pMsg);
#endif
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
	int j;
	va_list	argptr;
	char	text[1024];
	char	empty[1024];

	if (devloper_level < level)
		return;

	va_start(argptr, pMsg);
	Q_vsnprintf(text, sizeof(text), pMsg, argptr);
	va_end(argptr);

	// fill clear string
	for (j = 0; j < Q_strlen(text); j++) {
		empty[j] = ' ';
}
	empty[j] = '\r';
	empty[j + 1] = '\0';

	// do animation
	for (int i = 0; i < 8; i++)
	{
		Sys_IgnoreLog(i < 7);
		if (i & 1) {
			Sys_Print(text);
		}
		else {
			Sys_Print(empty);
		}
		Sys_Sleep(150);
	}
}

/*
================
Sys_Sleep

freeze application for some time
================
*/
void Sys_Sleep( unsigned int msec )
{
	if( !msec )
		return;

	msec = Q_min( msec, 1000 );
#if XASH_WIN32
        Sleep( msec );
#elif XASH_POSIX
        usleep( msec * 1000 );
#else
#error "Implement me!"
#endif
}

void Sys_WaitForKeyInput()
{
#if XASH_WIN32
	system("pause>nul");
#else
	struct termios term;
	char buf;
	tcflag_t old_lflag;

	tcgetattr (0, &term);
	old_lflag = term.c_lflag;
	term.c_lflag &= ~(ECHO | ICANON);	
	tcsetattr (0, TCSANOW, &term);		
	while (read (0, &buf, 1)) {
		break;
	}
	term.c_lflag = old_lflag;
	tcsetattr (0, TCSANOW, &term);
#endif
}
