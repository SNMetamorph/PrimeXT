/*
fs_readbuffer.h - helpers for the buffered reading of the files
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

#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>

//
// The helpers live in a separate header, so that they can be checked by a
// unit test (tests/test_fs_readbuffer.cpp) without having to build the whole
// filesystem layer.
//

//
// Copies the data of the read buffer into the destination.
//
// The character that was pushed back with FS_UnGetc() is the first byte of
// the result, so the data of the buffer is copied after it.
//
// Returns the amount of the copied bytes.
//
inline size_t FS_CopyFromReadBuffer( void *dest, size_t destSize,
	const unsigned char *buff, size_t *buff_ind, size_t buff_len, int *ungetc )
{
	unsigned char *out = (unsigned char *)dest;
	size_t done = 0;

	// the pushed back character comes first
	if( *ungetc != EOF )
	{
		if( destSize == 0 )
			return 0;

		out[0] = (unsigned char)*ungetc;
		*ungetc = EOF;
		done = 1;
	}

	if( *buff_ind < buff_len )
	{
		size_t count = buff_len - *buff_ind;

		if( count > destSize - done )
			count = destSize - done;

		// the data of the buffer goes after the pushed back character
		memcpy( out + done, buff + *buff_ind, count );
		*buff_ind += count;
		done += count;
	}

	return done;
}

//
// Reads a line into the string, like fgets().
//
// The "\n" that follows a "\r" is removed, and the character that follows the
// "\r" is pushed back, so that it is read again.
//
// getchar is called to get the next character of the file, ungetchar is
// called to push a character back.
//
// Returns the character that ended the line, or EOF.
//
template<typename GetChar, typename UnGetChar>
inline int FS_ReadLine( GetChar getchar, UnGetChar ungetchar, unsigned char *string, size_t bufsize )
{
	int c, end = 0;

	while( 1 )
	{
		c = getchar();

		if( c == '\r' || c == '\n' || c < 0 )
			break;

		if( end < bufsize - 1 )
			string[end++] = (unsigned char)c;
	}
	string[end] = 0;

	// remove \n following \r
	if( c == '\r' )
	{
		c = getchar();

		// EOF must not be pushed back: it would be read as a regular character
		if( c != '\n' && c != EOF )
			ungetchar( c );
	}

	return c;
}
