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

#include "PxIO.h"
#include "PxSimpleTypes.h"

class UserStream : public physx::PxInputStream, public physx::PxOutputStream
{
public:
	UserStream( const char *filename, bool load );
	virtual ~UserStream();

	virtual uint32_t read( void *outbuf, uint32_t size );
	virtual uint32_t write( const void *buffer, uint32_t size );

private:
	bool	load_file; // state
	FILE	*fp;
	byte	*buffer;
	size_t	m_iOffset;
	size_t	m_iLength;

	// misc routines
	void	CreatePath( char *path );
};

class MemoryWriteBuffer : public physx::PxOutputStream
{
public:
	MemoryWriteBuffer();
	virtual ~MemoryWriteBuffer();

	virtual uint32_t write(const void *src, uint32_t count);

	physx::PxU32	currentSize;
	physx::PxU32	maxSize;
	physx::PxU8		*data;
};

class MemoryReadBuffer : public physx::PxInputStream
{
public:
	MemoryReadBuffer(const physx::PxU8 *data);
	virtual ~MemoryReadBuffer();

	virtual uint32_t read(void *dest, uint32_t count);

private:
	mutable const physx::PxU8 *buffer;
};

#endif // NX_USER_STREAM
