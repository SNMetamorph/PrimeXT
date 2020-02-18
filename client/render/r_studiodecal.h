/*
r_studiodecal.h - studio decal rendering
Copyright (C) 2018 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef R_STUDIODECAL_H
#define R_STUDIODECAL_H

enum
{
	DECAL_CLIP_MINUSU	= 0x1,
	DECAL_CLIP_MINUSV	= 0x2,
	DECAL_CLIP_PLUSU	= 0x4,
	DECAL_CLIP_PLUSV	= 0x8,
};

// used for build decal projection
typedef struct
{
	mstudiomaterial_t	*pmaterial;
	int		firstvertex;
	int		numvertices;
} DecalMesh_t;

typedef struct
{
	Vector		m_Position;
	Vector		m_Normal;
	Vector2D		m_TexCoord0;		// decal texture coordinates
	Vector2D		m_TexCoord1;		// mesh diffuse texture coordinates
	byte		m_Light[LM_STYLES][3];	// values from per-vertex lighting
	mstudioboneweight_t	m_BoneWeights;
	word		m_MeshVertexIndex;		// index into the mesh's vertex list
} DecalVertex_t;

typedef struct
{
	int		m_VertCount;		// Number of used vertices
	int		m_Indices[2][7];		// Indices into the clip verts array of the used vertices
	bool		m_Pass;			// Helps us avoid copying the m_Indices array by using double-buffering
	int		m_ClipVertCount;		// Add vertices we've started with and had to generate due to clipping
	DecalVertex_t	m_ClipVerts[16];
	int		m_ClipFlags[16];		// Union of the decal triangle clip flags above for each vert
} DecalClipState_t;

typedef struct
{
	Vector2D		m_UV;
	word		m_VertexIndex;		// index into the DecalVertex_t list
	bool		m_FrontFacing;
	bool		m_InValidArea;
} DecalVertexInfo_t;
	
#endif//R_STUDIODECAL_H
