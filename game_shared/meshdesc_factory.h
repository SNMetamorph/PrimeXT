/*
meshdesc_factory.h - class which responsible for creating and caching collision meshes
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
#include "meshdesc.h"
#include "clipfile.h"
#include <stdint.h>
#include <list>

class CMeshDescFactory
{
public:
	static CMeshDescFactory& Instance();
	void ClearCache();
	CMeshDesc& CreateObject(int32_t modelIndex, int32_t body, int32_t skin, clipfile::GeometryType geomType);

private:
	CMeshDescFactory();
	CMeshDescFactory(const CMeshDescFactory&) = delete;
	CMeshDescFactory& operator=(CMeshDescFactory&) = delete;

	std::list<CMeshDesc> m_meshDescList;
};
