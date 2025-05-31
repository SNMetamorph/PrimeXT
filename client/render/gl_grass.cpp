/*
gl_grass.cpp - auto grass system
Copyright (C) 2014 Uncle Mike

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
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_shader.h"
#include "gl_cvars.h"
#include "gl_debug.h"
#include <utlarray.h>
#include <vector>
#include <stringlib.h>
#include "vertex_fmt.h"

#define LEAF_MAX_EXPAND	48.0f
#define DENSITY_FACTOR	0.0001f

grasstexture_t		grasstexs[GRASS_TEXTURES];
CUtlArray<grassentry_t>	grassInfo;

// intermediate arrays used for creating VBO's
static gvert_t		m_arraygvert[MAX_GRASS_VERTS];
static word		m_indexarray[MAX_GRASS_ELEMS];
static int		m_iNumVertex, m_iNumIndex, m_iVertexState;
static int		m_iTotalVerts = 0;
static int		m_iTextureWidth;
static int		m_iTextureHeight;
static float		m_flGrassFadeStart;
static float		m_flGrassFadeDist;
static float		m_flGrassFadeEnd;

static void CreateBufferBaseGL21( grass_t *pOut, gvert_t *arrayxvert )
{
	std::vector<gvert_v0_gl21_t> arraygvert;
	arraygvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraygvert[i].center[0] = arrayxvert[i].center[0];
		arraygvert[i].center[1] = arrayxvert[i].center[1];
		arraygvert[i].center[2] = arrayxvert[i].center[2];
		arraygvert[i].center[3] = arrayxvert[i].center[3];
		arraygvert[i].light[0] = arrayxvert[i].light[0];
		arraygvert[i].light[1] = arrayxvert[i].light[1];
		arraygvert[i].light[2] = arrayxvert[i].light[2];
		arraygvert[i].light[3] = arrayxvert[i].light[3];
		arraygvert[i].normal[0] = arrayxvert[i].normal[0];
		arraygvert[i].normal[1] = arrayxvert[i].normal[1];
		arraygvert[i].normal[2] = arrayxvert[i].normal[2];
		arraygvert[i].normal[3] = arrayxvert[i].normal[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( gvert_v0_gl21_t );

	// create grass static buffer
	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraygvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

static void BindBufferBaseGL21( grass_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v0_gl21_t ), (void *)offsetof( gvert_v0_gl21_t, center ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v0_gl21_t ), (void *)offsetof( gvert_v0_gl21_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v0_gl21_t ), (void *)offsetof( gvert_v0_gl21_t, light )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( gvert_v0_gl21_t ), (void *)offsetof( gvert_v0_gl21_t, styles ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
}

static void CreateBufferBaseGL30( grass_t *pOut, gvert_t *arrayxvert )
{
	std::vector<gvert_v0_gl30_t> arraygvert;
	arraygvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraygvert[i].center[0] = FloatToHalf( arrayxvert[i].center[0] );
		arraygvert[i].center[1] = FloatToHalf( arrayxvert[i].center[1] );
		arraygvert[i].center[2] = FloatToHalf( arrayxvert[i].center[2] );
		arraygvert[i].center[3] = FloatToHalf( arrayxvert[i].center[3] );
		arraygvert[i].light[0] = arrayxvert[i].light[0];
		arraygvert[i].light[1] = arrayxvert[i].light[1];
		arraygvert[i].light[2] = arrayxvert[i].light[2];
		arraygvert[i].light[3] = arrayxvert[i].light[3];
		CompressNormalizedVector( arraygvert[i].normal, arrayxvert[i].normal );
		arraygvert[i].normal[3] = arrayxvert[i].normal[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( gvert_v0_gl30_t );

	// create grass static buffer
	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraygvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

static void BindBufferBaseGL30( grass_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( gvert_v0_gl30_t ), (void *)offsetof( gvert_v0_gl30_t, center ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 4, GL_BYTE, GL_FALSE, sizeof( gvert_v0_gl30_t ), (void *)offsetof( gvert_v0_gl30_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v0_gl30_t ), (void *)offsetof( gvert_v0_gl30_t, light )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( gvert_v0_gl30_t ), (void *)offsetof( gvert_v0_gl30_t, styles ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
}

static void CreateBufferBaseBumpGL21( grass_t *pOut, gvert_t *arrayxvert )
{
	std::vector<gvert_v1_gl21_t> arraygvert;
	arraygvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraygvert[i].center[0] = arrayxvert[i].center[0];
		arraygvert[i].center[1] = arrayxvert[i].center[1];
		arraygvert[i].center[2] = arrayxvert[i].center[2];
		arraygvert[i].center[3] = arrayxvert[i].center[3];
		arraygvert[i].light[0] = arrayxvert[i].light[0];
		arraygvert[i].light[1] = arrayxvert[i].light[1];
		arraygvert[i].light[2] = arrayxvert[i].light[2];
		arraygvert[i].light[3] = arrayxvert[i].light[3];
		arraygvert[i].delux[0] = arrayxvert[i].delux[0];
		arraygvert[i].delux[1] = arrayxvert[i].delux[1];
		arraygvert[i].delux[2] = arrayxvert[i].delux[2];
		arraygvert[i].delux[3] = arrayxvert[i].delux[3];
		arraygvert[i].normal[0] = arrayxvert[i].normal[0];
		arraygvert[i].normal[1] = arrayxvert[i].normal[1];
		arraygvert[i].normal[2] = arrayxvert[i].normal[2];
		arraygvert[i].normal[3] = arrayxvert[i].normal[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( gvert_v1_gl21_t );

	// create grass static buffer
	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->cacheSize, &arraygvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

static void BindBufferBaseBumpGL21( grass_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v1_gl21_t ), (void *)offsetof( gvert_v1_gl21_t, center ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v1_gl21_t ), (void *)offsetof( gvert_v1_gl21_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v1_gl21_t ), (void *)offsetof( gvert_v1_gl21_t, light )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_VECS, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v1_gl21_t ), (void *)offsetof( gvert_v1_gl21_t, delux )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_VECS );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( gvert_v1_gl21_t ), (void *)offsetof( gvert_v1_gl21_t, styles ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
}

static void CreateBufferBaseBumpGL30( grass_t *pOut, gvert_t *arrayxvert )
{
	std::vector<gvert_v1_gl30_t> arraygvert;
	arraygvert.resize(pOut->numVerts);

	// convert to GLSL-compacted array
	for( int i = 0; i < pOut->numVerts; i++ )
	{
		arraygvert[i].center[0] = FloatToHalf( arrayxvert[i].center[0] );
		arraygvert[i].center[1] = FloatToHalf( arrayxvert[i].center[1] );
		arraygvert[i].center[2] = FloatToHalf( arrayxvert[i].center[2] );
		arraygvert[i].center[3] = FloatToHalf( arrayxvert[i].center[3] );
		arraygvert[i].light[0] = arrayxvert[i].light[0];
		arraygvert[i].light[1] = arrayxvert[i].light[1];
		arraygvert[i].light[2] = arrayxvert[i].light[2];
		arraygvert[i].light[3] = arrayxvert[i].light[3];
		arraygvert[i].delux[0] = arrayxvert[i].delux[0];
		arraygvert[i].delux[1] = arrayxvert[i].delux[1];
		arraygvert[i].delux[2] = arrayxvert[i].delux[2];
		arraygvert[i].delux[3] = arrayxvert[i].delux[3];
		CompressNormalizedVector( arraygvert[i].normal, arrayxvert[i].normal );
		arraygvert[i].normal[3] = arrayxvert[i].normal[3];
	}

	pOut->cacheSize = pOut->numVerts * sizeof( gvert_v1_gl30_t );

	// create grass static buffer
	pglGenBuffersARB( 1, &pOut->vbo );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, pOut->numVerts * sizeof( gvert_v1_gl30_t ), &arraygvert[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

static void BindBufferBaseBumpGL30( grass_t *pOut, int attrFlags )
{
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( gvert_v1_gl30_t ), (void *)offsetof( gvert_v1_gl30_t, center ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 4, GL_BYTE, GL_FALSE, sizeof( gvert_v1_gl30_t ), (void *)offsetof( gvert_v1_gl30_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v1_gl30_t ), (void *)offsetof( gvert_v1_gl30_t, light )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_VECS, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_v1_gl30_t ), (void *)offsetof( gvert_v1_gl30_t, delux )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_VECS );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( gvert_v1_gl30_t ), (void *)offsetof( gvert_v1_gl30_t, styles ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
}

static void CreateIndexBuffer( grass_t *pOut, unsigned short *arrayelems )
{
	uint cacheSize = pOut->numElems * sizeof( word ); 

	pglGenBuffersARB( 1, &pOut->ibo );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, pOut->ibo );
	pglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, cacheSize, &arrayelems[0], GL_STATIC_DRAW_ARB );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );

	// update total buffer size
	pOut->cacheSize += cacheSize;
}

static void BindIndexBuffer( grass_t *pOut )
{
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, pOut->ibo );
}

unsigned int SelectGrassLoader( void )
{
	if( FBitSet( world->features, WORLD_HAS_DELUXEMAP ) && r_grass->value > 1.0f )
		return GRASSLOADER_BASEBUMP;
	return GRASSLOADER_BASE;
}

static grass_loader_t pfnMeshLoaderGL21[GRASSLOADER_COUNT] =
{
{ CreateBufferBaseGL21, BindBufferBaseGL21, "BaseBuffer" },
{ CreateBufferBaseBumpGL21, BindBufferBaseBumpGL21, "BumpBaseBuffer" },
};

static grass_loader_t pfnMeshLoaderGL30[GRASSLOADER_COUNT] =
{
{ CreateBufferBaseGL30, BindBufferBaseGL30, "BaseBuffer" },
{ CreateBufferBaseBumpGL30, BindBufferBaseBumpGL30, "BumpBaseBuffer" },
};

/*
================
R_GetPointForBush

compute 16 points for single bush
================
*/
static const Vector R_GetPointForBush( int vertexNum, const Vector &pos, float scale )
{
	float s1 = ( m_iTextureWidth * 0.075f ) * scale;
	float s2 = ( m_iTextureWidth * 0.1f ) * scale;
	float s3 = ( m_iTextureHeight * 0.1f ) * scale;

	switch(( vertexNum & 15 ))
	{
	case 0:  return Vector( pos.x - s2, pos.y, pos.z );
	case 1:  return Vector( pos.x - s2, pos.y, pos.z + s3 );
	case 2:  return Vector( pos.x + s2, pos.y, pos.z + s3 );
	case 3:  return Vector( pos.x + s2, pos.y, pos.z );
	case 4:  return Vector( pos.x, pos.y - s2, pos.z );
	case 5:  return Vector( pos.x, pos.y - s2, pos.z + s3 );
	case 6:  return Vector( pos.x, pos.y + s2, pos.z + s3 );
	case 7:  return Vector( pos.x, pos.y + s2, pos.z );
	case 8:  return Vector( pos.x - s1, pos.y - s1, pos.z );
	case 9:  return Vector( pos.x - s1, pos.y - s1, pos.z + s3 );
	case 10: return Vector( pos.x + s1, pos.y + s1, pos.z + s3 );
	case 11: return Vector( pos.x + s1, pos.y + s1, pos.z );
	case 12: return Vector( pos.x - s1, pos.y + s1, pos.z );
	case 13: return Vector( pos.x - s1, pos.y + s1, pos.z + s3 );
	case 14: return Vector( pos.x + s1, pos.y - s1, pos.z + s3 );
	case 15: return Vector( pos.x + s1, pos.y - s1, pos.z );
	}

	return g_vecZero; // error
}
/*
================
R_GetTexCoordForBush

get fixed texcoord for bush
================
*/
static const Vector2D R_GetTexCoordForBush( int vertexNum )
{
	switch(( vertexNum & 7 ))
	{
	case 0: return Vector2D( 0.0f, 1.0f );
	case 1: return Vector2D( 0.0f, 0.0f );
	case 2: return Vector2D( 1.0f, 0.0f );
	case 3: return Vector2D( 1.0f, 1.0f );
	case 4: return Vector2D( 1.0f, 1.0f );
	case 5: return Vector2D( 1.0f, 0.0f );
	case 6: return Vector2D( 0.0f, 0.0f );
	case 7: return Vector2D( 0.0f, 1.0f );
	}

	return Vector2D( 0.0f, 0.0f ); // error
}

