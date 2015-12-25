/*
NxUserStream.h - a Novodex physics engine implementation
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

#ifndef NX_USER_STREAM
#define NX_USER_STREAM

#include "NxStream.h"

class UserStream : public NxStream
{
public:
	UserStream( const char *filename, bool load );
	virtual ~UserStream();

	virtual NxU8 readByte() const;
	virtual NxU16 readWord() const;
	virtual NxU32 readDword() const;
	virtual float readFloat() const;
	virtual double readDouble() const;
	virtual void readBuffer( void *outbuf, NxU32 size ) const;

	virtual NxStream& storeByte( NxU8 b );
	virtual NxStream& storeWord( NxU16 w );
	virtual NxStream& storeDword( NxU32 d );
	virtual NxStream& storeFloat( NxReal f );
	virtual NxStream& storeDouble( NxF64 f );
	virtual NxStream& storeBuffer( const void* buffer, NxU32 size );
private:
	bool	load_file; // state
	FILE	*fp;
	byte	*buffer;
	size_t	m_iOffset;
	size_t	m_iLength;

	// misc routines
	void	CreatePath( char *path );
};

class MemoryWriteBuffer : public NxStream
{
public:
	MemoryWriteBuffer();
	virtual ~MemoryWriteBuffer();

	virtual NxU8 readByte() const	{ NX_ASSERT(0); return 0;	}
	virtual NxU16 readWord() const { NX_ASSERT(0); return 0;	}
	virtual NxU32 readDword() const { NX_ASSERT(0); return 0;	}
	virtual float readFloat() const { NX_ASSERT(0); return 0.0f; }
	virtual double readDouble() const { NX_ASSERT(0);	return 0.0; }
	virtual void readBuffer( void *buffer, NxU32 size ) const { NX_ASSERT(0); }

	virtual NxStream& storeByte( NxU8 b );
	virtual NxStream& storeWord( NxU16 w );
	virtual NxStream& storeDword( NxU32 d );
	virtual NxStream& storeFloat( NxReal f );
	virtual NxStream& storeDouble( NxF64 f );
	virtual NxStream& storeBuffer( const void* buffer, NxU32 size );

	NxU32	currentSize;
	NxU32	maxSize;
	NxU8	*data;
};

class MemoryReadBuffer : public NxStream
{
public:
	MemoryReadBuffer( const NxU8* data );
	virtual ~MemoryReadBuffer();

	virtual NxU8 readByte() const;
	virtual NxU16 readWord() const;
	virtual NxU32 readDword() const;
	virtual float readFloat() const;
	virtual double readDouble() const;
	virtual void readBuffer( void* buffer, NxU32 size ) const;

	virtual NxStream& storeByte( NxU8 b ) { NX_ASSERT(0); return *this; }
	virtual NxStream& storeWord( NxU16 w ) { NX_ASSERT(0); return *this; }
	virtual NxStream& storeDword( NxU32 d )	{ NX_ASSERT(0); return *this; }
	virtual NxStream& storeFloat( NxReal f ) { NX_ASSERT(0); return *this; }
	virtual NxStream& storeDouble( NxF64 f ) { NX_ASSERT(0); return *this; }
	virtual NxStream& storeBuffer( const void *buffer, NxU32 size ) { NX_ASSERT(0);	return *this; }

	mutable const NxU8*	buffer;
};

#endif//NX_USER_STREAM
