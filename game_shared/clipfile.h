/*
clipfile.h - studio cached clip geometry
Copyright (C) 2019 Uncle Mike
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

#ifndef CLIPFILE_H
#define CLIPFILE_H

/*
==============================================================================

CLIP FILES

.clip contain static geometry clip
==============================================================================
*/

#include "bspfile.h"
#include "vector.h"
#include <stdint.h>

typedef struct mvert_s
{
	Vector		point;
	float		st[2];			// for alpha-texture test
} mvert_t;

namespace clipfile
{
	constexpr uint32_t kFormatVersion = 2;
	constexpr uint32_t kFileMagic = (('P' << 24) + ('I' << 16) + ('L' << 8) + 'C'); // little-endian "CLIP"

	constexpr uint32_t kDataLumpFacets = 0;
	constexpr uint32_t kDataLumpPlanes = 1;
	constexpr uint32_t kDataLumpPlaneIndexes = 2;
	constexpr uint32_t kDataLumpCount = 16; // for future expansions

	enum class GeometryType : uint32_t
	{
		Unknown = 0,
		Original,
		CookedBox,
		CookedConvexHull,
		CookedTriangleMesh,
	};

#pragma pack(push, 1)
	struct Header
	{
		uint32_t id;			// must be little endian "CLIP"
		uint32_t version;
		uint32_t modelCRC;	// catch for model changes
		uint32_t tableOffset;
	};

	struct CacheDataLump
	{
		uint32_t fileofs;
		uint32_t filelen;
	};

	struct CacheData
	{
		CacheDataLump lumps[kDataLumpCount];
	};

	struct CacheEntry
	{
		uint32_t dataLength;
		uint32_t dataOffset;
		int32_t body;
		int32_t skin;
		GeometryType geometryType;
	};

	struct CacheTable
	{
		uint32_t entriesCount;
		CacheEntry entries[1];		// [entriesCount];
	};

	using Plane = dplane_t;
	struct Facet
	{
		int16_t		skinref;		// pointer to texture for special effects
		mvert_t		triangle[3];	// store triangle points
		vec3_t		mins, maxs;		// an individual size of each facet
		vec3_t		edge1, edge2;	// new trace stuff
		uint8_t		numplanes;		// because numplanes for each facet can't exceeds MAX_FACET_PLANES!
		uint32_t	firstindex;		// first index into CLIP_PLANE_INDEXES lump
	};
#pragma pack(pop)
};

#endif // CLIPFILE_H