/*
================
R_GetNormalForBush

get fixed normals for single bush
================
*/
static const Vector R_GetNormalForBush( int vertexNum )
{
	switch(( vertexNum & 15 ))
	{
	case 0:  return Vector( 0.0f, 1.0f, 0.0f );
	case 1:  return Vector( 0.0f, 1.0f, 0.0f );
	case 2:  return Vector( 0.0f, 1.0f, 0.0f );
	case 3:  return Vector( 0.0f, 1.0f, 0.0f );
	case 4:  return Vector( -1.0f, 0.0f, 0.0f );
	case 5:  return Vector( -1.0f, 0.0f, 0.0f );
	case 6:  return Vector( -1.0f, 0.0f, 0.0f );
	case 7:  return Vector( -1.0f, 0.0f, 0.0f );
	case 8:  return Vector( -0.707107f, 0.707107f, 0.0f );
	case 9:  return Vector( -0.707107f, 0.707107f, 0.0f );
	case 10: return Vector( -0.707107f, 0.707107f, 0.0f );
	case 11: return Vector( -0.707107f, 0.707107f, 0.0f );
	case 12: return Vector( 0.707107f, 0.707107f, 0.0f );
	case 13: return Vector( 0.707107f, 0.707107f, 0.0f );
	case 14: return Vector( 0.707107f, 0.707107f, 0.0f );
	case 15: return Vector( 0.707107f, 0.707107f, 0.0f );
	}

	return g_vecZero; // error
}

