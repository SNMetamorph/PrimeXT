/*
gl_studiovbo.cpp - stored various vertex formats onto GPU
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

#include "hud.h"
#include "utils.h"
#include "gl_local.h"
#include "com_model.h"
#include "r_studioint.h"
#include "pm_movevars.h"
#include "gl_studio.h"
#include "gl_sprite.h"
#include "event_api.h"
#include <mathlib.h>
#include "pm_defs.h"
#include "stringlib.h"
#include "triangleapi.h"
#include "entity_types.h"
#include "gl_shader.h"
#include "gl_world.h"
#include "vertex_fmt.h"

mesh_loader_t CStudioModelRenderer :: m_pfnMeshLoaderGL21[MESHLOADER_COUNT] =
{
{ CreateBufferBaseGL21, BindBufferBaseGL21, "BaseBuffer" },
{ CreateBufferBaseBumpGL21, BindBufferBaseBumpGL21, "BumpBaseBuffer" },
{ CreateBufferVLightGL21, BindBufferVLightGL21, "VertexLightBuffer" },
{ CreateBufferVLightBumpGL21, BindBufferVLightBumpGL21, "BumpVertexLightBuffer" },
{ CreateBufferWeightGL21, BindBufferWeightGL21, "WeightBuffer" },
{ CreateBufferWeightBumpGL21, BindBufferWeightBumpGL21, "WeightBumpBuffer" },
{ CreateBufferLightMapGL21, BindBufferLightMapGL21, "LightMapBuffer" },
{ CreateBufferLightMapBumpGL21, BindBufferLightMapBumpGL21, "BumpLightMapBuffer" },
{ CreateBufferGenericGL21, BindBufferGenericGL21, "GenericBuffer" },
};

mesh_loader_t CStudioModelRenderer :: m_pfnMeshLoaderGL30[MESHLOADER_COUNT] =
{
{ CreateBufferBaseGL30, BindBufferBaseGL30, "BaseBuffer" },
{ CreateBufferBaseBumpGL30, BindBufferBaseBumpGL30, "BumpBaseBuffer" },
{ CreateBufferVLightGL30, BindBufferVLightGL30, "VertexLightBuffer" },
{ CreateBufferVLightBumpGL30, BindBufferVLightBumpGL30, "BumpVertexLightBuffer" },
{ CreateBufferWeightGL30, BindBufferWeightGL30, "WeightBuffer" },
{ CreateBufferWeightBumpGL30, BindBufferWeightBumpGL30, "WeightBumpBuffer" },
{ CreateBufferLightMapGL30, BindBufferLightMapGL30, "LightMapBuffer" },
{ CreateBufferLightMapBumpGL30, BindBufferLightMapBumpGL30, "BumpLightMapBuffer" },
{ CreateBufferGenericGL30, BindBufferGenericGL30, "GenericBuffer" },
};

void CStudioModelRenderer :: CreateBufferBaseGL21( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v0_gl21_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for (uint32_t i = 0; i < pOut->numVerts; i++)
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].stcoord[2] = arrayxvert[i].stcoord[2];
		arraysvert[i].stcoord[3] = arrayxvert[i].stcoord[3];
		arraysvert[i].normal[0] = arrayxvert[i].normal[0];
		arraysvert[i].normal[1] = arrayxvert[i].normal[1];
		arraysvert[i].normal[2] = arrayxvert[i].normal[2];
		arraysvert[i].boneid = arrayxvert[i].boneid[0];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v0_gl21_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferBaseGL21( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v0_gl21_t ), (void *)offsetof( svert_v0_gl21_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v0_gl21_t ), (void *)offsetof( svert_v0_gl21_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v0_gl21_t ), (void *)offsetof( svert_v0_gl21_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 1, GL_FLOAT, GL_FALSE, sizeof( svert_v0_gl21_t ), (void *)offsetof( svert_v0_gl21_t, boneid )); 
}

void CStudioModelRenderer :: CreateBufferBaseGL30( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v0_gl30_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = FloatToHalf( arrayxvert[i].stcoord[0] );
		arraysvert[i].stcoord[1] = FloatToHalf( arrayxvert[i].stcoord[1] );
		arraysvert[i].stcoord[2] = FloatToHalf( arrayxvert[i].stcoord[2] );
		arraysvert[i].stcoord[3] = FloatToHalf( arrayxvert[i].stcoord[3] );
		CompressNormalizedVector( arraysvert[i].normal, arrayxvert[i].normal );
		arraysvert[i].boneid = arrayxvert[i].boneid[0];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v0_gl30_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferBaseGL30( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v0_gl30_t ), (void *)offsetof( svert_v0_gl30_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v0_gl30_t ), (void *)offsetof( svert_v0_gl30_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v0_gl30_t ), (void *)offsetof( svert_v0_gl30_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 1, GL_BYTE, GL_FALSE, sizeof( svert_v0_gl30_t ), (void *)offsetof( svert_v0_gl30_t, boneid )); 
}

void CStudioModelRenderer :: CreateBufferBaseBumpGL21( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v1_gl21_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].stcoord[2] = arrayxvert[i].stcoord[2];
		arraysvert[i].stcoord[3] = arrayxvert[i].stcoord[3];
		arraysvert[i].tangent[0] = arrayxvert[i].tangent[0];
		arraysvert[i].tangent[1] = arrayxvert[i].tangent[1];
		arraysvert[i].tangent[2] = arrayxvert[i].tangent[2];
		arraysvert[i].binormal[0] = arrayxvert[i].binormal[0];
		arraysvert[i].binormal[1] = arrayxvert[i].binormal[1];
		arraysvert[i].binormal[2] = arrayxvert[i].binormal[2];
		arraysvert[i].normal[0] = arrayxvert[i].normal[0];
		arraysvert[i].normal[1] = arrayxvert[i].normal[1];
		arraysvert[i].normal[2] = arrayxvert[i].normal[2];
		arraysvert[i].boneid = arrayxvert[i].boneid[0];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v1_gl21_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferBaseBumpGL21( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v1_gl21_t ), (void *)offsetof( svert_v1_gl21_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v1_gl21_t ), (void *)offsetof( svert_v1_gl21_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v1_gl21_t ), (void *)offsetof( svert_v1_gl21_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v1_gl21_t ), (void *)offsetof( svert_v1_gl21_t, binormal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v1_gl21_t ), (void *)offsetof( svert_v1_gl21_t, tangent ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 1, GL_FLOAT, GL_FALSE, sizeof( svert_v1_gl21_t ), (void *)offsetof( svert_v1_gl21_t, boneid )); 
}

void CStudioModelRenderer :: CreateBufferBaseBumpGL30( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v1_gl30_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = FloatToHalf( arrayxvert[i].stcoord[0] );
		arraysvert[i].stcoord[1] = FloatToHalf( arrayxvert[i].stcoord[1] );
		arraysvert[i].stcoord[2] = FloatToHalf( arrayxvert[i].stcoord[2] );
		arraysvert[i].stcoord[3] = FloatToHalf( arrayxvert[i].stcoord[3] );
		CompressNormalizedVector( arraysvert[i].normal, arrayxvert[i].normal );
		CompressNormalizedVector( arraysvert[i].tangent, arrayxvert[i].tangent );
		CompressNormalizedVector( arraysvert[i].binormal, arrayxvert[i].binormal );
		arraysvert[i].boneid = arrayxvert[i].boneid[0];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v1_gl30_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferBaseBumpGL30( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v1_gl30_t ), (void *)offsetof( svert_v1_gl30_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v1_gl30_t ), (void *)offsetof( svert_v1_gl30_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v1_gl30_t ), (void *)offsetof( svert_v1_gl30_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v1_gl30_t ), (void *)offsetof( svert_v1_gl30_t, binormal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_BYTE, GL_FALSE, sizeof( svert_v1_gl30_t ), (void *)offsetof( svert_v1_gl30_t, tangent ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 1, GL_BYTE, GL_FALSE, sizeof( svert_v1_gl30_t ), (void *)offsetof( svert_v1_gl30_t, boneid )); 
}

void CStudioModelRenderer :: CreateBufferVLightGL21( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v2_gl21_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].stcoord[2] = arrayxvert[i].stcoord[2];
		arraysvert[i].stcoord[3] = arrayxvert[i].stcoord[3];
		arraysvert[i].normal[0] = arrayxvert[i].normal[0];
		arraysvert[i].normal[1] = arrayxvert[i].normal[1];
		arraysvert[i].normal[2] = arrayxvert[i].normal[2];
		arraysvert[i].light[0] = arrayxvert[i].light[0];
		arraysvert[i].light[1] = arrayxvert[i].light[1];
		arraysvert[i].light[2] = arrayxvert[i].light[2];
		arraysvert[i].light[3] = arrayxvert[i].light[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v2_gl21_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferVLightGL21( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v2_gl21_t ), (void *)offsetof( svert_v2_gl21_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v2_gl21_t ), (void *)offsetof( svert_v2_gl21_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v2_gl21_t ), (void *)offsetof( svert_v2_gl21_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v2_gl21_t ), (void *)offsetof( svert_v2_gl21_t, light )); 
}

void CStudioModelRenderer :: CreateBufferVLightGL30( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v2_gl30_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = FloatToHalf( arrayxvert[i].stcoord[0] );
		arraysvert[i].stcoord[1] = FloatToHalf( arrayxvert[i].stcoord[1] );
		arraysvert[i].stcoord[2] = FloatToHalf( arrayxvert[i].stcoord[2] );
		arraysvert[i].stcoord[3] = FloatToHalf( arrayxvert[i].stcoord[3] );
		CompressNormalizedVector( arraysvert[i].normal, arrayxvert[i].normal );
		arraysvert[i].light[0] = arrayxvert[i].light[0];
		arraysvert[i].light[1] = arrayxvert[i].light[1];
		arraysvert[i].light[2] = arrayxvert[i].light[2];
		arraysvert[i].light[3] = arrayxvert[i].light[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v2_gl30_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferVLightGL30( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v2_gl30_t ), (void *)offsetof( svert_v2_gl30_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v2_gl30_t ), (void *)offsetof( svert_v2_gl30_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v2_gl30_t ), (void *)offsetof( svert_v2_gl30_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v2_gl30_t ), (void *)offsetof( svert_v2_gl30_t, light )); 
}

void CStudioModelRenderer :: CreateBufferVLightBumpGL21( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v3_gl21_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].stcoord[2] = arrayxvert[i].stcoord[2];
		arraysvert[i].stcoord[3] = arrayxvert[i].stcoord[3];
		arraysvert[i].light[0] = arrayxvert[i].light[0];
		arraysvert[i].light[1] = arrayxvert[i].light[1];
		arraysvert[i].light[2] = arrayxvert[i].light[2];
		arraysvert[i].light[3] = arrayxvert[i].light[3];
		arraysvert[i].deluxe[0] = arrayxvert[i].deluxe[0];
		arraysvert[i].deluxe[1] = arrayxvert[i].deluxe[1];
		arraysvert[i].deluxe[2] = arrayxvert[i].deluxe[2];
		arraysvert[i].deluxe[3] = arrayxvert[i].deluxe[3];
		arraysvert[i].tangent[0] = arrayxvert[i].tangent[0];
		arraysvert[i].tangent[1] = arrayxvert[i].tangent[1];
		arraysvert[i].tangent[2] = arrayxvert[i].tangent[2];
		arraysvert[i].binormal[0] = arrayxvert[i].binormal[0];
		arraysvert[i].binormal[1] = arrayxvert[i].binormal[1];
		arraysvert[i].binormal[2] = arrayxvert[i].binormal[2];
		arraysvert[i].normal[0] = arrayxvert[i].normal[0];
		arraysvert[i].normal[1] = arrayxvert[i].normal[1];
		arraysvert[i].normal[2] = arrayxvert[i].normal[2];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v3_gl21_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferVLightBumpGL21( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl21_t ), (void *)offsetof( svert_v3_gl21_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl21_t ), (void *)offsetof( svert_v3_gl21_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl21_t ), (void *)offsetof( svert_v3_gl21_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl21_t ), (void *)offsetof( svert_v3_gl21_t, binormal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl21_t ), (void *)offsetof( svert_v3_gl21_t, tangent ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl21_t ), (void *)offsetof( svert_v3_gl21_t, light )); 

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_VECS );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_VECS, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl21_t ), (void *)offsetof( svert_v3_gl21_t, deluxe )); 
}

void CStudioModelRenderer :: CreateBufferVLightBumpGL30( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v3_gl30_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = FloatToHalf( arrayxvert[i].stcoord[0] );
		arraysvert[i].stcoord[1] = FloatToHalf( arrayxvert[i].stcoord[1] );
		arraysvert[i].stcoord[2] = FloatToHalf( arrayxvert[i].stcoord[2] );
		arraysvert[i].stcoord[3] = FloatToHalf( arrayxvert[i].stcoord[3] );
		arraysvert[i].light[0] = arrayxvert[i].light[0];
		arraysvert[i].light[1] = arrayxvert[i].light[1];
		arraysvert[i].light[2] = arrayxvert[i].light[2];
		arraysvert[i].light[3] = arrayxvert[i].light[3];
		arraysvert[i].deluxe[0] = arrayxvert[i].deluxe[0];
		arraysvert[i].deluxe[1] = arrayxvert[i].deluxe[1];
		arraysvert[i].deluxe[2] = arrayxvert[i].deluxe[2];
		arraysvert[i].deluxe[3] = arrayxvert[i].deluxe[3];
		CompressNormalizedVector( arraysvert[i].normal, arrayxvert[i].normal );
		CompressNormalizedVector( arraysvert[i].tangent, arrayxvert[i].tangent );
		CompressNormalizedVector( arraysvert[i].binormal, arrayxvert[i].binormal );
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v3_gl30_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferVLightBumpGL30( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl30_t ), (void *)offsetof( svert_v3_gl30_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v3_gl30_t ), (void *)offsetof( svert_v3_gl30_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v3_gl30_t ), (void *)offsetof( svert_v3_gl30_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v3_gl30_t ), (void *)offsetof( svert_v3_gl30_t, binormal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_BYTE, GL_FALSE, sizeof( svert_v3_gl30_t ), (void *)offsetof( svert_v3_gl30_t, tangent ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl30_t ), (void *)offsetof( svert_v3_gl30_t, light )); 

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_VECS );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_VECS, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v3_gl30_t ), (void *)offsetof( svert_v3_gl30_t, deluxe )); 
}

void CStudioModelRenderer :: CreateBufferWeightGL21( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v4_gl21_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].stcoord[2] = arrayxvert[i].stcoord[2];
		arraysvert[i].stcoord[3] = arrayxvert[i].stcoord[3];
		arraysvert[i].normal[0] = arrayxvert[i].normal[0];
		arraysvert[i].normal[1] = arrayxvert[i].normal[1];
		arraysvert[i].normal[2] = arrayxvert[i].normal[2];
		arraysvert[i].boneid[0] = arrayxvert[i].boneid[0];
		arraysvert[i].boneid[1] = arrayxvert[i].boneid[1];
		arraysvert[i].boneid[2] = arrayxvert[i].boneid[2];
		arraysvert[i].boneid[3] = arrayxvert[i].boneid[3];
		arraysvert[i].weight[0] = arrayxvert[i].weight[0];
		arraysvert[i].weight[1] = arrayxvert[i].weight[1];
		arraysvert[i].weight[2] = arrayxvert[i].weight[2];
		arraysvert[i].weight[3] = arrayxvert[i].weight[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v4_gl21_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferWeightGL21( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v4_gl21_t ), (void *)offsetof( svert_v4_gl21_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v4_gl21_t ), (void *)offsetof( svert_v4_gl21_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v4_gl21_t ), (void *)offsetof( svert_v4_gl21_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v4_gl21_t ), (void *)offsetof( svert_v4_gl21_t, boneid )); 

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_WEIGHTS, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v4_gl21_t ), (void *)offsetof( svert_v4_gl21_t, weight )); 
}

void CStudioModelRenderer :: CreateBufferWeightGL30( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v4_gl30_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = FloatToHalf( arrayxvert[i].stcoord[0] );
		arraysvert[i].stcoord[1] = FloatToHalf( arrayxvert[i].stcoord[1] );
		arraysvert[i].stcoord[2] = FloatToHalf( arrayxvert[i].stcoord[2] );
		arraysvert[i].stcoord[3] = FloatToHalf( arrayxvert[i].stcoord[3] );
		CompressNormalizedVector( arraysvert[i].normal, arrayxvert[i].normal );
		arraysvert[i].boneid[0] = arrayxvert[i].boneid[0];
		arraysvert[i].boneid[1] = arrayxvert[i].boneid[1];
		arraysvert[i].boneid[2] = arrayxvert[i].boneid[2];
		arraysvert[i].boneid[3] = arrayxvert[i].boneid[3];
		arraysvert[i].weight[0] = arrayxvert[i].weight[0];
		arraysvert[i].weight[1] = arrayxvert[i].weight[1];
		arraysvert[i].weight[2] = arrayxvert[i].weight[2];
		arraysvert[i].weight[3] = arrayxvert[i].weight[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v4_gl30_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferWeightGL30( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v4_gl30_t ), (void *)offsetof( svert_v4_gl30_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v4_gl30_t ), (void *)offsetof( svert_v4_gl30_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v4_gl30_t ), (void *)offsetof( svert_v4_gl30_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 4, GL_BYTE, GL_FALSE, sizeof( svert_v4_gl30_t ), (void *)offsetof( svert_v4_gl30_t, boneid )); 

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_WEIGHTS, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( svert_v4_gl30_t ), (void *)offsetof( svert_v4_gl30_t, weight )); 
}

void CStudioModelRenderer :: CreateBufferWeightBumpGL21( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v5_gl21_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].stcoord[2] = arrayxvert[i].stcoord[2];
		arraysvert[i].stcoord[3] = arrayxvert[i].stcoord[3];
		arraysvert[i].tangent[0] = arrayxvert[i].tangent[0];
		arraysvert[i].tangent[1] = arrayxvert[i].tangent[1];
		arraysvert[i].tangent[2] = arrayxvert[i].tangent[2];
		arraysvert[i].binormal[0] = arrayxvert[i].binormal[0];
		arraysvert[i].binormal[1] = arrayxvert[i].binormal[1];
		arraysvert[i].binormal[2] = arrayxvert[i].binormal[2];
		arraysvert[i].normal[0] = arrayxvert[i].normal[0];
		arraysvert[i].normal[1] = arrayxvert[i].normal[1];
		arraysvert[i].normal[2] = arrayxvert[i].normal[2];
		arraysvert[i].boneid[0] = arrayxvert[i].boneid[0];
		arraysvert[i].boneid[1] = arrayxvert[i].boneid[1];
		arraysvert[i].boneid[2] = arrayxvert[i].boneid[2];
		arraysvert[i].boneid[3] = arrayxvert[i].boneid[3];
		arraysvert[i].weight[0] = arrayxvert[i].weight[0];
		arraysvert[i].weight[1] = arrayxvert[i].weight[1];
		arraysvert[i].weight[2] = arrayxvert[i].weight[2];
		arraysvert[i].weight[3] = arrayxvert[i].weight[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v5_gl21_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferWeightBumpGL21( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v5_gl21_t ), (void *)offsetof( svert_v5_gl21_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v5_gl21_t ), (void *)offsetof( svert_v5_gl21_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v5_gl21_t ), (void *)offsetof( svert_v5_gl21_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v5_gl21_t ), (void *)offsetof( svert_v5_gl21_t, binormal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v5_gl21_t ), (void *)offsetof( svert_v5_gl21_t, tangent ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v5_gl21_t ), (void *)offsetof( svert_v5_gl21_t, boneid )); 

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_WEIGHTS, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v5_gl21_t ), (void *)offsetof( svert_v5_gl21_t, weight )); 
}

void CStudioModelRenderer :: CreateBufferWeightBumpGL30( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v5_gl30_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord[0] = FloatToHalf( arrayxvert[i].stcoord[0] );
		arraysvert[i].stcoord[1] = FloatToHalf( arrayxvert[i].stcoord[1] );
		arraysvert[i].stcoord[2] = FloatToHalf( arrayxvert[i].stcoord[2] );
		arraysvert[i].stcoord[3] = FloatToHalf( arrayxvert[i].stcoord[3] );
		CompressNormalizedVector( arraysvert[i].normal, arrayxvert[i].normal );
		CompressNormalizedVector( arraysvert[i].tangent, arrayxvert[i].tangent );
		CompressNormalizedVector( arraysvert[i].binormal, arrayxvert[i].binormal );
		arraysvert[i].boneid[0] = arrayxvert[i].boneid[0];
		arraysvert[i].boneid[1] = arrayxvert[i].boneid[1];
		arraysvert[i].boneid[2] = arrayxvert[i].boneid[2];
		arraysvert[i].boneid[3] = arrayxvert[i].boneid[3];
		arraysvert[i].weight[0] = arrayxvert[i].weight[0];
		arraysvert[i].weight[1] = arrayxvert[i].weight[1];
		arraysvert[i].weight[2] = arrayxvert[i].weight[2];
		arraysvert[i].weight[3] = arrayxvert[i].weight[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v5_gl30_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferWeightBumpGL30( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v5_gl30_t ), (void *)offsetof( svert_v5_gl30_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v5_gl30_t ), (void *)offsetof( svert_v5_gl30_t, stcoord ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v5_gl30_t ), (void *)offsetof( svert_v5_gl30_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v5_gl30_t ), (void *)offsetof( svert_v5_gl30_t, binormal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_BYTE, GL_FALSE, sizeof( svert_v5_gl30_t ), (void *)offsetof( svert_v5_gl30_t, tangent ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 4, GL_BYTE, GL_FALSE, sizeof( svert_v5_gl30_t ), (void *)offsetof( svert_v5_gl30_t, boneid )); 

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS );
	pglVertexAttribPointerARB( ATTR_INDEX_BONE_WEIGHTS, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( svert_v5_gl30_t ), (void *)offsetof( svert_v5_gl30_t, weight )); 
}

void CStudioModelRenderer :: CreateBufferLightMapGL21( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v6_gl21_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord0[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord0[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].stcoord0[2] = arrayxvert[i].stcoord[2];
		arraysvert[i].stcoord0[3] = arrayxvert[i].stcoord[3];
		arraysvert[i].lmcoord0[0] = arrayxvert[i].lmcoord0[0];
		arraysvert[i].lmcoord0[1] = arrayxvert[i].lmcoord0[1];
		arraysvert[i].lmcoord0[2] = arrayxvert[i].lmcoord0[2];
		arraysvert[i].lmcoord0[3] = arrayxvert[i].lmcoord0[3];
		arraysvert[i].lmcoord1[0] = arrayxvert[i].lmcoord1[0];
		arraysvert[i].lmcoord1[1] = arrayxvert[i].lmcoord1[1];
		arraysvert[i].lmcoord1[2] = arrayxvert[i].lmcoord1[2];
		arraysvert[i].lmcoord1[3] = arrayxvert[i].lmcoord1[3];
		arraysvert[i].normal[0] = arrayxvert[i].normal[0];
		arraysvert[i].normal[1] = arrayxvert[i].normal[1];
		arraysvert[i].normal[2] = arrayxvert[i].normal[2];
		arraysvert[i].styles[0] = arrayxvert[i].styles[0];
		arraysvert[i].styles[1] = arrayxvert[i].styles[1];
		arraysvert[i].styles[2] = arrayxvert[i].styles[2];
		arraysvert[i].styles[3] = arrayxvert[i].styles[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v6_gl21_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferLightMapGL21( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v6_gl21_t ), (void *)offsetof( svert_v6_gl21_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v6_gl21_t ), (void *)offsetof( svert_v6_gl21_t, stcoord0 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v6_gl21_t ), (void *)offsetof( svert_v6_gl21_t, lmcoord0 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v6_gl21_t ), (void *)offsetof( svert_v6_gl21_t, lmcoord1 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v6_gl21_t ), (void *)offsetof( svert_v6_gl21_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, 0, sizeof( svert_v6_gl21_t ), (void *)offsetof( svert_v6_gl21_t, styles ));
}

void CStudioModelRenderer :: CreateBufferLightMapGL30( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v6_gl30_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord0[0] = FloatToHalf( arrayxvert[i].stcoord[0] );
		arraysvert[i].stcoord0[1] = FloatToHalf( arrayxvert[i].stcoord[1] );
		arraysvert[i].stcoord0[2] = FloatToHalf( arrayxvert[i].stcoord[2] );
		arraysvert[i].stcoord0[3] = FloatToHalf( arrayxvert[i].stcoord[3] );
		arraysvert[i].lmcoord0[0] = arrayxvert[i].lmcoord0[0];
		arraysvert[i].lmcoord0[1] = arrayxvert[i].lmcoord0[1];
		arraysvert[i].lmcoord0[2] = arrayxvert[i].lmcoord0[2];
		arraysvert[i].lmcoord0[3] = arrayxvert[i].lmcoord0[3];
		arraysvert[i].lmcoord1[0] = arrayxvert[i].lmcoord1[0];
		arraysvert[i].lmcoord1[1] = arrayxvert[i].lmcoord1[1];
		arraysvert[i].lmcoord1[2] = arrayxvert[i].lmcoord1[2];
		arraysvert[i].lmcoord1[3] = arrayxvert[i].lmcoord1[3];
		CompressNormalizedVector( arraysvert[i].normal, arrayxvert[i].normal );
		arraysvert[i].styles[0] = arrayxvert[i].styles[0];
		arraysvert[i].styles[1] = arrayxvert[i].styles[1];
		arraysvert[i].styles[2] = arrayxvert[i].styles[2];
		arraysvert[i].styles[3] = arrayxvert[i].styles[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v6_gl30_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferLightMapGL30( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v6_gl30_t ), (void *)offsetof( svert_v6_gl30_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v6_gl30_t ), (void *)offsetof( svert_v6_gl30_t, stcoord0 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v6_gl30_t ), (void *)offsetof( svert_v6_gl30_t, lmcoord0 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v6_gl30_t ), (void *)offsetof( svert_v6_gl30_t, lmcoord1 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v6_gl30_t ), (void *)offsetof( svert_v6_gl30_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, 0, sizeof( svert_v6_gl30_t ), (void *)offsetof( svert_v6_gl30_t, styles ));
}

void CStudioModelRenderer :: CreateBufferLightMapBumpGL21( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v7_gl21_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord0[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord0[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].stcoord0[2] = arrayxvert[i].stcoord[2];
		arraysvert[i].stcoord0[3] = arrayxvert[i].stcoord[3];
		arraysvert[i].lmcoord0[0] = arrayxvert[i].lmcoord0[0];
		arraysvert[i].lmcoord0[1] = arrayxvert[i].lmcoord0[1];
		arraysvert[i].lmcoord0[2] = arrayxvert[i].lmcoord0[2];
		arraysvert[i].lmcoord0[3] = arrayxvert[i].lmcoord0[3];
		arraysvert[i].lmcoord1[0] = arrayxvert[i].lmcoord1[0];
		arraysvert[i].lmcoord1[1] = arrayxvert[i].lmcoord1[1];
		arraysvert[i].lmcoord1[2] = arrayxvert[i].lmcoord1[2];
		arraysvert[i].lmcoord1[3] = arrayxvert[i].lmcoord1[3];
		arraysvert[i].tangent[0] = arrayxvert[i].tangent[0];
		arraysvert[i].tangent[1] = arrayxvert[i].tangent[1];
		arraysvert[i].tangent[2] = arrayxvert[i].tangent[2];
		arraysvert[i].binormal[0] = arrayxvert[i].binormal[0];
		arraysvert[i].binormal[1] = arrayxvert[i].binormal[1];
		arraysvert[i].binormal[2] = arrayxvert[i].binormal[2];
		arraysvert[i].normal[0] = arrayxvert[i].normal[0];
		arraysvert[i].normal[1] = arrayxvert[i].normal[1];
		arraysvert[i].normal[2] = arrayxvert[i].normal[2];
		arraysvert[i].styles[0] = arrayxvert[i].styles[0];
		arraysvert[i].styles[1] = arrayxvert[i].styles[1];
		arraysvert[i].styles[2] = arrayxvert[i].styles[2];
		arraysvert[i].styles[3] = arrayxvert[i].styles[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v7_gl21_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferLightMapBumpGL21( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl21_t ), (void *)offsetof( svert_v7_gl21_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl21_t ), (void *)offsetof( svert_v7_gl21_t, stcoord0 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl21_t ), (void *)offsetof( svert_v7_gl21_t, lmcoord0 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl21_t ), (void *)offsetof( svert_v7_gl21_t, lmcoord1 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl21_t ), (void *)offsetof( svert_v7_gl21_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl21_t ), (void *)offsetof( svert_v7_gl21_t, binormal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl21_t ), (void *)offsetof( svert_v7_gl21_t, tangent ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, 0, sizeof( svert_v7_gl21_t ), (void *)offsetof( svert_v7_gl21_t, styles ));
}

void CStudioModelRenderer :: CreateBufferLightMapBumpGL30( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v7_gl30_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord0[0] = FloatToHalf( arrayxvert[i].stcoord[0] );
		arraysvert[i].stcoord0[1] = FloatToHalf( arrayxvert[i].stcoord[1] );
		arraysvert[i].stcoord0[2] = FloatToHalf( arrayxvert[i].stcoord[2] );
		arraysvert[i].stcoord0[3] = FloatToHalf( arrayxvert[i].stcoord[3] );
		arraysvert[i].lmcoord0[0] = arrayxvert[i].lmcoord0[0];
		arraysvert[i].lmcoord0[1] = arrayxvert[i].lmcoord0[1];
		arraysvert[i].lmcoord0[2] = arrayxvert[i].lmcoord0[2];
		arraysvert[i].lmcoord0[3] = arrayxvert[i].lmcoord0[3];
		arraysvert[i].lmcoord1[0] = arrayxvert[i].lmcoord1[0];
		arraysvert[i].lmcoord1[1] = arrayxvert[i].lmcoord1[1];
		arraysvert[i].lmcoord1[2] = arrayxvert[i].lmcoord1[2];
		arraysvert[i].lmcoord1[3] = arrayxvert[i].lmcoord1[3];
		CompressNormalizedVector( arraysvert[i].normal, arrayxvert[i].normal );
		CompressNormalizedVector( arraysvert[i].tangent, arrayxvert[i].tangent );
		CompressNormalizedVector( arraysvert[i].binormal, arrayxvert[i].binormal );
		arraysvert[i].styles[0] = arrayxvert[i].styles[0];
		arraysvert[i].styles[1] = arrayxvert[i].styles[1];
		arraysvert[i].styles[2] = arrayxvert[i].styles[2];
		arraysvert[i].styles[3] = arrayxvert[i].styles[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v7_gl30_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferLightMapBumpGL30( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl30_t ), (void *)offsetof( svert_v7_gl30_t, vertex ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v7_gl30_t ), (void *)offsetof( svert_v7_gl30_t, stcoord0 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl30_t ), (void *)offsetof( svert_v7_gl30_t, lmcoord0 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );
	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v7_gl30_t ), (void *)offsetof( svert_v7_gl30_t, lmcoord1 ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v7_gl30_t ), (void *)offsetof( svert_v7_gl30_t, normal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
	pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v7_gl30_t ), (void *)offsetof( svert_v7_gl30_t, binormal ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
	pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_BYTE, GL_FALSE, sizeof( svert_v7_gl30_t ), (void *)offsetof( svert_v7_gl30_t, tangent ));

	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, 0, sizeof( svert_v7_gl30_t ), (void *)offsetof( svert_v7_gl30_t, styles ));
}

void CStudioModelRenderer :: CreateBufferGenericGL21( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v8_gl21_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord0[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord0[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].stcoord0[2] = arrayxvert[i].stcoord[2];
		arraysvert[i].stcoord0[3] = arrayxvert[i].stcoord[3];
		arraysvert[i].lmcoord0[0] = arrayxvert[i].lmcoord0[0];
		arraysvert[i].lmcoord0[1] = arrayxvert[i].lmcoord0[1];
		arraysvert[i].lmcoord0[2] = arrayxvert[i].lmcoord0[2];
		arraysvert[i].lmcoord0[3] = arrayxvert[i].lmcoord0[3];
		arraysvert[i].lmcoord1[0] = arrayxvert[i].lmcoord1[0];
		arraysvert[i].lmcoord1[1] = arrayxvert[i].lmcoord1[1];
		arraysvert[i].lmcoord1[2] = arrayxvert[i].lmcoord1[2];
		arraysvert[i].lmcoord1[3] = arrayxvert[i].lmcoord1[3];
		arraysvert[i].tangent[0] = arrayxvert[i].tangent[0];
		arraysvert[i].tangent[1] = arrayxvert[i].tangent[1];
		arraysvert[i].tangent[2] = arrayxvert[i].tangent[2];
		arraysvert[i].binormal[0] = arrayxvert[i].binormal[0];
		arraysvert[i].binormal[1] = arrayxvert[i].binormal[1];
		arraysvert[i].binormal[2] = arrayxvert[i].binormal[2];
		arraysvert[i].normal[0] = arrayxvert[i].normal[0];
		arraysvert[i].normal[1] = arrayxvert[i].normal[1];
		arraysvert[i].normal[2] = arrayxvert[i].normal[2];
		arraysvert[i].boneid[0] = arrayxvert[i].boneid[0];
		arraysvert[i].boneid[1] = arrayxvert[i].boneid[1];
		arraysvert[i].boneid[2] = arrayxvert[i].boneid[2];
		arraysvert[i].boneid[3] = arrayxvert[i].boneid[3];
		arraysvert[i].weight[0] = arrayxvert[i].weight[0];
		arraysvert[i].weight[1] = arrayxvert[i].weight[1];
		arraysvert[i].weight[2] = arrayxvert[i].weight[2];
		arraysvert[i].weight[3] = arrayxvert[i].weight[3];
		arraysvert[i].light[0] = arrayxvert[i].light[0];
		arraysvert[i].light[1] = arrayxvert[i].light[1];
		arraysvert[i].light[2] = arrayxvert[i].light[2];
		arraysvert[i].light[3] = arrayxvert[i].light[3];
		arraysvert[i].deluxe[0] = arrayxvert[i].deluxe[0];
		arraysvert[i].deluxe[1] = arrayxvert[i].deluxe[1];
		arraysvert[i].deluxe[2] = arrayxvert[i].deluxe[2];
		arraysvert[i].deluxe[3] = arrayxvert[i].deluxe[3];
		arraysvert[i].styles[0] = arrayxvert[i].styles[0];
		arraysvert[i].styles[1] = arrayxvert[i].styles[1];
		arraysvert[i].styles[2] = arrayxvert[i].styles[2];
		arraysvert[i].styles[3] = arrayxvert[i].styles[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v8_gl21_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferGenericGL21( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	if( FBitSet( attrFlags, FATTR_POSITION ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
		pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, vertex ));
	}

	if( FBitSet( attrFlags, FATTR_TEXCOORD0 ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
		pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, stcoord0 ));
	}

	if( FBitSet( attrFlags, FATTR_TEXCOORD1 ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );
		pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, lmcoord0 ));
	}

	if( FBitSet( attrFlags, FATTR_TEXCOORD2 ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );
		pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, lmcoord1 ));
	}

	if( FBitSet( attrFlags, FATTR_NORMAL ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
		pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, normal ));
	}

	if( FBitSet( attrFlags, FATTR_BINORMAL ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
		pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, binormal ));
	}

	if( FBitSet( attrFlags, FATTR_TANGENT ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
		pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, tangent ));
	}

	if( FBitSet( attrFlags, FATTR_BONE_INDEXES ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
		pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, boneid )); 
	}

	if( FBitSet( attrFlags, FATTR_BONE_WEIGHTS ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS );
		pglVertexAttribPointerARB( ATTR_INDEX_BONE_WEIGHTS, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, weight )); 
	}

	if( FBitSet( attrFlags, FATTR_LIGHT_COLOR ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );
		pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, light )); 
	}

	if( FBitSet( attrFlags, FATTR_LIGHT_VECS ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_VECS );
		pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_VECS, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, deluxe )); 
	}

	if( FBitSet( attrFlags, FATTR_LIGHT_STYLES ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
		pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, 0, sizeof( svert_v8_gl21_t ), (void *)offsetof( svert_v8_gl21_t, styles ));
	}
}

void CStudioModelRenderer :: CreateBufferGenericGL30( vbomesh_t *pOut, svert_t *arrayxvert )
{
	std::vector<svert_v8_gl30_t> arraysvert;
	arraysvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraysvert[i].vertex[0] = arrayxvert[i].vertex[0];
		arraysvert[i].vertex[1] = arrayxvert[i].vertex[1];
		arraysvert[i].vertex[2] = arrayxvert[i].vertex[2];
		arraysvert[i].stcoord0[0] = FloatToHalf( arrayxvert[i].stcoord[0] );
		arraysvert[i].stcoord0[1] = FloatToHalf( arrayxvert[i].stcoord[1] );
		arraysvert[i].stcoord0[2] = FloatToHalf( arrayxvert[i].stcoord[2] );
		arraysvert[i].stcoord0[3] = FloatToHalf( arrayxvert[i].stcoord[3] );
		arraysvert[i].lmcoord0[0] = arrayxvert[i].lmcoord0[0];
		arraysvert[i].lmcoord0[1] = arrayxvert[i].lmcoord0[1];
		arraysvert[i].lmcoord0[2] = arrayxvert[i].lmcoord0[2];
		arraysvert[i].lmcoord0[3] = arrayxvert[i].lmcoord0[3];
		arraysvert[i].lmcoord1[0] = arrayxvert[i].lmcoord1[0];
		arraysvert[i].lmcoord1[1] = arrayxvert[i].lmcoord1[1];
		arraysvert[i].lmcoord1[2] = arrayxvert[i].lmcoord1[2];
		arraysvert[i].lmcoord1[3] = arrayxvert[i].lmcoord1[3];
		CompressNormalizedVector( arraysvert[i].normal, arrayxvert[i].normal );
		CompressNormalizedVector( arraysvert[i].tangent, arrayxvert[i].tangent );
		CompressNormalizedVector( arraysvert[i].binormal, arrayxvert[i].binormal );
		arraysvert[i].boneid[0] = arrayxvert[i].boneid[0];
		arraysvert[i].boneid[1] = arrayxvert[i].boneid[1];
		arraysvert[i].boneid[2] = arrayxvert[i].boneid[2];
		arraysvert[i].boneid[3] = arrayxvert[i].boneid[3];
		arraysvert[i].weight[0] = arrayxvert[i].weight[0];
		arraysvert[i].weight[1] = arrayxvert[i].weight[1];
		arraysvert[i].weight[2] = arrayxvert[i].weight[2];
		arraysvert[i].weight[3] = arrayxvert[i].weight[3];
		arraysvert[i].light[0] = arrayxvert[i].light[0];
		arraysvert[i].light[1] = arrayxvert[i].light[1];
		arraysvert[i].light[2] = arrayxvert[i].light[2];
		arraysvert[i].light[3] = arrayxvert[i].light[3];
		arraysvert[i].deluxe[0] = arrayxvert[i].deluxe[0];
		arraysvert[i].deluxe[1] = arrayxvert[i].deluxe[1];
		arraysvert[i].deluxe[2] = arrayxvert[i].deluxe[2];
		arraysvert[i].deluxe[3] = arrayxvert[i].deluxe[3];
		arraysvert[i].styles[0] = arrayxvert[i].styles[0];
		arraysvert[i].styles[1] = arrayxvert[i].styles[1];
		arraysvert[i].styles[2] = arrayxvert[i].styles[2];
		arraysvert[i].styles[3] = arrayxvert[i].styles[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( svert_v8_gl30_t );

	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraysvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void CStudioModelRenderer :: BindBufferGenericGL30( vbomesh_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	if( FBitSet( attrFlags, FATTR_POSITION ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
		pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, vertex ));
	}

	if( FBitSet( attrFlags, FATTR_TEXCOORD0 ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
		pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, stcoord0 ));
	}

	if( FBitSet( attrFlags, FATTR_TEXCOORD1 ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );
		pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, lmcoord0 ));
	}

	if( FBitSet( attrFlags, FATTR_TEXCOORD2 ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );
		pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, lmcoord1 ));
	}

	if( FBitSet( attrFlags, FATTR_NORMAL ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
		pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, normal ));
	}

	if( FBitSet( attrFlags, FATTR_BINORMAL ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_BINORMAL );
		pglVertexAttribPointerARB( ATTR_INDEX_BINORMAL, 3, GL_BYTE, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, binormal ));
	}

	if( FBitSet( attrFlags, FATTR_TANGENT ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TANGENT );
		pglVertexAttribPointerARB( ATTR_INDEX_TANGENT, 3, GL_BYTE, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, tangent ));
	}

	if( FBitSet( attrFlags, FATTR_BONE_INDEXES ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
		pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 4, GL_BYTE, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, boneid )); 
	}

	if( FBitSet( attrFlags, FATTR_BONE_WEIGHTS ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS );
		pglVertexAttribPointerARB( ATTR_INDEX_BONE_WEIGHTS, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, weight )); 
	}

	if( FBitSet( attrFlags, FATTR_LIGHT_COLOR ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );
		pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, light )); 
	}

	if( FBitSet( attrFlags, FATTR_LIGHT_VECS ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_VECS );
		pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_VECS, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, deluxe )); 
	}

	if( FBitSet( attrFlags, FATTR_LIGHT_STYLES ))
	{
		pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
		pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, 0, sizeof( svert_v8_gl30_t ), (void *)offsetof( svert_v8_gl30_t, styles ));
	}
}

void CStudioModelRenderer :: CreateIndexBuffer( vbomesh_t *pOut, unsigned int *arrayelems )
{
	uint cacheSize = pOut->numElems * sizeof( unsigned int ); 

	pglGenBuffersARB( 1, &pOut->ibo );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, pOut->ibo );
	pglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, cacheSize, &arrayelems[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );

	// update total buffer size
	pOut->cacheSize += cacheSize;
}

void CStudioModelRenderer :: BindIndexBuffer( vbomesh_t *pOut )
{
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, pOut->ibo );
}

unsigned int CStudioModelRenderer :: ComputeAttribFlags( int numbones, bool has_bumpmap, bool has_boneweights, bool has_vertexlight, bool has_lightmap )
{
	unsigned int defaultAttribs = (FATTR_POSITION|FATTR_TEXCOORD0|FATTR_NORMAL|FATTR_BONE_INDEXES);

	if( has_boneweights )
		SetBits( defaultAttribs, FATTR_BONE_WEIGHTS );

	if( has_bumpmap )
		SetBits( defaultAttribs, FATTR_BINORMAL|FATTR_TANGENT );

	if( has_vertexlight )
	{
		SetBits( defaultAttribs, FATTR_LIGHT_VECS );
		SetBits( defaultAttribs, FATTR_LIGHT_COLOR );
	}

	if( has_lightmap )
		SetBits( defaultAttribs, FATTR_TEXCOORD1|FATTR_TEXCOORD2|FATTR_LIGHT_STYLES );

	return defaultAttribs;
}

unsigned int CStudioModelRenderer :: SelectMeshLoader( int numbones, bool has_bumpmap, bool has_boneweights, bool has_vertexlight, bool has_lightmap )
{
	if( numbones <= 1 && has_lightmap )
	{
		// special case for single bone vertex lighting
		if( has_bumpmap )
			return MESHLOADER_LIGHTMAPBUMP;
		return MESHLOADER_LIGHTMAP;
	}
	else if( numbones <= 1 && has_vertexlight )
	{
		// special case for single bone vertex lighting
		return MESHLOADER_VLIGHTBUMP;
	}
	else if( !has_boneweights && !has_vertexlight && !has_lightmap )
	{
		// typical GoldSrc models without weights
		if( has_bumpmap )
			return MESHLOADER_BASEBUMP;
		return MESHLOADER_BASE;
	}
	else if( has_boneweights && !has_vertexlight && !has_lightmap )
	{
		// extended Xash3D models with boneweights
		if( has_bumpmap )
			return MESHLOADER_WEIGHTBUMP;
		return MESHLOADER_WEIGHT;
	}
	else
	{
		if( numbones > 1 && ( has_vertexlight || has_lightmap ))
			ALERT( at_aiconsole, "%s static model have skeleton\n", RI->currentmodel->name );
		return MESHLOADER_GENERIC; // all other cases
	}
}