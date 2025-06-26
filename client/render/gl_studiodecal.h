/*
gl_studiodecal.h - studio decal rendering
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

#ifndef GL_STUDIODECAL_H
#define GL_STUDIODECAL_H
#include "gl_studio_userdata.h"
#include <stdint.h>

enum
{
	DECAL_CLIP_MINUSU	= 0x1,
	DECAL_CLIP_MINUSV	= 0x2,
	DECAL_CLIP_PLUSU	= 0x4,
	DECAL_CLIP_PLUSV	= 0x8,
};

typedef struct
{
	Vector2D	m_UV;
	int32_t		m_VertexIndex;		// index into the DecalVertex_t list
	bool		m_FrontFacing;
	bool		m_InValidArea;
} DecalVertexInfo_t;

// decal entry
typedef struct studiodecal_s
{
	studiodecal_s() :
		flags(0), 
		normal(0.0f), 
		position(0.0f),
		modelpose(0), 
		texinfo(nullptr),
		depth(0), 
		modelmesh(nullptr) {};

	// this part is goes to savelist
	byte			flags;
	Vector			normal;
	Vector			position;
	word			modelpose;	// m_pModelInstance->pose_stamps[modelpose]
	const DecalGroupEntry	*texinfo;		// pointer to decal material

	// history
	int			depth;		// equal for all the decal fragments (used to remove all frgaments)

	// VBO cache
	vbomesh_t			mesh;		// decal private mesh

	// shader cache
	vbomesh_t*		modelmesh;	// pointer to studio mesh who owned decal
	shader_t			forwardScene;
	shader_t			forwardLightSpot;
	shader_t			forwardLightOmni;
	shader_t			forwardLightProj;
} studiodecal_t;
	
#endif//GL_STUDIODECAL_H
