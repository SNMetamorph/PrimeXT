/*
r_garss.cpp - grass rendering
Copyright (C) 2013 SovietCoder

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
#include "r_local.h"
#include "mathlib.h"
#include "r_world.h"
#include "r_shader.h"

grass_t			*grass_surfaces[MAX_REF_STACK][GRASS_TEXTURES];
grass_t			*grass_lighting[GRASS_TEXTURES];
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
void R_GrassLightForVertex( msurface_t *fa, mextrasurf_t *es, const Vector &vertex, float posz, float light[MAXLIGHTMAPS] )
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
		light[map] = (float)((double)((lm->r << 16) | (lm->g << 8) | lm->b) / (double)(1 << 24));
		lm += size; // skip to next lightmap
	}
}

/*
================
R_CreateSingleBush

create a bush with specified pos
================
*/
bool R_CreateSingleBush( mextraleaf_t *leaf, msurface_t *surf, mextrasurf_t *es, grasshdr_t *hdr, const Vector &pos, float size )
{
	for( int i = 0; i < 16; i++ )
	{
		gvert_t *entry = &m_arraygvert[m_iNumVertex];
		Vector vertex = R_GetPointForBush( i, pos, size );
		memcpy( entry->styles, surf->styles, sizeof( entry->styles ));
		R_GrassLightForVertex( surf, es, vertex, pos.z, entry->light );
		if( leaf ) AddPointToBounds( vertex, leaf->mins, leaf->maxs ); // expand bbox with grass size
		AddPointToBounds( vertex, hdr->mins, hdr->maxs ); // build bbox for grass
		Vector dir = ( vertex - pos ).Normalize();
		float scale = ( vertex - pos ).Length();
#ifdef GRASS_PACKED_VERTEX
		entry->normal[0] = FloatToHalf( dir.x );
		entry->normal[1] = FloatToHalf( dir.y );
		entry->normal[2] = FloatToHalf( dir.z );
		entry->normal[3] = FloatToHalf( scale );
		entry->center[0] = FloatToHalf( pos.x );
		entry->center[1] = FloatToHalf( pos.y );
		entry->center[2] = FloatToHalf( pos.z );
		entry->center[3] = FloatToHalf( i );
#else
		entry->normal = Vector4D( dir.x, dir.y, dir.z, scale );
		entry->center = Vector4D( pos.x, pos.y, pos.z, i );
#endif
		// generate indices
		if( !R_GrassAdvanceVertex( ))
		{
			// vertexes is out
			return false;
		}
	}

	return true;
}

void R_CreateSurfaceVBO( grass_t *out )
{
	if( !m_iNumVertex ) return; // empty mesh?

	// create GPU static buffer
	pglGenBuffersARB( 1, &out->vbo );
	pglGenVertexArrays( 1, &out->vao );

	// create vertex array object
	pglBindVertexArray( out->vao );

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, out->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, m_iNumVertex * sizeof( gvert_t ), &m_arraygvert[0], GL_STATIC_DRAW_ARB );
#ifdef GRASS_PACKED_VERTEX
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( gvert_t ), (void *)offsetof( gvert_t, center ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 4, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( gvert_t ), (void *)offsetof( gvert_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
#else
	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_t ), (void *)offsetof( gvert_t, center ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_t ), (void *)offsetof( gvert_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );
#endif
	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( gvert_t ), (void *)offsetof( gvert_t, light )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( gvert_t ), (void *)offsetof( gvert_t, styles ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );

	// create index array buffer
	pglGenBuffersARB( 1, &out->ibo );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, out->ibo );
	pglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_iNumIndex * sizeof( unsigned short ), &m_indexarray[0], GL_STATIC_DRAW_ARB );

	// don't forget to unbind them
	pglBindVertexArray( GL_FALSE );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );

	world->grassmem += sizeof( gvert_t ) * m_iNumVertex;
	world->grasscount += m_iNumVertex;
}

