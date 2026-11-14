/*
test_fs_openmode.cpp - unit test for the parsing of the mode of a file
Copyright (C) 2026 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstdio>

#include "fs_openmode.h"

#define ARRAYSIZE( x ) ( sizeof( x ) / sizeof( ( x )[0] ) )

struct testcase_t
{
	const char	*name;
	const char	*mode;
	int			mod;
	bool	create;
	bool	truncate;
	bool	append;
	int			invalid;	// the unknown character of the mode, 0 if there is none
};

static const testcase_t tests[] =
{
	{ "read",		"r",	O_RDONLY,	false, false, false,	0 },
	{ "read binary",	"rb",	O_RDONLY,	false, false, false,	0 },
	{ "read write",		"r+",	O_RDWR,	false, false, false,	0 },
	{ "write",		"w",	O_WRONLY,	true,	true,  false,	0 },
	{ "write binary",	"wb",	O_WRONLY,	true,	true,  false,	0 },
	{ "append",		"a",	O_WRONLY,	true,	false, true,	0 },
	{ "edit",		"e",	O_WRONLY,	true,	false, false,	0 },
	{ "invalid",		"x",	-1,		false, false, false,	0 },
	{ "unknown char",	"rz",	O_RDONLY,	false, false, false,	'z' },
};

int main( void )
{
	for( size_t i = 0; i < ARRAYSIZE( tests ); i++ )
	{
		const testcase_t *test = &tests[i];

		fs_openmode_t mode = FS_ParseOpenMode( test->mode );

		if( mode.mod != test->mod )
		{
			std::fprintf( stderr, "%s: %s: the access mode is %d, expected %d\n",
				__FILE__, test->name, mode.mod, test->mod );
			return 1;
		}

		if( mode.create != test->create || mode.truncate != test->truncate || mode.append != test->append )
		{
			std::fprintf( stderr, "%s: %s: wrong flags (create %d, truncate %d, append %d)\n",
				__FILE__, test->name, mode.create, mode.truncate, mode.append );
			return 1;
		}

		if( mode.invalid != test->invalid )
		{
			std::fprintf( stderr, "%s: %s: the unknown character is '%c', expected '%c'\n",
				__FILE__, test->name, mode.invalid, test->invalid );
			return 1;
		}

		// The filesystem layer is a binary layer: the files are never
		// translated, no matter what the mode says.
		//
		// The C runtime translates the line endings of the files opened in the
		// text mode on Windows, so the amount of the data in such a file is
		// less than the size of the file, and the bookkeeping of the file
		// positions breaks: the last line of the file is read wrong (issue #103).
		if( !mode.binary )
		{
			std::fprintf( stderr, "%s: %s: the file is not opened in the binary mode\n",
				__FILE__, test->name );
			return 1;
		}

#if defined( _WIN32 )
		// Only Windows has a text mode at all, everywhere else the flag is
		// empty and there is nothing to check.
		if( !( FS_OpenFlags( mode ) & O_BINARY ) )
		{
			std::fprintf( stderr, "%s: %s: O_BINARY is not set in the flags\n",
				__FILE__, test->name );
			return 1;
		}
#endif
	}

	std::fprintf( stderr, "test_fs_openmode: all cases passed\n" );
	return 0;
}
