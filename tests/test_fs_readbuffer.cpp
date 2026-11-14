/*
test_fs_readbuffer.cpp - unit test for the buffered reading of the files
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
#include <cstring>

#include "fs_readbuffer.h"

#define ARRAYSIZE( x ) ( sizeof( x ) / sizeof( ( x )[0] ) )

//
// The copy of the data of the read buffer.
//
struct copycase_t
{
	const char	*name;
	const char	*buffer;		// the content of the read buffer
	size_t		buff_ind;	// the position in the buffer
	int			ungetc;		// the pushed back character, EOF if there is none
	size_t		dest_size;	// the size of the destination
	const char	*expected;	// the expected content of the destination
	size_t		expected_done;	// the expected amount of the copied bytes
	size_t		expected_ind;	// the expected position in the buffer afterwards
	int			expected_ungetc;	// the expected pushed back character afterwards
};

static const copycase_t copycases[] =
{
	{ "empty buffer",		"",		0, EOF, 4, "",		0, 0, EOF },
	{ "data",			"ABCD",		1, EOF, 4, "BCD",		3, 4, EOF },
	{ "small destination",	"ABCD",		0, EOF, 2, "AB",		2, 2, EOF },
	{ "pushed back character",	"CD",	0, 'X', 4, "XCD",	3, 2, EOF },
	{ "pushed back character, no data",	"",	0, 'X', 4, "X",		1, 0, EOF },
	{ "pushed back character, small destination",	"CD",	0, 'X', 1, "X",	1, 0, EOF },
	{ "empty destination",	"ABCD",		0, 'X', 0, "",			0, 0, 'X' },
};

//
// The reading of a line.
//
struct linecase_t
{
	const char	*name;
	const char	*data;			// the content of the file
	const char	*expected;		// the expected line
	int			expected_char;	// the expected character that ended the line
	int			expected_ungetc;	// the expected pushed back character afterwards
};

static const linecase_t linecases[] =
{
	{ "line",			"hello\nworld",	"hello",	'\n', EOF },
	{ "empty line",		"\nworld",		"",		'\n', EOF },
	{ "crlf",			"hello\r\nworld",	"hello",	'\n', EOF },
	{ "cr at the end",	"hello\r",		"hello",	EOF,	EOF },
	{ "cr in the middle",	"hello\rworld",	"hello",	'w',	'w' },
	{ "eof",			"",			"",		EOF,	EOF },
};

// A file that returns the characters of a string, like the real one does.
struct testfile_t
{
	const char	*data;
	size_t		pos;
	int			ungetc;

	int getchar( void )
	{
		if( ungetc != EOF )
		{
			int c = ungetc;
			ungetc = EOF;
			return c;
		}

		if( !data || data[pos] == '\0' )
			return EOF;

		return (unsigned char)data[pos++];
	}

	// the real FS_UnGetc() stores the character as a byte
	void ungetchar( int c )
	{
		ungetc = (unsigned char)c;
	}
};

static int Test_CopyFromReadBuffer( void )
{
	for( size_t i = 0; i < ARRAYSIZE( copycases ); i++ )
	{
		const copycase_t *test = &copycases[i];

		unsigned char buff[16] = { 0 };
		unsigned char dest[16] = { 0 };
		size_t buff_ind = test->buff_ind;
		int ungetc = test->ungetc;

		memcpy( buff, test->buffer, strlen( test->buffer ) );

		size_t done = FS_CopyFromReadBuffer( dest, test->dest_size, buff, &buff_ind, strlen( test->buffer ), &ungetc );

		if( done != test->expected_done )
		{
			std::fprintf( stderr, "%s: %s: copied %u bytes, expected %u\n",
				__FILE__, test->name, (unsigned)done, (unsigned)test->expected_done );
			return 1;
		}

		if( memcmp( dest, test->expected, strlen( test->expected ) ) != 0 )
		{
			std::fprintf( stderr, "%s: %s: got \"%s\", expected \"%s\"\n",
				__FILE__, test->name, dest, test->expected );
			return 1;
		}

		if( buff_ind != test->expected_ind )
		{
			std::fprintf( stderr, "%s: %s: the position in the buffer is %u, expected %u\n",
				__FILE__, test->name, (unsigned)buff_ind, (unsigned)test->expected_ind );
			return 1;
		}

		if( ungetc != test->expected_ungetc )
		{
			std::fprintf( stderr, "%s: %s: the pushed back character is %d, expected %d\n",
				__FILE__, test->name, ungetc, test->expected_ungetc );
			return 1;
		}
	}

	return 0;
}

static int Test_ReadLine( void )
{
	for( size_t i = 0; i < ARRAYSIZE( linecases ); i++ )
	{
		const linecase_t *test = &linecases[i];

		testfile_t file = { 0 };
		unsigned char line[64] = { 0 };

		file.data = test->data;
		file.ungetc = EOF;

		int c = FS_ReadLine(
			[&file]() { return file.getchar(); },
			[&file]( int ch ) { file.ungetchar( ch ); },
			line, sizeof( line ) );

		if( c != test->expected_char )
		{
			std::fprintf( stderr, "%s: %s: the line was ended by %d, expected %d\n",
				__FILE__, test->name, c, test->expected_char );
			return 1;
		}

		if( strcmp( (const char *)line, test->expected ) != 0 )
		{
			std::fprintf( stderr, "%s: %s: got the line \"%s\", expected \"%s\"\n",
				__FILE__, test->name, line, test->expected );
			return 1;
		}

		if( file.ungetc != test->expected_ungetc )
		{
			std::fprintf( stderr, "%s: %s: the pushed back character is %d, expected %d\n",
				__FILE__, test->name, file.ungetc, test->expected_ungetc );
			return 1;
		}
	}

	return 0;
}

int main( void )
{
	if( Test_CopyFromReadBuffer() )
		return 1;

	if( Test_ReadLine() )
		return 1;

	std::fprintf( stderr, "test_fs_readbuffer: all cases passed\n" );
	return 0;
}
