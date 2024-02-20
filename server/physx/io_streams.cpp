/*
io_streams.cpp - i/o stream classes that used in PhysX for cooking
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

#ifdef USE_PHYSICS_ENGINE

#include "extdll.h"
#include "util.h"
#include "io_streams.h"

using namespace physx;

UserStream::UserStream(const char *filePath, bool precacheToMemory)
{
	m_offset = 0;
	m_fileCached = precacheToMemory;
	m_outputFile = {};

	if (m_fileCached) {
		fs::LoadFileToBuffer(filePath, m_dataBuffer);
	}
	else
	{
		m_outputFile.Open(filePath, "wb");
		ASSERT(m_outputFile.IsOpen());
	}
}

UserStream::~UserStream()
{
	if (m_fileCached)
	{
		m_offset = 0;
		m_dataBuffer.clear();
	}
	else
	{
		if (m_outputFile.IsOpen()) {
			m_outputFile.Close();
		}
	}
}

uint32_t UserStream::read(void *outputBuf, uint32_t size)
{
	if (size == 0) {
		return 0;
	}

#ifdef _DEBUG
	// in case we failed to loading file
	memset(outputBuf, 0x00, size);
#endif

	size_t bufferLength = m_dataBuffer.size();
	if (!bufferLength || !outputBuf)
		return 0;

	// check for enough room
	if (m_offset >= bufferLength) {
		ALERT(at_warning, "UserStream::read: precached file buffer overrun\n");
		return 0; // hit EOF
	}

	if (m_offset + size <= bufferLength)
	{
		memcpy(outputBuf, m_dataBuffer.data() + m_offset, size);
		m_offset += size;
		return size;
	}
	else
	{
		ALERT(at_warning, "UserStream::read: precached file buffer overrun\n");
		size_t reducedSize = bufferLength - m_offset;
		memcpy(outputBuf, m_dataBuffer.data() + m_offset, reducedSize);
		m_offset += reducedSize;
		return reducedSize;
	}
}

uint32_t UserStream::write(const void *inputBuf, uint32_t size)
{
	ASSERT(m_outputFile.IsOpen());
	return m_outputFile.Write(const_cast<void*>(inputBuf), size);
}

MemoryWriteBuffer::MemoryWriteBuffer()
{
	m_currentSize = 0;
}

MemoryWriteBuffer::~MemoryWriteBuffer()
{
	m_dataBuffer.clear();
}

uint32_t MemoryWriteBuffer::write(const void *inputBuf, uint32_t size)
{
	const size_t growthSize = 4096;
	size_t expectedSize = m_currentSize + size;
	if (expectedSize > m_dataBuffer.size()) {
		m_dataBuffer.resize(expectedSize + growthSize);
	}
	memcpy(m_dataBuffer.data() + m_currentSize, inputBuf, size);
	m_currentSize += size;
	return size;
}

size_t MemoryWriteBuffer::getSize() const
{
	return m_dataBuffer.size();
}

const uint8_t *MemoryWriteBuffer::getData() const
{
	return m_dataBuffer.data();
}

MemoryReadBuffer::MemoryReadBuffer(const uint8_t *data, size_t dataSize)
{
	m_dataSize = dataSize;
	m_dataOffset = 0;
	m_buffer = data;
}

MemoryReadBuffer::~MemoryReadBuffer()
{
	// We don't own the data => no delete
}

uint32_t MemoryReadBuffer::read(void *outputBuf, uint32_t count)
{
	if (m_dataOffset >= m_dataSize) {
		ALERT(at_warning, "MemoryReadBuffer::read: input buffer is overrun\n");
		return 0;
	}

	size_t bytesToRead = count;
	if (m_dataOffset + count > m_dataSize) {
		ALERT(at_warning, "MemoryReadBuffer::read: input buffer is overrun\n");
		bytesToRead = m_dataSize - m_dataOffset;
	}

	memcpy(outputBuf, m_buffer + m_dataOffset, bytesToRead);
	m_dataOffset += bytesToRead;
	return bytesToRead;
}

#endif // USE_PHYSICS_ENGINE