void R_DeleteSurfaceVBO( grass_t *out )
{
	if( out->vao ) pglDeleteVertexArrays( 1, &out->vao );
	if( out->vbo ) pglDeleteBuffersARB( 1, &out->vbo );
	if( out->ibo ) pglDeleteBuffersARB( 1, &out->ibo );

	world->grassmem -= sizeof( gvert_t ) * out->numVerts;
	world->grasscount -= out->numVerts;
}

/*
================
R_BuildGrassMesh

build mesh with single texture
================
*/
bool R_BuildGrassMesh( mextraleaf_t *leaf, msurface_t *surf, mextrasurf_t *es, grassentry_t *entry, grasshdr_t *hdr, grass_t *out )
{
	mfaceinfo_t *land = surf->texinfo->faceinfo;
	bvert_t *v0, *v1, *v2;

	// update random set to get predictable positions for grass 'random' placement
	RANDOM_SEED(( surf - worldmodel->surfaces ) * entry->seed );

	m_iNumVertex = m_iNumIndex = m_iVertexState = 0;

	m_iTextureWidth = RENDER_GET_PARM( PARM_TEX_WIDTH, grasstexs[entry->texture].gl_texturenum );
	m_iTextureHeight = RENDER_GET_PARM( PARM_TEX_HEIGHT, grasstexs[entry->texture].gl_texturenum );

	// turn the face into a bunch of polygons, and compute the area of each
	v0 = &world->vertexes[es->firstvertex];

	for( int i = 1; i < es->numverts - 1; i++ )
	{
		v1 = &world->vertexes[es->firstvertex+i];
		v2 = &world->vertexes[es->firstvertex+i+1];

		// compute two triangle edges
		Vector e1 = v1->vertex - v0->vertex;
		Vector e2 = v2->vertex - v0->vertex;

		// compute the triangle area
		Vector areaVec = CrossProduct( e1, e2 );
		float normalLength = areaVec.Length();
		float area = 0.5f * normalLength;

		// compute the number of samples to take
		int numSamples = area * entry->density * 0.0001f;

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

			if( !R_CreateSingleBush( leaf, surf, es, hdr, pos, size ))
				goto build_mesh; // vertices is out (more than 2048 bushes per surface created)
		}
	}

	// nothing to added?
	if( !m_iNumVertex ) return false;

build_mesh:
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
void R_ConstructGrassForSurface( mextraleaf_t *leaf, msurface_t *surf, mextrasurf_t *es )
{
	// already init or not specified?
	if( es->grass || !es->grasscount )
		return;

	pglBindVertexArray( GL_FALSE );

	size_t grasshdr_size = sizeof( grasshdr_t ) + sizeof( grass_t ) * ( es->grasscount - 1 );
	grasshdr_t *hdr = es->grass = (grasshdr_t *)IEngineStudio.Mem_Calloc( 1, grasshdr_size );
	hdr->mins = es->mins;
	hdr->maxs = es->maxs;
	hdr->dist = -1.0f;
	hdr->count = 0;

	for( int i = 0; i < grassInfo.Count(); i++ )
	{
		grassentry_t *entry = &grassInfo[i];

		if( !Mod_CheckLayerNameForSurf( surf, entry->name ))
			continue;

		// create a single mesh for all the bushes that have same texture
		if( !R_BuildGrassMesh( leaf, surf, es, entry, hdr, &hdr->g[hdr->count] ))
			continue;	// failed to build for some reasons

		hdr->count++;
	}

	// for rotating bmodels
	hdr->radius = RadiusFromBounds( hdr->mins, hdr->maxs );

	// bah! failed to create
	if( !hdr->count )
	{
		Mem_Free( es->grass );
		es->grasscount = 0;
		es->grass = NULL;
	}

	pglBindVertexArray( world->vertex_array_object ); // restore old binding

	// restore normal randomization
	RANDOM_SEED( 0 );
}

