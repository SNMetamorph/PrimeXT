/*
fs_openmode.h - parsing of the mode of a file
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

#include <fcntl.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

//
// The mode of a file, as it is used by the filesystem layer.
//
// The helpers live in a separate header, so that they can be checked by a
// unit test (tests/test_fs_openmode.cpp) without having to build the whole
// filesystem layer.
//
struct fs_openmode_t
{
	int		mod;		// the access mode: O_RDONLY, O_WRONLY or O_RDWR
	bool	create;	// create the file, if it doesn't exist
	bool	truncate;	// truncate the file
	bool	append;	// write at the end of the file
	bool	binary;	// the file is never translated
	int		invalid;	// the unknown character of the mode, 0 if there is none
};

//
// Parses the mode of a file.
//
// The mode is a string, like in fopen(): the first character is the access
// mode ("r", "w", "a" or "e"), the rest are the flags ("+" and "b").
//
inline fs_openmode_t FS_ParseOpenMode( const char *mode )
{
	fs_openmode_t result = { 0, false, false, false, false, 0 };

	switch( mode[0] )
	{
	case 'r':	// read
		result.mod = O_RDONLY;
		break;
	case 'w': // write
		result.mod = O_WRONLY;
		result.create = true;
		result.truncate = true;
		break;
	case 'a': // append
		result.mod = O_WRONLY;
		result.create = true;
		result.append = true;
		break;
	case 'e': // edit
		result.mod = O_WRONLY;
		result.create = true;
		break;
	default:
		result.mod = -1;
		break;
	}

	for( int ind = 1; mode[ind] != '\0'; ind++ )
	{
		switch( mode[ind] )
		{
		case '+':
			result.mod = O_RDWR;
			break;
		case 'b':
			result.binary = true;
			break;
		default:
			result.invalid = mode[ind];
			break;
		}
	}

	return result;
}

//
// Returns the flags for open() of the file with the given mode.
//
inline int FS_OpenFlags( const fs_openmode_t &mode )
{
	int flags = mode.mod;

	if( mode.create ) flags |= O_CREAT;
	if( mode.truncate ) flags |= O_TRUNC;
	if( mode.append ) flags |= O_APPEND;
	if( mode.binary ) flags |= O_BINARY;

	return flags;
}