/*
================
R_AdvanceVertex

routine to build quad sequences
================
*/
bool R_GrassAdvanceVertex( void )
{
	if((( m_iNumIndex + 6 ) >= MAX_GRASS_ELEMS ) || (( m_iNumVertex + 4 ) >= MAX_GRASS_VERTS ))
		return false;

	if( m_iVertexState++ < 3 )
	{
		m_indexarray[m_iNumIndex++] = m_iNumVertex;
	}
	else
	{
		// we've already done triangle (0, 1, 2), now draw (2, 3, 0)
		m_indexarray[m_iNumIndex++] = m_iNumVertex - 1;
		m_indexarray[m_iNumIndex++] = m_iNumVertex;
		m_indexarray[m_iNumIndex++] = m_iNumVertex - 3;
		m_iVertexState = 0;
	}
	m_iNumVertex++;

	return true;
}

/*
================
R_GrassLightForVertex

We already have a valid spot on texture
Just find lightmap point and update grass color
================
*/
void R_GrassLightForVertex( msurface_t *fa, mextrasurf_t *es, const Vector &vertex, float posz, float light[MAXLIGHTMAPS], float delux[MAXLIGHTMAPS] )
{
	if( !worldmodel->lightdata || !fa->samples )
		return;

	float sample_size = Mod_SampleSizeForFace( fa );
	Vector pos = Vector( vertex.x, vertex.y, posz ); // drop to floor
	float s = DotProduct( pos, fa->info->lmvecs[0] ) + fa->info->lmvecs[0][3] - fa->info->lightmapmins[0];
	float t = DotProduct( pos, fa->info->lmvecs[1] ) + fa->info->lmvecs[1][3] - fa->info->lightmapmins[1];
	int smax = (fa->info->lightextents[0] / sample_size) + 1;
	int tmax = (fa->info->lightextents[1] / sample_size) + 1;
	int size = smax * tmax;
	int map;

	// some bushes may be out of current poly
	s = bound( 0, s, fa->info->lightextents[0] );
	t = bound( 0, t, fa->info->lightextents[1] );
	s /= sample_size;
	t /= sample_size;

	color24 *lm = fa->samples + Q_rint( t ) * smax + Q_rint( s );

	for( map = 0; map < MAXLIGHTMAPS && fa->styles[map] != 255; map++ )
	{
		color24 out;
		out.r = R_LightToTexGamma( lm->r );
		out.g = R_LightToTexGamma( lm->g );
		out.b = R_LightToTexGamma( lm->b );
		light[map] = PackColor( out );
		lm += size; // skip to next lightmap
	}

	if( !es->normals ) return;

	color24 *dm = es->normals + Q_rint( t ) * smax + Q_rint( s );
	Vector vec_x, vec_y, vec_z;
	matrix3x3	mat;

	// flat TBN for better results
	vec_x = Vector( fa->info->lmvecs[0] );
	vec_y = Vector( fa->info->lmvecs[1] );
	if( FBitSet( fa->flags, SURF_PLANEBACK ))
		vec_z = -fa->plane->normal;
	else vec_z = fa->plane->normal;

	// create tangent space rotational matrix
	mat.SetForward( vec_x.Normalize( ));
	mat.SetRight( -vec_y.Normalize( ));
	mat.SetUp( vec_z.Normalize( ));

	for( map = 0; map < MAXLIGHTMAPS && fa->styles[map] != 255; map++ )
	{
		float f = (1.0f / 128.0f);
		Vector normal = Vector(((float)dm->r - 128.0f) * f, ((float)dm->g - 128.0f) * f, ((float)dm->b - 128.0f) * f);
		delux[map] = NormalToFloat( mat.VectorRotate( normal ));
		dm += size; // skip to next lightmap
	}
}

/*
================
R_CreateSingleBush

create a bush with specified pos
================
*/
bool R_CreateSingleBush( msurface_t *surf, mextrasurf_t *es, grasshdr_t *hdr, const Vector &pos, float size )
{
	for( int i = 0; i < 16; i++ )
	{
		gvert_t *entry = &m_arraygvert[m_iNumVertex];
		Vector vertex = R_GetPointForBush( i, pos, size );
		memcpy( entry->styles, surf->styles, sizeof( entry->styles ));
		R_GrassLightForVertex( surf, es, vertex, pos.z, entry->light, entry->delux );
		AddPointToBounds( vertex, hdr->mins, hdr->maxs ); // build bbox for grass
		Vector dir = ( vertex - pos ).Normalize();
		float scale = ( vertex - pos ).Length();

		entry->center[0] = pos.x;
		entry->center[1] = pos.y;
		entry->center[2] = pos.z;
		entry->center[3] = scale;
		entry->normal[0] = dir.x;
		entry->normal[1] = dir.y;
		entry->normal[2] = dir.z;
		entry->normal[3] = i;

		// generate indices
		if( !R_GrassAdvanceVertex( ))
		{
			// vertexes is out
			return false;
		}
	}

	return true;
}

void R_CreateSurfaceVBO( grass_t *pOut )
{
	if( !m_iNumVertex ) return; // empty mesh?

	GL_CheckVertexArrayBinding();

	// determine optimal mesh loader
	uint attribs = 0; // unused
	uint type = SelectGrassLoader();

	// move data to video memory
	if( glConfig.version < ACTUAL_GL_VERSION )
		pfnMeshLoaderGL21[type].CreateBuffer( pOut, m_arraygvert );
	else pfnMeshLoaderGL30[type].CreateBuffer( pOut, m_arraygvert );
	CreateIndexBuffer( pOut, m_indexarray );

	// link it with vertex array object
	pglGenVertexArrays( 1, &pOut->vao );
	pglBindVertexArray( pOut->vao );
	if( glConfig.version < ACTUAL_GL_VERSION )
		pfnMeshLoaderGL21[type].BindBuffer( pOut, attribs );
	else pfnMeshLoaderGL30[type].BindBuffer( pOut, attribs );
	BindIndexBuffer( pOut );
	pglBindVertexArray( GL_FALSE );

	// update stats
	tr.total_vbo_memory += pOut->cacheSize;
}

void R_DeleteSurfaceVBO( grass_t *pOut )
{
	if( pOut->vao ) pglDeleteVertexArrays( 1, &pOut->vao );
	if( pOut->vbo ) pglDeleteBuffersARB( 1, &pOut->vbo );
	if( pOut->ibo ) pglDeleteBuffersARB( 1, &pOut->ibo );
	tr.total_vbo_memory -= pOut->cacheSize;
	pOut->cacheSize = 0;
}