void R_RemoveGrassForSurface( mextrasurf_t *es )
{
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
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, mesh->ibo );

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_TRIANGLES, 0, mesh->numVerts - 1, mesh->numElems, GL_UNSIGNED_SHORT, 0 );
	else pglDrawElements( GL_TRIANGLES, mesh->numElems, GL_UNSIGNED_SHORT, 0 );

	r_stats.c_total_tris += (mesh->numVerts / 2);
	r_stats.c_grass_polys += (mesh->numVerts / 4);
	r_stats.num_flushes++;
}

/*
================
R_DrawGrassMesh

render grass with diffuse or skyambient lighting
================
*/
void R_DrawGrassMesh( grass_t *grass, int tex, word &hLastShader, word &hCachedMatrix )
{
	if( hLastShader != grass->shaderNum )
	{
		GL_BindShader( &glsl_programs[grass->shaderNum] );

		ASSERT( RI.currentshader != NULL );

		pglUniform1fvARB( RI.currentshader->u_LightStyleValues, MAX_LIGHTSTYLES, &tr.lightstyles[0] );
		pglUniform3fARB( RI.currentshader->u_ViewOrigin, tr.cached_vieworigin.x, tr.cached_vieworigin.y, tr.cached_vieworigin.z );
		pglUniform4fARB( RI.currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
		pglUniform4fvARB( RI.currentshader->u_GammaTable, 64, &tr.gamma_table[0][0] );
		pglUniform1fARB( RI.currentshader->u_GrassFadeStart, m_flGrassFadeStart );
		pglUniform1fARB( RI.currentshader->u_GrassFadeDist, m_flGrassFadeDist );
		pglUniform1fARB( RI.currentshader->u_GrassFadeEnd, m_flGrassFadeEnd );
		pglUniform1fARB( RI.currentshader->u_RealTime, tr.time );
		hLastShader = grass->shaderNum;
		hCachedMatrix = -1;
	}

	if( CVAR_TO_BOOL( r_lightmap ) && !CVAR_TO_BOOL( r_fullbright ))
		GL_Bind( GL_TEXTURE0, tr.whiteTexture );
	else GL_Bind( GL_TEXTURE0, grasstexs[tex].gl_texturenum );

	if( grass->hCachedMatrix != hCachedMatrix )
	{
		gl_state_t *glm = &tr.cached_state[grass->hCachedMatrix];
		pglUniformMatrix4fvARB( RI.currentshader->u_ModelMatrix, 1, GL_FALSE, &glm->modelMatrix[0] );
		hCachedMatrix = grass->hCachedMatrix;
	}

	// render grass meshes
	R_DrawGrassMeshFromBuffer( grass );
}

/*
================
R_DrawGrassShadowMesh

render grass shadowpass
================
*/
void R_DrawGrassShadowMesh( grass_t *grass, int tex, word &hCachedMatrix )
{
	GL_Bind( GL_TEXTURE0, grasstexs[tex].gl_texturenum );

	if( grass->hCachedMatrix != hCachedMatrix )
	{
		gl_state_t *glm = &tr.cached_state[grass->hCachedMatrix];
		pglUniformMatrix4fvARB( RI.currentshader->u_ModelMatrix, 1, GL_FALSE, &glm->modelMatrix[0] );
		hCachedMatrix = grass->hCachedMatrix;
	}

	// render grass meshes
	R_DrawGrassMeshFromBuffer( grass );
}

/*
================
R_RenderGrassOnList

construct and rendering the grass
================
*/
void R_RenderGrassOnList( void )
{
	int	pass = glState.stack_position;
	word	hCachedMatrix = -1;
	word	hLastShader = -1;

	if( !FBitSet( world->features, WORLD_HAS_GRASS ))
		return; // don't waste time

	GL_AlphaTest( GL_TRUE );
	pglAlphaFunc( GL_GREATER, r_grass_alpha->value );
	GL_Cull( GL_NONE );	// grass is double-sided poly

	for( int i = 0; i < GRASS_TEXTURES; i++ )
	{
		for( grass_t *g = grass_surfaces[pass][i]; g != NULL; g = g->chain[pass] )
			R_DrawGrassMesh( g, i, hLastShader, hCachedMatrix );
	}

	pglBindVertexArray( world->vertex_array_object ); // restore old binding
	GL_Cull( GL_FRONT );
}

/*
================
R_DrawLightForGrassMesh

render grass with diffuse or skyambient lighting
================
*/
void R_DrawLightForGrassMesh( plight_t *pl, grass_t *grass, int tex, word &hLastShader, word &hCachedMatrix )
{
	GLfloat gl_lightViewProjMatrix[16];

	if( hLastShader != grass->hLightShader )
	{
		Vector lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
		pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );

		GL_BindShader( &glsl_programs[grass->hLightShader] );

		ASSERT( RI.currentshader != NULL );

		// write constants
		pglUniformMatrix4fvARB( RI.currentshader->u_LightViewProjectionMatrix, 1, GL_FALSE, &gl_lightViewProjMatrix[0] );
		float shadowWidth = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_WIDTH, pl->shadowTexture );
		float shadowHeight = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_HEIGHT, pl->shadowTexture );

		// depth scale and bias and shadowmap resolution
		pglUniform4fARB( RI.currentshader->u_LightDir, lightdir.x, lightdir.y, lightdir.z, pl->fov );
		pglUniform4fARB( RI.currentshader->u_LightDiffuse, pl->color.r / 255.0f, pl->color.g / 255.0f, pl->color.b / 255.0f, pl->lightFalloff );
		pglUniform4fARB( RI.currentshader->u_ShadowParams, shadowWidth, shadowHeight, -pl->projectionMatrix[2][2], pl->projectionMatrix[3][2] );
		pglUniform4fARB( RI.currentshader->u_LightOrigin, pl->origin.x, pl->origin.y, pl->origin.z, ( 1.0f / pl->radius ));
		pglUniform4fARB( RI.currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );

		GL_Bind( GL_TEXTURE1, pl->projectionTexture );
		GL_Bind( GL_TEXTURE2, pl->shadowTexture );

		pglUniform3fARB( RI.currentshader->u_ViewOrigin, tr.cached_vieworigin.x, tr.cached_vieworigin.y, tr.cached_vieworigin.z );
		pglUniform1fARB( RI.currentshader->u_GrassFadeStart, m_flGrassFadeStart );
		pglUniform1fARB( RI.currentshader->u_GrassFadeDist, m_flGrassFadeDist );
		pglUniform1fARB( RI.currentshader->u_GrassFadeEnd, m_flGrassFadeEnd );
		pglUniform1fARB( RI.currentshader->u_RealTime, tr.time );

		hLastShader = grass->hLightShader;
		hCachedMatrix = -1;
	}

	GL_Bind( GL_TEXTURE0, grasstexs[tex].gl_texturenum );

	if( grass->hCachedMatrix != hCachedMatrix )
	{
		gl_state_t *glm = &tr.cached_state[grass->hCachedMatrix];

		pglUniformMatrix4fvARB( RI.currentshader->u_ModelMatrix, 1, GL_FALSE, &glm->modelMatrix[0] );
		hCachedMatrix = grass->hCachedMatrix;
	}

	// render light for grass meshes
	R_DrawGrassMeshFromBuffer( grass );
}

