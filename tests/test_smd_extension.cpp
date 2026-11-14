/*
test_smd_extension.cpp - unit test for the .smd extension handling
Copyright (C) 2026 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Covers issue #229: pxstudiomdl couldn't find a submodel whose name contains
a dot, because every dot was treated as the beginning of an extension.
*/

#include "extension.h"

#include <cstdio>
#include <cstring>

static int Test_DefaultExtension( void )
{
	static const char *strings[][3] = {
		{ "head",			".smd", "head.smd" },
		{ "head.smd",		".smd", "head.smd" },
		{ "HEAD.SMD",		".smd", "HEAD.SMD" },
		// issue #229: a submodel name may contain a dot
		{ "my.mesh",		".smd", "my.mesh.smd" },
		{ "my.mesh.smd",	".smd", "my.mesh.smd" },
		{ "path/to/my.mesh",	".smd", "path/to/my.mesh.smd" },
		{ "path/to/my.mesh.smd", ".smd", "path/to/my.mesh.smd" },
	};

	for( int i = 0; i < static_cast<int>( sizeof( strings ) / sizeof( strings[0] )); i++ )
	{
		char s[256];

		strncpy( s, strings[i][0], sizeof( s ) - 1 );
		s[sizeof( s ) - 1] = '\0';

		FileDefaultExtension( s, sizeof( s ), strings[i][1] );

		if( strcmp( s, strings[i][2] ) != 0 )
		{
			std::fprintf( stderr, "%s:%d: case %d: \"%s\" -> \"%s\", expected \"%s\"\n",
				__FILE__, __LINE__, i, strings[i][0], s, strings[i][2] );
			return i + 1;
		}
	}

	return 0;
}

static int Test_DefaultExtension_BufferBound( void )
{
	char s[8];

	// "my.mesh" doesn't leave enough room for ".smd", the path must stay intact
	strncpy( s, "my.mesh", sizeof( s ) - 1 );
	s[sizeof( s ) - 1] = '\0';

	FileDefaultExtension( s, sizeof( s ), ".smd" );

	if( strcmp( s, "my.mesh" ) != 0 )
	{
		std::fprintf( stderr, "%s:%d: path was modified: \"%s\"\n", __FILE__, __LINE__, s );
		return 1;
	}

	return 0;
}

static int Test_HasExtension( void )
{
	static const char *strings[][3] = {
		{ "head.smd",		".smd", "yes" },
		{ "HEAD.SMD",		".smd", "yes" },
		{ "head",			".smd", "no" },
		{ "my.mesh",		".smd", "no" },
		{ "my.mesh.smd",	".smd", "yes" },
		{ "",				".smd", "no" },
	};

	for( int i = 0; i < static_cast<int>( sizeof( strings ) / sizeof( strings[0] )); i++ )
	{
		const bool has = FileHasExtension( strings[i][0], strings[i][1] );
		const bool expected = strcmp( strings[i][2], "yes" ) == 0;

		if( has != expected )
		{
			std::fprintf( stderr, "%s:%d: case %d: \"%s\" has extension \"%s\": %s, expected %s\n",
				__FILE__, __LINE__, i, strings[i][0], strings[i][1],
				has ? "yes" : "no", strings[i][2] );
			return i + 1;
		}
	}

	return 0;
}

int main( void )
{
	int ret = Test_DefaultExtension();
	if( ret )
		return ret;

	ret = Test_DefaultExtension_BufferBound();
	if( ret )
		return ret + 32;

	ret = Test_HasExtension();
	if( ret )
		return ret + 64;

	return 0;
}
