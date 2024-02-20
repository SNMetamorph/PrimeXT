/*
filesystem_utils.cpp - routines for convenient working with engine filesystem
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

#include "filesystem_utils.h"
#include "filesystem_manager.h"

static fs::CFilesystemManager g_FileSystemManager;

fs::File::File() :
	m_handle(nullptr)
{
}

fs::File::~File()
{
	if (m_handle) {
		g_FileSystemManager.GetInterface()->Close(m_handle);
	}
}

fs::File::File(const Path &filePath, const char *mode)
{
	m_handle = g_FileSystemManager.GetInterface()->Open(filePath.string().c_str(), mode, "GAME");
}

fs::File::File(File &&other)
{
	m_handle = other.m_handle;
	other.m_handle = nullptr;
}

fs::File& fs::File::operator=(File &&other)
{
	m_handle = other.m_handle;
	other.m_handle = nullptr;
	return *this;
}

bool fs::File::Open(const Path &filePath, const char *mode)
{
	m_handle = g_FileSystemManager.GetInterface()->Open(filePath.string().c_str(), mode, "GAME");
	return (m_handle != nullptr) ? true : false;
}

void fs::File::Close()
{
	if (m_handle) 
	{
		g_FileSystemManager.GetInterface()->Close(m_handle);
		m_handle = nullptr;
	}
}

void fs::File::Seek(int32_t offset, SeekType type)
{
	if (!m_handle)
		return;

	FileSystemSeek_t seekType;
	switch (type)
	{
		case fs::SeekType::Set:
			seekType = FILESYSTEM_SEEK_HEAD;
			break;
		case fs::SeekType::Current:
			seekType = FILESYSTEM_SEEK_CURRENT;
			break;
		default:
			seekType = FILESYSTEM_SEEK_TAIL;
			break;
	}
	g_FileSystemManager.GetInterface()->Seek(m_handle, offset, seekType);
}

void fs::File::Flush()
{
	if (!m_handle)
		return;

	g_FileSystemManager.GetInterface()->Flush(m_handle);
}

int32_t fs::File::Tell()
{
	if (!m_handle)
		return 0;

	return g_FileSystemManager.GetInterface()->Tell(m_handle);
}

int32_t fs::File::Size()
{
	if (!m_handle)
		return 0;

	return g_FileSystemManager.GetInterface()->Size(m_handle);
}

bool fs::File::EndOfFile()
{
	if (!m_handle)
		return true;

	return g_FileSystemManager.GetInterface()->EndOfFile(m_handle);
}

bool fs::File::IsOpen()
{
	return (m_handle != nullptr) ? true : false;
}

int32_t fs::File::Read(void *buffer, int32_t size)
{
	if (!m_handle)
		return 0;

	return g_FileSystemManager.GetInterface()->Read(buffer, size, m_handle);
}

int32_t fs::File::Write(void *buffer, int32_t size)
{
	if (!m_handle)
		return 0;

	return g_FileSystemManager.GetInterface()->Write(buffer, size, m_handle);
}

const char *fs::File::ReadLine(char *buffer, int32_t size)
{
	if (!m_handle)
		return nullptr;

	return g_FileSystemManager.GetInterface()->ReadLine(buffer, size, m_handle);
}

bool fs::Initialize()
{
	return g_FileSystemManager.Initialize();
}

bool fs::FileExists(const Path &filePath)
{
	return g_FileSystemManager.GetInterface()->FileExists(filePath.string().c_str());
}

void fs::RemoveFile(const Path &filePath)
{
	g_FileSystemManager.GetInterface()->RemoveFile(filePath.string().c_str(), "GAME");
}

bool fs::IsDirectory(const Path &filePath)
{
	return g_FileSystemManager.GetInterface()->IsDirectory(filePath.string().c_str());
}

bool fs::LoadFileToBuffer(const Path &filePath, std::vector<uint8_t> &dataBuffer)
{
	const auto fsInterface = g_FileSystemManager.GetInterface();
	FileHandle_t fileHandle = fsInterface->Open(filePath.string().c_str(), "rb", "GAME");
	if (fileHandle) 
	{
		size_t bufferSize = fsInterface->Size(fileHandle);
		dataBuffer.clear();
		dataBuffer.resize(bufferSize);
		if (fsInterface->Read(dataBuffer.data(), bufferSize, fileHandle) == bufferSize) {
			fsInterface->Close(fileHandle);
			return true;
		}
		fsInterface->Close(fileHandle);
	}
	return false;
}