/*
================
R_DrawLightForGrass

draw lighting for grass
================
*/
void R_DrawLightForGrass( plight_t *pl )
{
	word hCachedMatrix = -1;
	word hLastShader = -1;

	if( !tr.num_light_grass ) return; // don't waste time

	GL_AlphaTest( GL_TRUE );
	pglAlphaFunc( GL_GREATER, r_grass_alpha->value );
	GL_Cull( GL_NONE );

	for( int i = 0; i < GRASS_TEXTURES; i++ )
	{
		for( grass_t *g = grass_lighting[i]; g != NULL; g = g->lightchain )
			R_DrawLightForGrassMesh( pl, g, i, hLastShader, hCachedMatrix );
	}

	pglBindVertexArray( world->vertex_array_object ); // restore old binding
	GL_Cull( GL_FRONT );
}

/*
================
R_RenderGrassOnList

construct and rendering the grass
================
*/
void R_RenderShadowGrassOnList( void )
{
	int	pass = glState.stack_position;
	word	hCachedMatrix = -1;
	word	hLastShader = -1;

	if( !FBitSet( world->features, WORLD_HAS_GRASS ))
		return; // don't waste time

	GL_AlphaTest( GL_TRUE );
	pglAlphaFunc( GL_GREATER, r_grass_alpha->value );
	GL_Cull( GL_NONE );	// grass is double-sided poly
	GL_BindShader( glsl.grassDepthFill );

	pglUniform3fARB( RI.currentshader->u_ViewOrigin, tr.cached_vieworigin.x, tr.cached_vieworigin.y, tr.cached_vieworigin.z );
	pglUniform1fARB( RI.currentshader->u_GrassFadeStart, m_flGrassFadeStart );
	pglUniform1fARB( RI.currentshader->u_GrassFadeDist, m_flGrassFadeDist );
	pglUniform1fARB( RI.currentshader->u_GrassFadeEnd, m_flGrassFadeEnd );
	pglUniform1fARB( RI.currentshader->u_RealTime, tr.time );

	for( int i = 0; i < GRASS_TEXTURES; i++ )
	{
		for( grass_t *g = grass_surfaces[pass][i]; g != NULL; g = g->chain[pass] )
			R_DrawGrassShadowMesh( g, i, hCachedMatrix );
	}

	GL_Cull( GL_FRONT );
}