/*
================
R_BuildGrassMesh

build mesh with single texture
================
*/
bool R_BuildGrassMesh( msurface_t *surf, mextrasurf_t *es, grassentry_t *entry, grasshdr_t *hdr, grass_t *out )
{
	mfaceinfo_t *land = surf->texinfo->faceinfo;
	bvert_t *v0, *v1, *v2;

	// update random set to get predictable positions for grass 'random' placement
	RANDOM_SEED(( surf - worldmodel->surfaces ) * entry->seed );

	m_iNumVertex = m_iNumIndex = m_iVertexState = 0;

	m_iTextureWidth = grasstexs[entry->texture].gl_texturenum.GetWidth();
	m_iTextureHeight = grasstexs[entry->texture].gl_texturenum.GetHeight();

	// turn the face into a bunch of polygons, and compute the area of each
	v0 = &world->vertexes[es->firstvertex];

	for( int i = 1; i < es->numverts - 1; i++ )
	{
		v1 = &world->vertexes[es->firstvertex+i+0];
		v2 = &world->vertexes[es->firstvertex+i+1];

		// compute two triangle edges
		Vector e1 = v1->vertex - v0->vertex;
		Vector e2 = v2->vertex - v0->vertex;

		// compute the triangle area
		Vector areaVec = CrossProduct( e1, e2 );
		float normalLength = areaVec.Length();
		float area = 0.5f * normalLength;

		// compute the number of samples to take
		int numSamples = area * entry->density * DENSITY_FACTOR;

		// now take a sample, and randomly place an object there
		for( int j = 0; j < numSamples; j++ )
		{
			// Create a random sample...
			float u = RANDOM_FLOAT( 0.0f, 1.0f );
			float v = RANDOM_FLOAT( 0.0f, 1.0f );

			if( v > ( 1.0f - u ))
			{
				u = 1.0f - u;
				v = 1.0f - v;
			}

			float size = RANDOM_FLOAT( entry->min, entry->max );

			Vector pos = v0->vertex + e1 * u + e2 * v;

			if( !Mod_CheckLayerNameForPixel( land, pos, entry->name ))
				continue;	// rejected by heightmap

			if( !R_CreateSingleBush( surf, es, hdr, pos, size ))
				goto build_mesh; // vertices is out (more than 2048 bushes per surface created)
		}
	}

	// nothing to added?
	if( !m_iNumVertex ) return false;

build_mesh:
	// give lightnums from surface
	memcpy( out->lights, es->lights, sizeof( byte ) * MAXDYNLIGHTS );
	out->texture = entry->texture;
	out->numVerts = m_iNumVertex;
	out->numElems = m_iNumIndex;
	R_CreateSurfaceVBO( out );

	return true;
}

/*
================
R_ConstructGrassForSurface

compile all the grassdata with
specified texture into single VBO
================
*/
void R_ConstructGrassForSurface( msurface_t *surf, mextrasurf_t *es )
{
	// already init or not specified?
	if( es->grass || !es->grasscount )
		return;

	size_t grasshdr_size = sizeof( grasshdr_t ) + sizeof( grass_t ) * ( es->grasscount - 1 );
	grasshdr_t *hdr = es->grass = (grasshdr_t *)IEngineStudio.Mem_Calloc( 1, grasshdr_size );
	hdr->mins = es->mins;
	hdr->maxs = es->maxs;
	hdr->count = 0;

	for( int i = 0; i < grassInfo.Count(); i++ )
	{
		grassentry_t *entry = &grassInfo[i];

		if( !Mod_CheckLayerNameForSurf( surf, entry->name ))
			continue;

		// create a single mesh for all the bushes that have same texture
		if( !R_BuildGrassMesh( surf, es, entry, hdr, &hdr->g[hdr->count] ))
			continue;	// failed to build for some reasons

		hdr->count++;
	}

	ClearBits( surf->flags, SURF_GRASS_UPDATE );

	// bah! failed to create
	if( !hdr->count )
	{
		Mem_Free( es->grass );
		es->grasscount = 0;
		es->grass = NULL;
	}

	// restore normal randomization
	RANDOM_SEED( 0 );
}

void R_RemoveGrassForSurface( mextrasurf_t *es )
{
	ClearBits( es->surf->flags, SURF_GRASS_UPDATE );

	// not specified?
	if( !es->grass ) return;

	grasshdr_t *hdr = es->grass;

	for( int i = 0; i < hdr->count; i++ )
		R_DeleteSurfaceVBO( &hdr->g[i] );

	es->grass = NULL;
	Mem_Free( hdr );
}

void R_DrawGrassMeshFromBuffer( const grass_t *mesh )
{
	pglBindVertexArray( mesh->vao );

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElements( GL_TRIANGLES, 0, mesh->numVerts - 1, mesh->numElems, GL_UNSIGNED_SHORT, 0 );
	else pglDrawElements( GL_TRIANGLES, mesh->numElems, GL_UNSIGNED_SHORT, 0 );

	r_stats.c_total_tris += (mesh->numVerts / 2);
	r_stats.num_flushes_total++;
}

