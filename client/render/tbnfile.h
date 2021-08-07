/*
tbnfile.h - studio cached TBN
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

#ifndef TBNFILE_H
#define TBNFILE_H

/*
==============================================================================

TBN FILES

.tbn contain precomputed TBN
==============================================================================
*/

#define IDTBNHEADER		(('\0'<<24)+('N'<<16)+('B'<<8)+'T') // little-endian "TBN "
#define TBN_VERSION		1

typedef struct
{
	vec3_t		tangent;
	vec3_t		binormal;
	vec3_t		normal;
} dvertmatrix_t;

typedef struct
{
	int		submodel_offset;	// hack to determine submodel
	int		vertex_offset;
} dvmodelofs_t;

typedef struct
{
	int		ident;		// to differentiate from previous lump LUMP_LEAF_LIGHTING
	int		version;		// data package version
	unsigned int	modelCRC;		// catch for model changes
	int		numverts;
	dvmodelofs_t	submodels[32];	// MAXSTUDIOMODELS
	dvertmatrix_t	verts[1];		// variable sized
} dmodeltbn_t;

#endif//TBNFILE_H