/*
================
R_GrassTextureForName

find or add unique texture for grass
================
*/
byte R_GrassTextureForName( const char *name )
{
	for( byte i = 0; i < GRASS_TEXTURES && grasstexs[i].name[0]; i++ )
	{
		if( !Q_stricmp( grasstexs[i].name, name ))
			return i;	// found
	}

	// allocate a new one
	Q_strncpy( grasstexs[i].name, name, sizeof( grasstexs[i].name ));
	grasstexs[i].gl_texturenum = LOAD_TEXTURE( name, NULL, 0, TF_CLAMP );

	if( !grasstexs[i].gl_texturenum )
	{
		ALERT( at_warning, "couldn't load %s\n", name );
		grasstexs[i].gl_texturenum = tr.defaultTexture;
	}

	return i;
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
			count += area * entry->density * 0.0001f;
		}

		if( count ) es->grasscount++; // mesh added 

		if( es->grasscount > 0 ) // at least one bush was detected
			SetBits( world->features, WORLD_HAS_GRASS );
	}
}

/*
================
R_GrassPrepareFrame

clear the chains
================
*/
void R_GrassPrepareFrame( void )
{
	int	pass = glState.stack_position;

	for( int i = 0; i < GRASS_TEXTURES; i++ )
		grass_surfaces[pass][i] = NULL;

	m_flGrassFadeStart = r_grass_fade_start->value;

	if( m_flGrassFadeStart < GRASS_ANIM_DIST )
		m_flGrassFadeStart = GRASS_ANIM_DIST;
	m_flGrassFadeDist = Q_max( 0.0f, r_grass_fade_dist->value );
	m_flGrassFadeEnd = m_flGrassFadeStart + m_flGrassFadeDist;
}

/*
================
R_GrassPrepareFrame

clear the chains
================
*/
void R_GrassPrepareLight( void )
{
	memset( grass_lighting, 0, sizeof( grass_lighting ));
	tr.num_light_grass = 0;
}

