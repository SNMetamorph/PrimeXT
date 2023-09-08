/*
NxUserStream.cpp - this file is a part of Novodex physics engine implementation
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

#ifdef USE_PHYSICS_ENGINE

#include "extdll.h"
#include "util.h"
//#include "Pxf.h"
#include "NxUserStream.h"
#include <direct.h>

using namespace physx;

// user stream. constructor
UserStream :: UserStream( const char* filename, bool load ) : fp(NULL)
{
	load_file = load;

	if( load_file )
	{
		int size;

		// load from pack or disk
		buffer = LOAD_FILE( filename, &size );
		m_iLength = size;
		m_iOffset = 0;
	}
	else
	{
		// make sure the directories have been made
		char	szFilePath[MAX_PATH];
		char	szFullPath[MAX_PATH];

		// make sure directories have been made
		GET_GAME_DIR( szFilePath );

		Q_snprintf( szFullPath, sizeof( szFullPath ), "%s/%s", szFilePath, filename );
		CreatePath( szFullPath ); // make sure what all folders are existing

		// write to disk
		fp = fopen( szFullPath, "wb" );
		ASSERT( fp != NULL );
	}
}

UserStream :: ~UserStream()
{
	if( load_file )
    {
		FREE_FILE( buffer );
		m_iOffset = m_iLength = 0;
		buffer = NULL;         
	}
	else
	{
		if( fp ) fclose( fp );
		fp = NULL;
	}
}

void UserStream :: CreatePath( char *path )
{
	char *ofs, save;

	for( ofs = path+1; *ofs; ofs++ )
	{
		if( *ofs == '/' || *ofs == '\\' )
		{
			// create the directory
			save = *ofs;
			*ofs = 0;
			_mkdir( path );
			*ofs = save;
		}
	}
}

// Loading API
uint32_t UserStream::read( void *outbuf, uint32_t size )
{
	if( size == 0 ) 
		return 0;

#ifdef _DEBUG
	// in case we failed to loading file
	memset( outbuf, 0x00, size );
#endif
	if( !buffer || !outbuf ) 
		return 0;

	// check for enough room
	if( m_iOffset >= m_iLength )
		return 0; // hit EOF

	size_t read_size = 0;

	if( m_iOffset + size <= m_iLength )
	{
		memcpy( outbuf, buffer + m_iOffset, size );
		(size_t)m_iOffset += size;
		read_size = size;
	}
	else
	{
		size_t reduced_size = m_iLength - m_iOffset;
		memcpy( outbuf, buffer + m_iOffset, reduced_size );
		(size_t)m_iOffset += reduced_size;
		read_size = reduced_size;
		ALERT( at_warning, "readBuffer: buffer is overrun\n" );
	}

	return read_size;
}

// Saving API
uint32_t UserStream::write( const void *buffer, uint32_t size )
{
	size_t w = fwrite( buffer, size, 1, fp );
	return w;
}

MemoryWriteBuffer::MemoryWriteBuffer() : currentSize(0), maxSize(0), data(NULL)
{
}

MemoryWriteBuffer::~MemoryWriteBuffer()
{
	free( data );
}

uint32_t MemoryWriteBuffer::write( const void *buffer, uint32_t size )
{
	PxU32 expectedSize = currentSize + size;
	if( expectedSize > maxSize )
	{
		maxSize = expectedSize + 4096;

		PxU8 *newData = (PxU8 *)malloc( maxSize );
		if( data )
		{
			memcpy( newData, data, currentSize );
			free( data );
		}
		data = newData;
	}

	memcpy( data + currentSize, buffer, size );
	currentSize += size;
	return size;
}

MemoryReadBuffer::MemoryReadBuffer(const PxU8* data) : buffer(data)
{
}

MemoryReadBuffer::~MemoryReadBuffer()
{
	// We don't own the data => no delete
}

uint32_t MemoryReadBuffer::read(void *dest, uint32_t count)
{
	memcpy(dest, buffer, count);
	buffer += count;
	return count;
}

#endif // USE_PHYSICS_ENGINE