/*
================
R_SetSurfaceUniforms

================
*/
void R_SetGrassUniforms( word hProgram, grass_t *grass )
{
	Vector	lightdir;
	float	*v;

	if( RI->currentshader != &glsl_programs[hProgram] )
		GL_BindShader( &glsl_programs[hProgram] );

	glsl_program_t *shader = RI->currentshader;
	gl_state_t *glm = GL_GetCache( grass->hCachedMatrix );
	CDynLight *pl = RI->currentlight; // may be NULL

	// sometime we can't set the uniforms
	if( !shader || !shader->numUniforms || !shader->uniforms )
		return;

	// setup specified uniforms (and texture bindings)
	for( int i = 0; i < shader->numUniforms; i++ )
	{
		uniform_t *u = &shader->uniforms[i];

		switch( u->type )
		{
		case UT_COLORMAP:
			u->SetValue( grasstexs[grass->texture].gl_texturenum.ToInt() );
			break;
		case UT_PROJECTMAP:
			if( pl && pl->type == LIGHT_SPOT )
				u->SetValue( pl->spotlightTexture.ToInt() );
			else u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_SHADOWMAP:
		case UT_SHADOWMAP0:
			if( pl ) u->SetValue( pl->shadowTexture[0].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP1:
			if( pl ) u->SetValue( pl->shadowTexture[1].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP2:
			if( pl ) u->SetValue( pl->shadowTexture[2].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP3:
			if( pl ) u->SetValue( pl->shadowTexture[3].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_LIGHTMAP:
		case UT_DELUXEMAP:
		case UT_DECALMAP:
			// unacceptable for grass
			u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_SCREENMAP:
			u->SetValue( tr.screen_color.ToInt() );
			break;
		case UT_DEPTHMAP:
			u->SetValue( tr.screen_depth.ToInt() );
			break;
		case UT_FITNORMALMAP:
			u->SetValue( tr.normalsFitting.ToInt() );
			break;
		case UT_MODELMATRIX:
			u->SetValue( &glm->modelMatrix[0] );
			break;
		case UT_SCREENSIZEINV:
			u->SetValue( 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			break;
		case UT_ZFAR:
			u->SetValue( -tr.farclip * 1.74f );
			break;
		case UT_LIGHTSTYLEVALUES:
			u->SetValue( &tr.lightstyle[0], MAX_LIGHTSTYLES );
			break;
		case UT_REALTIME:
			u->SetValue( (float)tr.time );
			break;
		case UT_FOGPARAMS:
			u->SetValue( tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			break;
		case UT_SHADOWPARMS:
			if( pl != NULL )
			{
				float shadowWidth = 1.0f / (float)pl->shadowTexture[0].GetWidth();
				float shadowHeight = 1.0f / (float)pl->shadowTexture[0].GetHeight();
				// depth scale and bias and shadowmap resolution
				u->SetValue( shadowWidth, shadowHeight, -pl->projectionMatrix[2][2], pl->projectionMatrix[3][2] );
			}
			else u->SetValue( 0.0f, 0.0f, 0.0f, 0.0f );
			break;
		case UT_VIEWORIGIN:
			u->SetValue( tr.cached_vieworigin.x, tr.cached_vieworigin.y, tr.cached_vieworigin.z );
			break;
		case UT_SHADOWMATRIX:
			if( pl ) u->SetValue( &pl->gl_shadowMatrix[0][0], MAX_SHADOWMAPS );
			break;
		case UT_SHADOWSPLITDIST:
			v = RI->view.parallelSplitDistances;
			u->SetValue( v[0], v[1], v[2], v[3] );
			break;
		case UT_TEXELSIZE:
			u->SetValue( 1.0f / (float)sunSize[0], 1.0f / (float)sunSize[1], 1.0f / (float)sunSize[2], 1.0f / (float)sunSize[3] );
			break;
		case UT_LIGHTGAMMA:
			u->SetValue(tr.light_gamma);
			break;
		case UT_LIGHTDIR:
			if( pl )
			{
				if( pl->type == LIGHT_DIRECTIONAL ) lightdir = -tr.sky_normal;
				else lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
				u->SetValue( lightdir.x, lightdir.y, lightdir.z, pl->fov );
			}
			break;
		case UT_LIGHTDIFFUSE:
			if( pl ) u->SetValue( pl->color.x, pl->color.y, pl->color.z );
			break;
		case UT_LIGHTORIGIN:
			if( pl ) u->SetValue( pl->origin.x, pl->origin.y, pl->origin.z, ( 1.0f / pl->radius )); 
			break;
		case UT_LIGHTVIEWPROJMATRIX:
			if( pl )
			{
				GLfloat gl_lightViewProjMatrix[16];
				pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );
				u->SetValue( &gl_lightViewProjMatrix[0] );
			}
			break;
		case UT_AMBIENTFACTOR:
			if( pl && pl->type == LIGHT_DIRECTIONAL )
				u->SetValue( tr.sun_ambient );
			else u->SetValue( tr.ambientFactor );
			break;
		case UT_SUNREFRACT:
			u->SetValue( tr.sun_refract );
			break;
		case UT_LIGHTNUMS0:
			u->SetValue( (float)grass->lights[0], (float)grass->lights[1], (float)grass->lights[2], (float)grass->lights[3] );
			break;
		case UT_LIGHTNUMS1:
			u->SetValue( (float)grass->lights[4], (float)grass->lights[5], (float)grass->lights[6], (float)grass->lights[7] );
			break;
		case UT_GRASSPARAMS:
			u->SetValue( m_flGrassFadeStart, m_flGrassFadeDist, m_flGrassFadeEnd );
			break;			
		default:
			ALERT( at_error, "%s: unhandled uniform %s\n", RI->currentshader->name, u->name );
			break;
		}
	}
}

/*
=================
R_SortGrassMeshes

sort by texture
=================
*/
static int R_SortGrassMeshes( struct grass_s *const *pa, struct grass_s *const *pb )
{
	grass_t *a = (grass_t *)*pa;
	grass_t *b = (grass_t *)*pb;

	if( a->texture > b->texture )
		return -1;
	if( a->texture < b->texture )
		return 1;

	return 0;
}

/*
================
R_RenderGrassOnList

rendering the grass
================
*/
void R_RenderGrassOnList( void )
{
	ZoneScoped;

	word	hCachedMatrix = -1;
	word	hLastShader = -1;
	word	hCurrentShader;
	byte	hLastTexture = -1;
	byte	cached_lights[MAXDYNLIGHTS] = { 255 };
	bool	update_params = false;
	int	parms_updated = 0;

	if( !RI->frame.grass_list.Count())
		return; // don't waste time

	GL_AlphaTest( GL_TRUE );

	GL_DEBUG_SCOPE();
	pglAlphaFunc( GL_GREATER, r_grass_alpha->value );
	GL_Cull( GL_NONE );	// grass is double-sided poly
	GL_DepthMask( GL_TRUE );
	GL_Blend( GL_FALSE );

	// sorting list to reduce shader switches
	if( !CVAR_TO_BOOL( cv_nosort ))
		RI->frame.grass_list.Sort( R_SortGrassMeshes );

	for( int i = 0; i < RI->frame.grass_list.Count(); i++ )
	{
		grass_t *g = RI->frame.grass_list[i];

		hCurrentShader = g->forwardScene.GetHandle();
		if( !hCurrentShader ) continue;

		if( hCachedMatrix != g->hCachedMatrix )
		{
			hCachedMatrix = g->hCachedMatrix;
			update_params = true;
		}

		GL_DepthRange( gldepthmin, gldepthmax );
		GL_ClipPlane( true );

		if( hLastShader != hCurrentShader )
		{
			hLastShader = hCurrentShader;
			update_params = true;
		}

		if( hLastTexture != g->texture )
		{
			hLastTexture = g->texture;
			update_params = true;
		}

		if( update_params )
		{
			R_SetGrassUniforms( hCurrentShader, g );
			update_params = false;
		}

		R_DrawGrassMeshFromBuffer( g );
	}

	// restore old binding
	GL_DepthRange( gldepthmin, gldepthmax );
	GL_ClipPlane( true );
	GL_Cull( GL_FRONT );
}

/*
================
R_DrawLightForGrass

draw lighting for grass
================
*/
void R_DrawLightForGrass( CDynLight *pl )
{
	word	hCachedMatrix = -1;
	word	hLastShader = -1;
	byte	hLastTexture = -1;
	bool	update_params = false;
	word	hLightShader;

	if( !RI->frame.light_grass.Count())
		return; // don't waste time

	GL_AlphaTest( GL_TRUE );
	pglAlphaFunc( GL_GREATER, r_grass_alpha->value );
	GL_Cull( GL_NONE );

	// sorting list to reduce shader switches
	if( !CVAR_TO_BOOL( cv_nosort ))
		RI->frame.light_grass.Sort( R_SortGrassMeshes );

	for( int i = 0; i < RI->frame.light_grass.Count(); i++ )
	{
		grass_t *g = RI->frame.light_grass[i];

		switch( pl->type )
		{
		case LIGHT_SPOT:
			hLightShader = g->forwardLightSpot.GetHandle();
			break;
		case LIGHT_OMNI:
			hLightShader = g->forwardLightOmni.GetHandle();
			break;
		case LIGHT_DIRECTIONAL:
			hLightShader = g->forwardLightProj.GetHandle();
			break;
		default:
			hLightShader = 0;
			break;
		}

		if( !hLightShader ) continue;

		if( hCachedMatrix != g->hCachedMatrix )
		{
			hCachedMatrix = g->hCachedMatrix;
			update_params = true;
		}

		if( hLastShader != hLightShader )
		{
			hLastShader = hLightShader;
			update_params = true;
		}

		if( hLastTexture != g->texture )
		{
			hLastTexture = g->texture;
			update_params = true;
		}

		if( update_params )
		{
			R_SetGrassUniforms( hLightShader, g );
			update_params = false;
		}

		R_DrawGrassMeshFromBuffer( g );
	}

	GL_Cull( GL_FRONT );
}

/*
================
R_RenderShadowGrassOnList

depth pass
================
*/
void R_RenderShadowGrassOnList( void )
{
	word	hCachedMatrix = -1;
	word	hLastShader = -1;
	byte	hLastTexture = -1;
	bool	update_params = false;

	if( !RI->frame.grass_list.Count())
		return; // don't waste time

	GL_DEBUG_SCOPE();
	GL_AlphaTest( GL_TRUE );
	pglAlphaFunc( GL_GREATER, r_grass_alpha->value );
	GL_Cull( GL_NONE );	// grass is double-sided poly

	// sorting list to reduce shader switches
	if( !CVAR_TO_BOOL( cv_nosort ))
		RI->frame.grass_list.Sort( R_SortGrassMeshes );

	for( int i = 0; i < RI->frame.grass_list.Count(); i++ )
	{
		grass_t *g = RI->frame.grass_list[i];

		if( hCachedMatrix != g->hCachedMatrix )
		{
			hCachedMatrix = g->hCachedMatrix;
			update_params = true;
		}

		if( hLastShader != g->forwardDepth.GetHandle() )
		{
			hLastShader = g->forwardDepth.GetHandle();
			update_params = true;
		}

		if( hLastTexture != g->texture )
		{
			hLastTexture = g->texture;
			update_params = true;
		}

		if( update_params )
		{
			R_SetGrassUniforms( g->forwardDepth.GetHandle(), g );
			update_params = false;
		}

		R_DrawGrassMeshFromBuffer( g );
	}

	// restore old binding
	GL_Cull( GL_FRONT );
}

/*
================
R_GrassTextureForName

find or add unique texture for grass
================
*/
int R_GrassTextureForName( const char *name, byte *tex )
{
	size_t i;

	for( i = 0; i < GRASS_TEXTURES && grasstexs[i].name[0]; i++ )
	{
		if( !Q_stricmp( grasstexs[i].name, name ))
			goto end;	// found
	}

	if( i == GRASS_TEXTURES )
	{
		ALERT( at_warning, "limit of grass textures was exceeded %d\n", GRASS_TEXTURES );
		return 1;
	}

	// allocate a new one
	Q_strncpy( grasstexs[i].name, name, sizeof( grasstexs[i].name ));
	grasstexs[i].gl_texturenum = LOAD_TEXTURE( name, NULL, 0, TF_GRASS );

	if( !grasstexs[i].gl_texturenum.Initialized() )
	{
		ALERT( at_warning, "couldn't load %s\n", name );
		grasstexs[i].gl_texturenum = tr.defaultTexture;
	}

end:
	*tex = (byte)i;
	return 0;
}

/*
================
R_GrassInitForSurface

lookup all the descriptions
because this system allow many
sorts of grass per one surface
================
*/
void R_GrassInitForSurface( msurface_t *surf )
{
	if( !surf || !surf->texinfo || !surf->texinfo->texture )
		return; // some bad polygons

	mextrasurf_t *es = surf->info;
	bvert_t *v0, *v1, *v2;

	for( int i = 0; i < grassInfo.Count(); i++ )
	{
		grassentry_t *entry = &grassInfo[i];

		if( !Mod_CheckLayerNameForSurf( surf, entry->name ))
			continue;

		if( !FBitSet( surf->flags, SURF_PLANEBACK ) && surf->plane->normal.z < 0.40f )
			continue; // vertical too much

		if( FBitSet( surf->flags, SURF_PLANEBACK ) && surf->plane->normal.z > -0.40f )
			continue; // vertical too much

		// turn the face into a bunch of polygons, and compute the area of each
		v0 = &world->vertexes[es->firstvertex];
		int count = 0;

		for( int j = 1; j < es->numverts - 1; j++ )
		{
			v1 = &world->vertexes[es->firstvertex+j];
			v2 = &world->vertexes[es->firstvertex+j+1];

			// compute two triangle edges
			Vector e1 = v1->vertex - v0->vertex;
			Vector e2 = v2->vertex - v0->vertex;

			// compute the triangle area
			Vector areaVec = CrossProduct( e1, e2 );
			float normalLength = areaVec.Length();
			float area = 0.5f * normalLength;

			// compute the number of samples to take
			count += area * entry->density * DENSITY_FACTOR;
		}

		if( count ) es->grasscount++; // mesh added 

		if( es->grasscount > 0 ) // at least one bush was detected
			SetBits( world->features, WORLD_HAS_GRASS );
	}
}

/*
================
R_GrassSetupFrame

clear the chains
================
*/
void R_GrassSetupFrame( void )
{
	m_flGrassFadeStart = r_grass_fade_start->value;

	if( m_flGrassFadeStart < GRASS_ANIM_DIST )
		m_flGrassFadeStart = GRASS_ANIM_DIST;
	m_flGrassFadeDist = Q_max( 0.0f, r_grass_fade_dist->value );
	m_flGrassFadeEnd = m_flGrassFadeStart + m_flGrassFadeDist;
}

/*
================
R_GrassShaderSceneForward

compute albedo with static lighting
================
*/
static word R_GrassShaderSceneForward( msurface_t *s, grass_t *g )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	mextrasurf_t *es = s->info;

	if( g->forwardScene.IsValid( ))
		return g->forwardScene.GetHandle(); // valid

	ASSERT( RI->currententity != NULL );

	Q_strncpy( glname, "forward/scene_grass", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	material_t *mat = R_TextureAnimation( s )->material;

	if( FBitSet( mat->flags, BRUSH_FULLBRIGHT ) || R_FullBright( ))
	{
		GL_AddShaderDirective( options, "LIGHTING_FULLBRIGHT" );
	}
	else
	{
		if( r_lightmap->value > 0.0f && r_lightmap->value <= 2.0f )
		{
			if( r_lightmap->value == 1.0f && worldmodel->lightdata )
				GL_AddShaderDirective( options, "LIGHTMAP_DEBUG" );
			else if( r_lightmap->value == 2.0f && FBitSet( world->features, WORLD_HAS_DELUXEMAP ))
				GL_AddShaderDirective( options, "LIGHTVEC_DEBUG" );
		}

		if( es->normals != NULL && r_grass->value > 1.0f )
			GL_AddShaderDirective( options, "HAS_DELUXEMAP" );

		// process lightstyles
		for( int i = 0; i < MAXLIGHTMAPS && s->styles[i] != LS_NONE; i++ )
			GL_AddShaderDirective( options, va( "APPLY_STYLE%i", i ));
	}

	GL_AddShaderDirective( options, "APPLY_FADE_DIST" );

	if (tr.fogEnabled && !RP_CUBEPASS())
		GL_AddShaderDirective( options, "APPLY_FOG_EXP" );

	word shaderNum = GL_FindUberShader( glname, options );
	if( !shaderNum )
	{
		SetBits( g->flags, FGRASS_NODRAW );
		return 0; // something bad happens
	}

	g->forwardScene.SetShader( shaderNum );
	ClearBits( g->flags, FGRASS_NODRAW );

	return shaderNum;
}

/*
================
R_GrassShaderLightForward

compute dynamic lighting
================
*/
static word R_GrassShaderLightForward( CDynLight *dl, grass_t *g )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];

	switch( dl->type )
	{
	case LIGHT_SPOT:
		if( g->forwardLightSpot.IsValid( ))
			return g->forwardLightSpot.GetHandle(); // valid
		break;
	case LIGHT_OMNI:
		if( g->forwardLightOmni.IsValid( ))
			return g->forwardLightOmni.GetHandle(); // valid
		break;
	case LIGHT_DIRECTIONAL:
		if( g->forwardLightProj.IsValid( ))
			return g->forwardLightProj.GetHandle(); // valid
		break;
	}

	Q_strncpy( glname, "forward/light_grass", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	switch( dl->type )
	{
	case LIGHT_SPOT:
		GL_AddShaderDirective( options, "LIGHT_SPOT" );
		break;
	case LIGHT_OMNI:
		GL_AddShaderDirective( options, "LIGHT_OMNI" );
		break;
	case LIGHT_DIRECTIONAL:
		GL_AddShaderDirective( options, "LIGHT_PROJ" );
		break;
	}

	GL_AddShaderDirective( options, "APPLY_FADE_DIST" );

	if( CVAR_TO_BOOL( r_shadows ) && !FBitSet( dl->flags, DLF_NOSHADOWS ))
	{
		GL_AddShaderDirective( options, "APPLY_SHADOW" );
	}

	// debug visualization
	if( r_lightmap->value > 0.0f && r_lightmap->value <= 2.0f )
	{
		if( r_lightmap->value == 1.0f && worldmodel->lightdata )
			GL_AddShaderDirective( options, "LIGHTMAP_DEBUG" );
		else if( r_lightmap->value == 2.0f && FBitSet( world->features, WORLD_HAS_DELUXEMAP ))
			GL_AddShaderDirective( options, "LIGHTVEC_DEBUG" );
	}

	word shaderNum = GL_FindUberShader( glname, options );

	if( !shaderNum )
	{
		if( dl->type == LIGHT_DIRECTIONAL )
			SetBits( g->flags, FGRASS_NOSUNLIGHT );
		else SetBits( g->flags, FGRASS_NODLIGHT );
		return 0; // something bad happens
	}

	// done
	switch( dl->type )
	{
	case LIGHT_SPOT:
		g->forwardLightSpot.SetShader( shaderNum );
		ClearBits( g->flags, FGRASS_NODLIGHT );
		break;
	case LIGHT_OMNI:
		g->forwardLightOmni.SetShader( shaderNum );
		ClearBits( g->flags, FGRASS_NODLIGHT );
		break;
	case LIGHT_DIRECTIONAL:
		g->forwardLightProj.SetShader( shaderNum );
		ClearBits( g->flags, FGRASS_NOSUNLIGHT );
		break;
	}

	return shaderNum;
}

/*
================
R_GrassShaderSceneDepth

return grass depth-shader
================
*/
static word R_GrassShaderSceneDepth( msurface_t *s, grass_t *g )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	mextrasurf_t *es = s->info;

	if( g->forwardDepth.IsValid( ))
		return g->forwardDepth.GetHandle(); // valid

	ASSERT( RI->currententity != NULL );

	Q_strncpy( glname, "forward/depth_grass", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	GL_AddShaderDirective( options, "APPLY_FADE_DIST" );

	word shaderNum = GL_FindUberShader( glname, options );
	if( !shaderNum ) return 0; // something bad happens

	g->forwardDepth.SetShader( shaderNum );

	return shaderNum;
}

/*
================
R_PrecacheGrass

precache grass in the world
================
*/
void R_PrecacheGrass( msurface_t *s, mextraleaf_t *leaf )
{
	mextrasurf_t *es = s->info;
	grasshdr_t *hdr = es->grass;
	cl_entity_t *e = GET_ENTITY( 0 );

	if( !es->grasscount ) return;	// no grass for this face

	if( hdr )
	{
		if( FBitSet( s->flags, SURF_GRASS_UPDATE ))
		{
			// rebuild mesh with new gamma
			R_RemoveGrassForSurface( es );
		}
		else
		{
			// already created?
			return;
		}
	}

	float curdist = VectorDistance( tr.cached_vieworigin, es->origin );

	// but grass that attached to sky entities will be ignoring distance
	if( curdist > m_flGrassFadeEnd )
		return; // too far

	// initialize grass for surface
	R_ConstructGrassForSurface( s, es );
	hdr = es->grass; // refresh the pointer

	if( hdr && leaf )
	{
		// prevent to expand leafs too much
		AddPointToBounds( hdr->mins, leaf->mins, leaf->maxs, LEAF_MAX_EXPAND );
		AddPointToBounds( hdr->maxs, leaf->mins, leaf->maxs, LEAF_MAX_EXPAND );
	}
}

/*
================
R_AddGrassToDrawList

initializes and assigns grass, linked with speficific surface, to draw list
TODO: function name isn't intuitive, maybe rename it?
================
*/
void R_AddGrassToDrawList( msurface_t *s, drawlist_t drawlist_type )
{
	if( !FBitSet( world->features, WORLD_HAS_GRASS ))
		return; // completely has no grass

	if( !CVAR_TO_BOOL( r_grass ) || FBitSet( RI->params, RP_NOGRASS ))
		return; // disabled by request

	if( drawlist_type == DRAWLIST_SHADOW && !CVAR_TO_BOOL( r_grass_shadows ))
		return;// don't cast shadows from grass

	if( drawlist_type == DRAWLIST_LIGHT && !CVAR_TO_BOOL( r_grass_lighting ))
		return;// don't cast shadows from grass

	mextrasurf_t *es = s->info;
	cl_entity_t *e = es->parent;
	Vector absmin, absmax;

	// do simple culling by distance
	if( es->grasscount <= 0 ) return; // surface doesn't contain grass
	float curdist = VectorDistance( es->origin, tr.cached_vieworigin );

	// but grass that attached to sky entities will be ignoring distance
	if( curdist > m_flGrassFadeEnd )
		return; // too far

	// rebuild mesh with new gamma
	if( es->grass && FBitSet( s->flags, SURF_GRASS_UPDATE ))
		R_RemoveGrassForSurface( es );

	if( !es->grass && RP_NORMALPASS( ))
		R_ConstructGrassForSurface( s, es );

	grasshdr_t *hdr = es->grass;
	if( !hdr ) return; // face completely missed grass or creation was failed

	gl_state_t *glm = GL_GetCache( e->hCachedMatrix );

	if( e->hCachedMatrix == WORLD_MATRIX )
	{
		// default case
		absmin = hdr->mins;
		absmax = hdr->maxs;
	}
	else
	{
		// moving entity
		TransformAABB( glm->transform, hdr->mins, hdr->maxs, absmin, absmax );
	}

	CFrustum *frustum = NULL;

	if( RI->currentlight != NULL )
		frustum = &RI->currentlight->frustum;
	else frustum = &RI->view.frustum;

	if( frustum && frustum->CullBoxFast( absmin, absmax ))
		return;

	// NOTE: at this point we have surface that passed visibility and frustum tests

	// each mesh should be added individually
	for( int i = 0; i < hdr->count; i++ )
	{
		grass_t *grass = &hdr->g[i];
		grass->hCachedMatrix = e->hCachedMatrix;

		switch( drawlist_type )
		{
		case DRAWLIST_SOLID:
			if( FBitSet( grass->flags, FGRASS_NODRAW ))
				continue;

			R_GrassShaderSceneForward( s, grass );
			RI->frame.grass_list.AddToTail( grass );
			break;
		case DRAWLIST_SHADOW:
			if( FBitSet( grass->flags, FGRASS_NODRAW ))
				continue;

			R_GrassShaderSceneDepth( s, grass );
			RI->frame.grass_list.AddToTail( grass );
			break;
		case DRAWLIST_LIGHT:
			if( RI->currentlight->type == LIGHT_DIRECTIONAL )
			{
				if( FBitSet( grass->flags, FGRASS_NOSUNLIGHT ))
					continue;
			}
			else
			{
				if( FBitSet( grass->flags, FGRASS_NODLIGHT ))
					continue;
			}
			R_GrassShaderLightForward( RI->currentlight, grass );
			RI->frame.light_grass.AddToTail( grass );
			break;
		default:	// invalid mode
			return;
		}
	}
}

/*
================
R_UnloadFarGrass

release far VBO's
================
*/
void R_UnloadFarGrass( void )
{
	if( !FBitSet( world->features, WORLD_HAS_GRASS ))
		return; // don't waste time

	if( ++tr.grassunloadframe < 300 )
		return; // run every three seconds

	// check surfaces
	for( int i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		mextrasurf_t *es = surf->info;

		if( !es->grasscount ) continue; // surface doesn't contain grass
		float curdist = VectorDistance( tr.cached_vieworigin, es->origin );

		if( curdist > m_flGrassFadeEnd )
		{
			if( curdist > ( m_flGrassFadeEnd * 2.0f ) && es->grass )
				R_RemoveGrassForSurface( es );
		}
	}

	tr.grassunloadframe = 0;
}

/*
================
R_GrassInit

parse grass definitions and load textures
================
*/
void R_GrassInit( void )
{
	static int random_seed = 1; // starts from 1

	char *afile = (char *)gEngfuncs.COM_LoadFile( "gfx/grass/grassinfo.txt", 5, NULL );
	if( !afile ) ALERT( at_error, "couldn't load grassinfo.txt\n" );

	// remove grass description from the pervious map
	grassInfo.Purge();

	memset( grasstexs, 0, sizeof( grasstexs ));
	
	if( afile )
	{
		grassentry_t entry;
		char *pfile = afile;
		int parse_line = 1;
		char token[1024];

		while(( pfile = COM_ParseFile( pfile, token )) != NULL )
		{
			Q_strncpy( entry.name, token, sizeof( entry.name ));

			pfile = COM_ParseLine( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "R_GrassInit: missed grass texture path at line %i\n", parse_line );
				parse_line++;
				continue;
			}

			if( R_GrassTextureForName( token, &entry.texture ))
				break;

			pfile = COM_ParseLine( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "R_GrassInit: missed grass density at line %i\n", parse_line );
				parse_line++;
				continue;
			}
			entry.density = Q_atof( token );
			entry.density = bound( 0.1f, entry.density, 512.0f );

			pfile = COM_ParseLine( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "R_GrassInit: missed grass min scale at line %i\n", parse_line );
				parse_line++;
				continue;
			}
			entry.min = Q_atof( token );
			entry.min = bound( 0.01f, entry.min, 100.0f );

			pfile = COM_ParseLine( pfile, token );
			if( !pfile )
			{
				ALERT( at_error, "R_GrassInit: missed grass max scale at line %i\n", parse_line );
				parse_line++;
				continue;
			}
			entry.max = Q_atof( token );
			entry.max = bound( entry.min, entry.max, 100.0f );
			if( entry.min > entry.max ) entry.min = entry.max;

			pfile = COM_ParseLine( pfile, token );
			if( pfile )
			{
				// seed is optional
				entry.seed = Q_atoi( token );
				entry.seed = Q_max( 1, entry.seed );
			}
			else entry.seed = random_seed++;

			grassInfo.AddToTail( entry );
			parse_line++;
		}

		gEngfuncs.COM_FreeFile( afile );
	}
}

/*
================
R_GrassShutdown

prepare grass system to shutdown
================
*/
void R_GrassShutdown( void )
{
	// release all grass textures
	for( int i = 0; i < GRASS_TEXTURES; i++ )
	{
		if( !grasstexs[i].gl_texturenum.Initialized() )
			continue;

		FREE_TEXTURE( grasstexs[i].gl_texturenum );
	}
}
