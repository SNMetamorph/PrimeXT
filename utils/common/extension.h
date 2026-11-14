/*
extension.h - helpers for working with file name extensions
Copyright (C) 2026 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

These helpers are header-only on purpose: they are used by the asset compilers
and by the unit tests, and pulling in the whole file system layer just to append
an extension would make them needlessly hard to test.
*/

#pragma once

#include <cstddef>
#include <cstring>

//
// Returns true if the path already ends with the given extension.
// The comparison is case-insensitive, the extension has to include the dot.
//
inline bool FileHasExtension( const char *path, const char *extension )
{
	if( !path || !extension )
		return false;

	const size_t	pathLen = strlen( path );
	const size_t	extLen = strlen( extension );

	if( extLen == 0 || pathLen < extLen )
		return false;

	const char *tail = path + pathLen - extLen;

	for( size_t i = 0; i < extLen; i++ )
	{
		char a = tail[i];
		char b = extension[i];

		if( a >= 'A' && a <= 'Z' ) a = static_cast<char>( a + 'a' - 'A' );
		if( b >= 'A' && b <= 'Z' ) b = static_cast<char>( b + 'a' - 'A' );

		if( a != b )
			return false;
	}

	return true;
}

//
// Appends the extension to the path, unless the path already ends with it.
//
// Unlike COM_DefaultExtension(), this doesn't treat every dot in the file name
// as the beginning of an extension: a submodel named "my.mesh" has to become
// "my.mesh.smd" and not stay "my.mesh" (see issue #229).
//
inline void FileDefaultExtension( char *path, size_t pathSize, const char *extension )
{
	if( !path || !extension || pathSize == 0 )
		return;

	if( FileHasExtension( path, extension ) )
		return;

	const size_t	len = strlen( path );
	const size_t	extLen = strlen( extension );

	if( len + extLen + 1 > pathSize )
		return;

	memcpy( path + len, extension, extLen + 1 );
}
