/*
vertex_fmt.h - VBO-stored vertex formats
this code written for Paranoia 2: Savior modification
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

#ifndef VERTEX_FMT_H
#define VERTEX_FMT_H

#ifdef _MSC_VER
#define no_align	__declspec(align(1))
#else
#define no_align
#endif
// name specific:
// fisrt letter is model type: B - BrushModel, G - Grass, D - Decal, S - StudioModel
// next four letters is always equal "vert" for more readability
// next is always underline
// next two symbols is vertex version v0 - v9
// next is always underline
// next four symbols is a target OpenGL version (gl21 - for very old machines that doesn't support GL3.0, gl30 - for modern hardware)

/*
=============================================================

	BMODEL VERTEXES FORMAT

=============================================================
*/

// 100 bytes here
typedef struct
{
	float		vertex[3];		// position
	float		tangent[3];		// tangent
	float		binormal[3];		// binormal
	float		normal[3];		// normal
	float		stcoord0[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords for styles 0-1
	float		lmcoord1[4];		// LM texture coords for styles 2-3
	byte		styles[MAXLIGHTMAPS];	// light styles
} bvert_v0_gl21_t;

#pragma pack(1)
// 84 bytes here
no_align typedef struct
{
	float		vertex[3];		// position
	char		tangent[3];		// tangent
	char		binormal[3];		// binormal
	char		normal[3];		// normal
	float		stcoord0[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords for styles 0-1
	float		lmcoord1[4];		// LM texture coords for styles 2-3
	byte		styles[MAXLIGHTMAPS];	// light styles
	byte		lights0[4];		// packed light numbers
	byte		lights1[4];		// packed light numbers
} bvert_v0_gl30_t;
#pragma pack()
/*
=============================================================

	GRASS VERTEXES FORMAT

=============================================================
*/

// version 0. no lightvectors
// 52 bytes
typedef struct
{
	float		center[4];		// used for rescale
	float		normal[4];		// center + vertex[2] * vertex[3];
	float		light[MAXLIGHTMAPS];	// packed color + unused entry
	byte		styles[MAXLIGHTMAPS];	// styles on surface
} gvert_v0_gl21_t;

// 32 bytes
typedef struct
{
	short		center[4];		// used for rescale
	char		normal[4];		// center + vertex[2] * vertex[3];
	float		light[MAXLIGHTMAPS];	// packed color + unused entry
	byte		styles[MAXLIGHTMAPS];	// styles on surface
} gvert_v0_gl30_t;

// version 1. width lightvectors (ugly case)
// 68 bytes
typedef struct
{
	float		center[4];		// used for rescale
	float		normal[4];		// center + vertex[2] * vertex[3];
	float		light[MAXLIGHTMAPS];	// packed color + unused entry
	float		delux[MAXLIGHTMAPS];	// packed lightdir + unused entry
	byte		styles[MAXLIGHTMAPS];	// styles on surface
} gvert_v1_gl21_t;

// 48 bytes
typedef struct
{
	short		center[4];		// used for rescale
	char		normal[4];		// center + vertex[2] * vertex[3];
	float		light[MAXLIGHTMAPS];	// packed color + unused entry
	float		delux[MAXLIGHTMAPS];	// packed lightdir + unused entry
	byte		styles[MAXLIGHTMAPS];	// styles on surface
} gvert_v1_gl30_t;

/*
=============================================================

	STUDIO VERTEXES FORMAT

=============================================================
*/

// version 0. no bump, no boneweights, no vertexlight
// 44 bytes
typedef struct
{
	float		vertex[3];		// position
	float		stcoord[4];		// ST texture coords
	float		normal[3];		// normal
	float		boneid;			// control bones
} svert_v0_gl21_t;
#pragma pack(1)
// 24 bytes
no_align typedef struct
{
	float		vertex[3];		// position
	short		stcoord[4];		// ST texture coords
	char		normal[3];		// normal
	char		boneid;			// control bones
} svert_v0_gl30_t;
#pragma pack()
// version 1. have bump, no boneweights, no vertexlight
// 68 bytes
typedef struct
{
	float		vertex[3];		// position
	float		stcoord[4];		// ST texture coords
	float		normal[3];		// normal
	float		tangent[3];		// tangent
	float		binormal[3];		// binormal
	float		boneid;			// control bones
} svert_v1_gl21_t;

#pragma pack(1)
// 32 bytes
no_align typedef struct
{
	float		vertex[3];		// position
	short		stcoord[4];		// ST texture coords
	char		normal[3];		// normal
	char		tangent[3];		// tangent
	char		binormal[3];		// binormal
	char		boneid;			// control bones
} svert_v1_gl30_t;
#pragma pack()

// version 2. no bump, single bone, has vertex lighting
// 56 bytes
typedef struct
{
	float		vertex[3];		// position
	float		stcoord[4];		// ST texture coords
	float		normal[3];		// normal
	float		light[MAXLIGHTMAPS];	// packed color
} svert_v2_gl21_t;
#pragma pack(1)

// 40 bytes
no_align typedef struct
{
	float		vertex[3];		// position
	short		stcoord[4];		// ST texture coords
	char		normal[3];		// normal
	float		light[MAXLIGHTMAPS];	// packed color
} svert_v2_gl30_t;
#pragma pack()

// version 3. have bump, single bone, has vertex lighting
// 96 bytes
typedef struct
{
	float		vertex[3];		// position
	float		stcoord[4];		// ST texture coords
	float		light[MAXLIGHTMAPS];	// packed color
	float		deluxe[MAXLIGHTMAPS];	// packed lightdir
	float		normal[3];		// normal
	float		tangent[3];		// tangent
	float		binormal[3];		// binormal
} svert_v3_gl21_t;
#pragma pack(1)

// 64 bytes
no_align typedef struct
{
	float		vertex[3];		// position
	short		stcoord[4];		// ST texture coords
	float		light[MAXLIGHTMAPS];	// packed color
	float		deluxe[MAXLIGHTMAPS];	// packed lightdir
	char		normal[3];		// normal
	char		tangent[3];		// tangent
	char		binormal[3];		// binormal
} svert_v3_gl30_t;
#pragma pack()

// version 4. no bump, have boneweights, no vertexlight
// 72 bytes
typedef struct
{
	float		vertex[3];		// position
	float		stcoord[4];		// ST texture coords
	float		normal[3];		// normal
	float		boneid[4];		// control bones
	float		weight[4];		// boneweights
} svert_v4_gl21_t;

#pragma pack(1)

// 32 bytes
no_align typedef struct
{
	float		vertex[3];		// position
	short		stcoord[4];		// ST texture coords
	char		normal[3];		// normal
	char		boneid[4];		// control bones
	byte		weight[4];		// boneweights
} svert_v4_gl30_t;
#pragma pack()

// version 5. have bump, have boneweights, no vertexlight
// 96 bytes
typedef struct
{
	float		vertex[3];		// position
	float		stcoord[4];		// ST texture coords
	float		normal[3];		// normal
	float		tangent[3];		// tangent
	float		binormal[3];		// binormal
	float		boneid[4];		// control bones
	float		weight[4];		// boneweights
} svert_v5_gl21_t;

#pragma pack(1)
// 40 bytes
no_align typedef struct
{
	float		vertex[3];		// position
	short		stcoord[4];		// ST texture coords
	char		normal[3];		// normal
	char		tangent[3];		// tangent
	char		binormal[3];		// binormal
	char		boneid[4];		// control bones
	byte		weight[4];		// boneweights
} svert_v5_gl30_t;
#pragma pack()

// version 6. no bump, single bone, has lightmaps
// 76 bytes
typedef struct
{
	float		vertex[3];		// position
	float		stcoord0[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords 0-1
	float		lmcoord1[4];		// LM texture coords 2-3
	float		normal[3];		// normal
	byte		styles[MAXLIGHTMAPS];	// light styles
} svert_v6_gl21_t;

#pragma pack(1)

// 60 bytes
no_align typedef struct
{
	float		vertex[3];		// position
	short		stcoord0[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords 0-1
	float		lmcoord1[4];		// LM texture coords 2-3
	char		normal[3];		// normal
	byte		styles[MAXLIGHTMAPS];	// light styles
} svert_v6_gl30_t;
#pragma pack()

// version 7. have bump, single bone, has lightmaps
// 100 bytes
typedef struct
{
	float		vertex[3];		// position
	float		stcoord0[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords 0-1
	float		lmcoord1[4];		// LM texture coords 2-3
	float		normal[3];		// normal
	float		tangent[3];		// tangent
	float		binormal[3];		// binormal
	byte		styles[MAXLIGHTMAPS];	// light styles
} svert_v7_gl21_t;

#pragma pack(1)
// 68 bytes
no_align typedef struct
{
	float		vertex[3];		// position
	short		stcoord0[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords 0-1
	float		lmcoord1[4];		// LM texture coords 2-3
	char		normal[3];		// normal
	char		tangent[3];		// tangent
	char		binormal[3];		// binormal
	byte		styles[MAXLIGHTMAPS];	// light styles
} svert_v7_gl30_t;
#pragma pack()

// version 8. includes all posible combination, slowest
// 164 bytes here
typedef struct
{
	float		vertex[3];		// position
	float		stcoord0[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords 0-1
	float		lmcoord1[4];		// LM texture coords 2-3
	float		normal[3];		// normal
	float		tangent[3];		// tangent
	float		binormal[3];		// binormal
	float		boneid[4];		// control bones
	float		weight[4];		// boneweights
	float		light[MAXLIGHTMAPS];	// packed color
	float		deluxe[MAXLIGHTMAPS];	// packed lightdir
	byte		styles[MAXLIGHTMAPS];	// light styles
} svert_v8_gl21_t;

#pragma pack(1)

// 108 bytes here
no_align typedef struct
{
	float		vertex[3];		// position
	short		stcoord0[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords 0-1
	float		lmcoord1[4];		// LM texture coords 2-3
	char		normal[3];		// normal
	char		tangent[3];		// tangent
	char		binormal[3];		// binormal
	char		boneid[4];		// control bones		
	byte		weight[4];		// boneweights
	float		light[MAXLIGHTMAPS];	// packed color
	float		deluxe[MAXLIGHTMAPS];	// packed lightdir
	byte		styles[MAXLIGHTMAPS];	// light styles
} svert_v8_gl30_t;
#pragma pack()

#endif//VERTEX_FMT_H