/*
===============
R_ChooseGrassProgram

Select the program for grass (diffuse\light\skybox)
===============
*/
word R_ChooseGrassProgram( msurface_t *s, grass_t *g, bool lightpass )
{
	if( FBitSet( RI.params, RP_SHADOWVIEW )) // shadow pass
		return (glsl.grassDepthFill - glsl_programs);

	if( lightpass )
		return GL_UberShaderForGrassDlight( RI.currentlight, g );
	return GL_UberShaderForGrassSolid( s, g );
}

/*
================
R_AddGrassToChain

build the grasschains
================
*/
bool R_AddGrassToChain( mextraleaf_t *leaf, msurface_t *s, cl_entity_t *e, CFrustum *frustum, int clipFlags, bool lightpass )
{
	if( !FBitSet( world->features, WORLD_HAS_GRASS ) || !CVAR_TO_BOOL( r_grass ) || FBitSet( RI.params, RP_NOGRASS ))
		return false; // don't waste time

	if( FBitSet( RI.params, RP_SHADOWVIEW ) && !CVAR_TO_BOOL( r_grass_shadows ))
		return false;

	if( lightpass && !CVAR_TO_BOOL( r_grass_lighting ))
		return false;

	mextrasurf_t *es = s->info;
	if( es->grasscount <= 0 ) return false; // surface doesn't contain grass
	float curdist = ( tr.cached_vieworigin - es->origin ).Length();

	if( curdist > m_flGrassFadeEnd )
		return false; // too far

	if( !es->grass ) R_ConstructGrassForSurface( leaf, s, es ); // first time?
	grasshdr_t *hdr = es->grass;
	if( !hdr ) return false; // ???

	int pass = glState.stack_position;
	Vector mins, maxs;
	word hShaderNum;

	if( e->angles != g_vecZero )
	{
		mins = e->origin - hdr->radius;
		maxs = e->origin + hdr->radius;
	}
	else
	{
		mins = e->origin + hdr->mins;
		maxs = e->origin + hdr->maxs;
	}

	if( frustum && frustum->CullBox( mins, maxs, clipFlags ))
		return false; // invisible

	hdr->dist = curdist;

	// link grass into supposed chain
	for( int i = 0; i < hdr->count; i++ )
	{
		grass_t *grass = &hdr->g[i];

		if( lightpass && FBitSet( grass->flags, FGRASS_NODLIGHT ))
			continue;
		else if( FBitSet( grass->flags, FGRASS_NODRAW ))
			continue;

		if( !( hShaderNum = R_ChooseGrassProgram( s, grass, lightpass )))
			continue;	// failed to build shader for this grass mesh

		grass->hCachedMatrix = e->hCachedMatrix;

		if( lightpass )
		{
			grass->hLightShader = hShaderNum;
			grass->lightchain = grass_lighting[grass->texture];
			grass_lighting[grass->texture] = grass;
			tr.num_light_grass++;
		}
		else
		{
			if( !FBitSet( RI.params, RP_SHADOWVIEW ))
				grass->shaderNum = hShaderNum;
			grass->chain[pass] = grass_surfaces[pass][grass->texture];
			grass_surfaces[pass][grass->texture] = grass;
		}
	}

	return true;
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

	if( ++tr.grassunloadframe < 100 )
		return; // thinking ~one tick per second

	// check surfaces
	for( int i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		mextrasurf_t *es = surf->info;

		if( !es->grasscount ) continue; // surface doesn't contain grass
		float curdist = ( tr.cached_vieworigin - es->origin ).Length();

		if( curdist > m_flGrassFadeEnd )
		{
			if( curdist > ( m_flGrassFadeEnd * 1.2f ) && es->grass )
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

			entry.texture = R_GrassTextureForName( token );

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
				entry.seed = max( 1, entry.seed );
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
		if( !grasstexs[i].gl_texturenum )
			continue;

		FREE_TEXTURE( grasstexs[i].gl_texturenum );
	}
}
