/*
filesystem_manager.h - class that encapsulates filesystem state & initialization
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
#include "VFileSystem009.h"

namespace fs
{
	class CFilesystemManager
	{
	public:
		CFilesystemManager() = default;
		~CFilesystemManager() = default;

		bool Initialize();
		IVFileSystem009 *GetInterface();

	private:
		IVFileSystem009 *m_pFileSystem = nullptr;
	};
}
