/*
io_streams.h - i/o stream classes that used in PhysX for cooking
Copyright (C) 2012 Uncle Mike
Copyright (C) 2023 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#include "filesystem_utils.h"
#include <PxConfig.h>
#include <PxIO.h>
#include <PxSimpleTypes.h>
#include <vector>

class UserStream : public physx::PxInputStream, public physx::PxOutputStream
{
public:
	UserStream(const char *filename, bool precacheToMemory);
	virtual ~UserStream();

	virtual uint32_t read(void *outputBuf, uint32_t size);
	virtual uint32_t write(const void *inputBuf, uint32_t size);

private:
	bool m_fileCached;
	size_t m_offset;
	std::vector<uint8_t> m_dataBuffer;
	fs::File m_outputFile;
};

class MemoryWriteBuffer : public physx::PxOutputStream
{
public:
	MemoryWriteBuffer();
	virtual ~MemoryWriteBuffer();

	virtual uint32_t write(const void *inputBuf, uint32_t count);
	const uint8_t *getData() const;
	size_t getSize() const;

private:
	size_t m_currentSize;
	std::vector<uint8_t> m_dataBuffer;
};

class MemoryReadBuffer : public physx::PxInputStream
{
public:
	MemoryReadBuffer(const uint8_t *data, size_t dataSize);
	virtual ~MemoryReadBuffer();

	virtual uint32_t read(void *dest, uint32_t count);

private:
	size_t m_dataSize;
	size_t m_dataOffset;
	mutable const uint8_t *m_buffer;
};
