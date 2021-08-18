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

// scriplib.h

#ifndef __CMDLIB__
#include "cmdlib.h"
#endif

#define MAXTOKEN	2048

extern char	token[MAXTOKEN];
extern char	g_TXcommand;
extern int	g_DXspecial;
extern int	scriptline;
extern bool	endofscript;

void LoadScriptFile( const char *filename );
void IncludeScriptFile( const char *filename );
void ParseFromMemory( const char *buffer, size_t size );

bool GetToken( bool crossline );
bool GetTokenAppend( char *buffer, bool crossline );
void UnGetToken( void );
bool TokenAvailable( void );
bool TryToken( void );
void CheckToken( const char *match );
void Parse1DMatrix( int x, vec_t *m );
void Parse1DMatrixAppend( char *buffer, int x, vec_t *m );
void Parse2DMatrix( int y, int x, vec_t *m );
void Parse3DMatrix( int z, int y, int x, vec_t *m );
void TokenError( const char *fmt, ... );
void SkipBracedSection( int depth );
void SkipAtRestLine( void );
