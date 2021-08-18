/*
bsp31migrate.h - bsp tool header file
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef BSP31MIGRATE_H
#define BSP31MIGRATE_H

#include <windows.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "scriplib.h"
#include "bspfile.h"
#include "wadfile.h"
#include "bspfile31.h"
#include <fcntl.h>
#include <io.h>

typedef enum
{
	MAP_UNKNOWN = 0,		// Unrecognized map type
	MAP_XASH31,		// Like GoldSrc but have HQ Lightmaps (8 texels per luxel) and two additional lumps for clipnodes
} map_type;

typedef enum
{
	MAP_NORMAL = 0,
	MAP_HLFX06,		// HLFX 0.6 expansion
	MAP_XASHXT_OLD,		// extraversion 1
	MAP_P2SAVIOR,		// extraversion 2 has smoothed TBN space, cubemap positions etc
	MAP_DEPRECATED,		// extraversion 3 (not supported)
	MAP_XASH3D_EXT,		// extraversion 4 (generic extension)
} sub_type;

extern int BspConvert( int argc, char **argv );

#endif//BSP31MIGRATE_H