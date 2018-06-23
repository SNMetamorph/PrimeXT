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

#include "extdll.h"
#include "util.h"
#include "Nxf.h"
#include "NxSimpleTypes.h"
#include "NxUserStream.h"
#include <direct.h>

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
NxU8 UserStream :: readByte() const
{
	NxU8 b;
	readBuffer( &b, sizeof( NxU8 ));
	return b;
}

NxU16 UserStream :: readWord() const
{
	NxU16 w;
	readBuffer( &w, sizeof( NxU16 ));
	return w;
}

NxU32 UserStream :: readDword() const
{
	NxU32 d;
	readBuffer( &d, sizeof( NxU32 ));
	return d;
}

float UserStream :: readFloat() const
{
	NxReal f;
	readBuffer( &f, sizeof( NxReal ));
	return f;
}

double UserStream :: readDouble() const
{
	NxF64 f;
	readBuffer( &f, sizeof( NxF64 ));
	return f;
}

void UserStream :: readBuffer( void *outbuf, NxU32 size ) const
{
	if( size == 0 ) return;

#ifdef _DEBUG
	// in case we failed to loading file
	memset( outbuf, 0x00, size );
#endif
	if( !buffer || !outbuf ) return;

	// check for enough room
	if( m_iOffset >= m_iLength )
		return; // hit EOF

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

	NX_ASSERT(read_size);
}

// Saving API
NxStream& UserStream :: storeByte( NxU8 b )
{
	size_t w = fwrite( &b, sizeof( NxU8 ), 1, fp );
	NX_ASSERT( w );

	return *this;
}

NxStream& UserStream :: storeWord( NxU16 w )
{
	size_t ww = fwrite( &w, sizeof( NxU16 ), 1, fp );
	NX_ASSERT( ww );
	return *this;
}

NxStream& UserStream :: storeDword( NxU32 d )
{
	size_t w = fwrite( &d, sizeof( NxU32 ), 1, fp );
	NX_ASSERT( w );

	return *this;
}

NxStream& UserStream :: storeFloat( NxReal f )
{
	size_t w = fwrite( &f, sizeof( NxReal ), 1, fp );
	NX_ASSERT( w );

	return *this;
}

NxStream& UserStream :: storeDouble( NxF64 f )
{
	size_t w = fwrite( &f, sizeof( NxF64 ), 1, fp );
	NX_ASSERT( w );

	return *this;
}

NxStream& UserStream :: storeBuffer( const void *buffer, NxU32 size )
{
	size_t w = fwrite( buffer, size, 1, fp );
	NX_ASSERT( w );

	return *this;
}

MemoryWriteBuffer :: MemoryWriteBuffer() : currentSize(0), maxSize(0), data(NULL)
{
}

MemoryWriteBuffer :: ~MemoryWriteBuffer()
{
	free( data );
}

NxStream& MemoryWriteBuffer :: storeByte (NxU8 b )
{
	storeBuffer( &b, sizeof( NxU8 ));
	return *this;
}

NxStream& MemoryWriteBuffer :: storeWord( NxU16 w )
{
	storeBuffer( &w, sizeof( NxU16 ));
	return *this;
}

NxStream& MemoryWriteBuffer :: storeDword( NxU32 d )
{
	storeBuffer( &d, sizeof( NxU32 ));
	return *this;
}

NxStream& MemoryWriteBuffer :: storeFloat( NxReal f )
{
	storeBuffer( &f, sizeof( NxReal ));
	return *this;
}

NxStream& MemoryWriteBuffer :: storeDouble( NxF64 f )
{
	storeBuffer( &f, sizeof( NxF64 ));
	return *this;
}

NxStream& MemoryWriteBuffer :: storeBuffer( const void *buffer, NxU32 size )
{
	NxU32 expectedSize = currentSize + size;

	if( expectedSize > maxSize )
	{
		maxSize = expectedSize + 4096;

		NxU8 *newData = (NxU8 *)malloc( maxSize );
		if( data )
		{
			memcpy( newData, data, currentSize );
			free( data );
		}
		data = newData;
	}

	memcpy( data + currentSize, buffer, size );
	currentSize += size;
	return *this;
}

MemoryReadBuffer :: MemoryReadBuffer( const NxU8 *data ) : buffer(data)
{
}

MemoryReadBuffer::~MemoryReadBuffer()
{
	// We don't own the data => no delete
}

NxU8 MemoryReadBuffer :: readByte() const
{
	NxU8 b;
	memcpy( &b, buffer, sizeof( NxU8 ));
	buffer += sizeof( NxU8 );
	return b;
}

NxU16 MemoryReadBuffer :: readWord() const
{
	NxU16 w;
	memcpy( &w, buffer, sizeof( NxU16 ));
	buffer += sizeof( NxU16 );
	return w;
}

NxU32 MemoryReadBuffer :: readDword() const
{
	NxU32 d;
	memcpy( &d, buffer, sizeof( NxU32 ));
	buffer += sizeof( NxU32 );
	return d;
}

float MemoryReadBuffer :: readFloat() const
{
	float f;
	memcpy( &f, buffer, sizeof( float ));
	buffer += sizeof( float );
	return f;
}

double MemoryReadBuffer :: readDouble() const
{
	double f;
	memcpy( &f, buffer, sizeof( double ));
	buffer += sizeof( double );
	return f;
}

void MemoryReadBuffer :: readBuffer( void *dest, NxU32 size ) const
{
	memcpy( dest, buffer, size );
	buffer += size;
}

