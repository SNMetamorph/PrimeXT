/*
filesystem_utils.h - routines for convenient working with engine filesystem
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
#include <vector>
#include <filesystem>
#include <stdint.h>

namespace fs
{
	using Path = std::filesystem::path;
	using FileHandle = void*;
	enum class SeekType
	{
		Set,
		Current,
		End
	};

	class File
	{
	public:
		File();
		~File();
		File(const Path &filePath, const char *mode);

		// move semantics
		File(File&& other);
		File& operator=(File&& other);

		// restrict copying, because class holds resource
		File(const File&) = delete;
		File& operator=(const File&) = delete;

		bool Open(const Path &filePath, const char *mode);
		void Close();
		void Seek(int32_t offset, SeekType type);
		void Flush();
		int32_t Tell();
		int32_t Size();
		bool EndOfFile();
		bool IsOpen();
		int32_t Read(void *buffer, int32_t size);
		int32_t Write(void *buffer, int32_t size);
		const char *ReadLine(char *buffer, int32_t size);

	private:
		FileHandle m_handle;
	};

	bool Initialize();
	bool FileExists(const Path &filePath);
	void RemoveFile(const Path &filePath);
	bool IsDirectory(const Path &filePath);
	bool LoadFileToBuffer(const Path &filePath, std::vector<uint8_t> &dataBuffer);
}
