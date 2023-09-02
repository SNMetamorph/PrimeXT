/*
filesystem_manager.cpp - class that encapsulates filesystem state & initialization
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

#include "filesystem_manager.h"
#include "port.h"
#include "assert.h"

#ifdef _WIN32
using dllhandle_t = HMODULE;
#else
using dllhandle_t = void*;
#endif

using pfnCreateInterface_t = void *(*)(const char *interfaceName, int *retval);

bool fs::CFilesystemManager::Initialize()
{
	assert(m_pFileSystem == nullptr);
	dllhandle_t filesystemModule = LoadLibrary("filesystem_stdio." OS_LIB_EXT);
	if (!filesystemModule) {
		return false;
	}

	pfnCreateInterface_t createInterfaceFn = reinterpret_cast<pfnCreateInterface_t>(
		GetProcAddress(filesystemModule, "CreateInterface")
	);

	if (!createInterfaceFn) {
		return false;
	}

	void *interfacePtr = createInterfaceFn(FILESYSTEM_INTERFACE_VERSION, nullptr);
	if (!interfacePtr) {
		return false;
	}

	m_pFileSystem = reinterpret_cast<IVFileSystem009*>(interfacePtr);
	return true;
}

IVFileSystem009 *fs::CFilesystemManager::GetInterface()
{
	assert(m_pFileSystem != nullptr);
	return m_pFileSystem;
}
