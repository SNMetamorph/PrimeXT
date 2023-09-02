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

fs::FileHandle fs::Open(const Path &filePath, const char *mode)
{
	return g_FileSystemManager.GetInterface()->Open(filePath.string().c_str(), mode, "GAME");
}

void fs::Close(FileHandle fileHandle)
{
	g_FileSystemManager.GetInterface()->Close(fileHandle);
}

void fs::Seek(FileHandle fileHandle, int32_t offset, SeekType type)
{
	FileSystemSeek_t seekType;
	switch (type)
	{
		case fs::SeekType::Set:
			seekType = FILESYSTEM_SEEK_HEAD;
			break;
		case fs::SeekType::Current:
			seekType = FILESYSTEM_SEEK_CURRENT;
			break;
		case fs::SeekType::End:
			seekType = FILESYSTEM_SEEK_TAIL;
			break;
	}
	g_FileSystemManager.GetInterface()->Seek(fileHandle, offset, seekType);
}

void fs::Flush(FileHandle fileHandle)
{
	g_FileSystemManager.GetInterface()->Flush(fileHandle);
}

int32_t fs::Tell(FileHandle fileHandle)
{
	return g_FileSystemManager.GetInterface()->Tell(fileHandle);
}

int32_t fs::Size(FileHandle fileHandle)
{
	return g_FileSystemManager.GetInterface()->Size(fileHandle);
}

bool fs::EndOfFile(FileHandle fileHandle)
{
	return g_FileSystemManager.GetInterface()->EndOfFile(fileHandle);
}

int32_t fs::Read(void *buffer, int32_t size, FileHandle fileHandle)
{
	return g_FileSystemManager.GetInterface()->Read(buffer, size, fileHandle);
}

int32_t fs::Write(void *buffer, int32_t size, FileHandle fileHandle)
{
	return g_FileSystemManager.GetInterface()->Write(buffer, size, fileHandle);
}

const char *fs::ReadLine(char *buffer, int32_t size, FileHandle fileHandle)
{
	return g_FileSystemManager.GetInterface()->ReadLine(buffer, size, fileHandle);
}
