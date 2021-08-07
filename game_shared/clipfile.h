/*
clipfile.h - studio cached clip geometry
Copyright (C) 2019 Uncle Mike

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

#define IDCLIPHEADER		(('P'<<24)+('I'<<16)+('L'<<8)+'C') // little-endian "CLIP"
#define CLIP_VERSION		1

// quake lump ordering
#define LUMP_CLIP_FACETS		0
#define LUMP_CLIP_PLANES		1
#define LUMP_CLIP_PLANE_INDEXES	2
// for future expansions
#define LUMP_COUNT			8

typedef struct
{
	int	fileofs;
	int	filelen;
} dcachelump_t;

typedef struct
{
	int		id;		// must be little endian STCH
	int		version;
	unsigned int	modelCRC;		// catch for model changes
	dcachelump_t	lumps[LUMP_COUNT];	
} dcachehdr_t;

typedef struct
{
	short		skinref;		// pointer to texture for special effects
	mvert_t		triangle[3];	// store triangle points
	vec3_t		mins, maxs;	// an individual size of each facet
	vec3_t		edge1, edge2;	// new trace stuff
	byte		numplanes;	// because numplanes for each facet can't exceeds MAX_FACET_PLANES!
	uint		firstindex;	// first index into CLIP_PLANE_INDEXES lump
} dfacet_t;

#endif//CLIPFILE_H