/*
gl_lightmap.cpp - generate lightmaps and uploading them
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

#include "hud.h"
#include "utils.h"
#include "const.h"
#include "studio.h"
#include "com_model.h"
#include "ref_params.h"
#include "gl_studio_userdata.h"
#include "gl_local.h"
#include <stringlib.h>
#include "gl_shader.h"
#include "gl_world.h"

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/
static int LM_AllocBlock( unsigned short w, unsigned short h, unsigned short *x, unsigned short *y )
{
	gl_lightmap_t *lms = &tr.lightmaps[tr.current_lightmap_texture];
	int	j, best, best2;

	best = GL_BLOCK_SIZE;
	for (int i = 0; i < GL_BLOCK_SIZE - w; i++)
	{
		best2 = 0;
		for (j = 0; j < w; j++)
		{
			if (lms->allocated[i + j] >= best)
				break;
			if (lms->allocated[i + j] > best2)
				best2 = lms->allocated[i + j];
		}

		if (j == w)
		{
			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > GL_BLOCK_SIZE)
	{
		// current lightmap is full
		lms->state = LM_DONE;
		return false;
	}

	for (int i = 0; i < w; i++) {
		lms->allocated[*x + i] = best + h;
	}

	lms->state = LM_USED; // lightmap in use
	return true;
}

static void LM_InitBlock( void )
{
	word	dummy;
	gl_lightmap_t *lms = &tr.lightmaps[tr.current_lightmap_texture];
	memset( lms->allocated, 0, sizeof( lms->allocated ));

	// first block at pos 0,0 used as black lightmap for studiomodel
	LM_AllocBlock( 1, 1, &dummy, &dummy );
}

static void LM_UploadPages( bool lightmap, bool deluxmap )
{
	char	lmName[16];
	byte	lightBuf[4];
	byte	deluxBuf[4];
	int	i;

	lightBuf[0] = 0;
	lightBuf[1] = 0;
	lightBuf[2] = 0;
	lightBuf[3] = 0;
	deluxBuf[0] = 127;
	deluxBuf[1] = 127;
	deluxBuf[2] = 255;
	deluxBuf[3] = 0;

	for( i = 0; i < MAX_LIGHTMAPS && tr.lightmaps[i].state != LM_FREE; i++ )
	{
		gl_lightmap_t	*lms = &tr.lightmaps[i];

		if( lightmap && !lms->lightmap.Initialized() )
		{
			Q_snprintf( lmName, sizeof( lmName ), "*diffuse%i", i );
			lms->lightmap = CREATE_TEXTURE( lmName, GL_BLOCK_SIZE, GL_BLOCK_SIZE, NULL, TF_LIGHTMAP ); 

			// also loading dummy blackpixel
			GL_Bind( GL_TEXTURE0, lms->lightmap );
			pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, lightBuf );
		}

		if( deluxmap && !lms->deluxmap.Initialized() )
		{
			Q_snprintf( lmName, sizeof( lmName ), "*normals%i", i );
			lms->deluxmap = CREATE_TEXTURE( lmName, GL_BLOCK_SIZE, GL_BLOCK_SIZE, NULL, TF_DELUXMAP );

			// also loading dummy blackpixel
			GL_Bind( GL_TEXTURE0, lms->deluxmap );
			pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, deluxBuf );
		}
	}
}

static void LM_GoToNextPage( void )
{
	gl_lightmap_t *lms = &tr.lightmaps[tr.current_lightmap_texture];

	if( lms->state != LM_DONE ) return; // current atlas not completed

	if( ++tr.current_lightmap_texture == MAX_LIGHTMAPS )
		HOST_ERROR( "MAX_LIGHTMAPS limit exceded\n" );
}

/*
==================
GL_BeginBuildingLightmaps

==================
*/
void GL_BeginBuildingLightmaps( void )
{
	int	i;

	// release old lightmaps first
	for( i = 0; i < MAX_LIGHTMAPS && tr.lightmaps[i].state != LM_FREE; i++ )
	{
		FREE_TEXTURE( tr.lightmaps[i].lightmap );
		FREE_TEXTURE( tr.lightmaps[i].deluxmap );
	}

	memset( tr.lightmaps, 0, sizeof( tr.lightmaps ));
	tr.current_lightmap_texture = 0;
	LM_InitBlock();
}

/*
=================
Mod_AllocLightmapForFace

NOTE: we don't loading lightmap here.
just create lmcoords and set lmnum
=================
*/
void GL_AllocLightmapForFace( msurface_t *surf )
{
	mextrasurf_t	*esrf = surf->info;
	word		smax, tmax;
	int		map;

	// always reject the tiled faces
	if( FBitSet( surf->flags, SURF_DRAWSKY ))
		return;

	// no lightdata and no deluxdata
	if( !surf->samples && !esrf->normals )
		return;

	int sample_size = Mod_SampleSizeForFace( surf );
	smax = ( surf->info->lightextents[0] / sample_size ) + 1;
	tmax = ( surf->info->lightextents[1] / sample_size ) + 1;
new_page:
	// alloc blocks for all the styles on current page
	for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != LS_NONE; map++ )
	{
		if( !LM_AllocBlock( smax, tmax, &esrf->light_s[map], &esrf->light_t[map] ))
		{
			// current page is not enough room for next 1-4 blocks
			LM_GoToNextPage();
			LM_InitBlock();
			goto new_page;
		}
	}

	// lightmap will be uploaded as far as player can see it
	esrf->lightmaptexturenum = tr.current_lightmap_texture;
	SetBits( surf->flags, SURF_LM_UPDATE|SURF_DM_UPDATE );
}

/*
=================
Mod_AllocLightmapForFace

NOTE: we don't loading lightmap here.
just create lmcoords and set lmnum
=================
*/
bool GL_AllocLightmapForFace( mstudiosurface_t *surf )
{
	word	smax, tmax;
	int	map;

	smax = surf->lightextents[0] + 1;
	tmax = surf->lightextents[1] + 1;

	// alloc blocks for all the styles on current page
	for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != LS_NONE; map++ )
	{
		if( !LM_AllocBlock( smax, tmax, &surf->light_s[map], &surf->light_t[map] ))
		{
			// current page is not enough room for next 1-4 blocks
			LM_GoToNextPage();
			LM_InitBlock();
			return false;
		}
	}

	// lightmap will be uploaded as far as player can see it
	surf->lightmaptexturenum = tr.current_lightmap_texture;
	SetBits( surf->flags, SURF_LM_UPDATE|SURF_DM_UPDATE );
	return true;
}

/*
=======================
GL_EndBuildingLightmaps
=======================
*/
void GL_EndBuildingLightmaps( bool lightmap, bool deluxmap )
{
	LM_UploadPages( lightmap, deluxmap );
}

/*
=================
R_BuildLightMapForStyle

write lightmap into page for a given style
=================
*/
static void R_BuildLightMapForStyle( msurface_t *surf, byte *dest, int style )
{
	mextrasurf_t	*esrf = surf->info;
	int		stride, size;
	int		smax, tmax;
	int		s, t;
	color24		*lm;
	byte		*sm;

	ASSERT( style >= 0 && style < MAXLIGHTMAPS );

	// always reject the sky faces
	if( FBitSet( surf->flags, SURF_DRAWSKY ))
		return;

	// no lightdata or style missed
	if( !surf->samples || surf->styles[style] == LS_NONE )
		return;

	int sample_size = Mod_SampleSizeForFace( surf );
	smax = ( surf->info->lightextents[0] / sample_size ) + 1;
	tmax = ( surf->info->lightextents[1] / sample_size ) + 1;
	size = smax * tmax;

	// jump to specified style
	lm = surf->samples + size * style;
	sm = esrf->shadows + size * style;

	// put into texture format
	stride = (smax * 4) - (smax << 2);

	for( t = 0; t < tmax; t++, dest += stride )
	{
		for( s = 0; s < smax; s++ )
		{
			dest[0] = lm->r;
			dest[1] = lm->g;
			dest[2] = lm->b;

			if( esrf->shadows != NULL )
				dest[3] = *sm++;
			else dest[3] = 255;

			dest += 4;
			lm++;
		}
	}
}

/*
=================
R_BuildLightMapForStyle

write lightmap into page for a given style
=================
*/
static void R_BuildLightMapForStyle( mstudiosurface_t *surf, byte *dest, int style )
{
	int	stride, size;
	int	smax, tmax;
	int	s, t;
	color24	*lm;
	byte	*sm;

	ASSERT( style >= 0 && style < MAXLIGHTMAPS );

	// no lightdata or style missed
	if( !surf->samples || surf->styles[style] == LS_NONE )
		return;

	smax = surf->lightextents[0] + 1;
	tmax = surf->lightextents[1] + 1;
	size = smax * tmax;

	// jump to specified style
	lm = surf->samples + size * style;
	sm = surf->shadows + size * style;

	// put into texture format
	stride = (smax * 4) - (smax << 2);

	for( t = 0; t < tmax; t++, dest += stride )
	{
		for( s = 0; s < smax; s++ )
		{
			dest[0] = lm->r;
			dest[1] = lm->g;
			dest[2] = lm->b;

			if( surf->shadows != NULL )
				dest[3] = *sm++;
			else dest[3] = 255;

			dest += 4;
			lm++;
		}
	}
}

/*
=================
R_BuildDeluxMapForStyle

write deluxmap into page for a given style
=================
*/
static void R_BuildDeluxMapForStyle( msurface_t *surf, byte *dest, int style )
{
	mextrasurf_t	*esrf = surf->info;
	int		stride, size;
	int		smax, tmax;
	int		s, t;
	color24		*dm;

	ASSERT( style >= 0 && style < MAXLIGHTMAPS );

	// always reject the sky faces
	if( FBitSet( surf->flags, SURF_DRAWSKY ))
		return;

	// no lightdata or style missed
	if( !esrf->normals || surf->styles[style] == LS_NONE )
		return;

	int sample_size = Mod_SampleSizeForFace( surf );
	smax = ( surf->info->lightextents[0] / sample_size ) + 1;
	tmax = ( surf->info->lightextents[1] / sample_size ) + 1;
	size = smax * tmax;

	// jump to specified style
	dm = esrf->normals + size * style;

	// put into texture format
	stride = (smax * 4) - (smax << 2);

	for( t = 0; t < tmax; t++, dest += stride )
	{
		for( s = 0; s < smax; s++ )
		{
			dest[0] = dm->r;
			dest[1] = dm->g;
			dest[2] = dm->b;
			dest[3] = 255;

			dest += 4;
			dm++;
		}
	}
}

/*
=================
R_BuildDeluxMapForStyle

write deluxmap into page for a given style
=================
*/
static void R_BuildDeluxMapForStyle( mstudiosurface_t *surf, byte *dest, int style )
{
	int	stride, size;
	int	smax, tmax;
	int	s, t;
	color24	*dm;

	ASSERT( style >= 0 && style < MAXLIGHTMAPS );

	// no lightdata or style missed
	if( !surf->normals || surf->styles[style] == LS_NONE )
		return;

	smax = surf->lightextents[0] + 1;
	tmax = surf->lightextents[1] + 1;
	size = smax * tmax;

	// jump to specified style
	dm = surf->normals + size * style;

	// put into texture format
	stride = (smax * 4) - (smax << 2);

	for( t = 0; t < tmax; t++, dest += stride )
	{
		for( s = 0; s < smax; s++ )
		{
			dest[0] = dm->r;
			dest[1] = dm->g;
			dest[2] = dm->b;
			dest[3] = 255;

			dest += 4;
			dm++;
		}
	}
}

/*
=================
R_UpdateLightMap

Combine and scale multiple lightmaps into the floating
format in r_blocklights
=================
*/
static void R_UpdateLightMap( msurface_t *surf )
{
	mextrasurf_t	*esrf = surf->info;
	static byte	buf[132*132*4];
	int		map, smax, tmax;

	// always reject the sky faces
	if( FBitSet( surf->flags, SURF_DRAWSKY ))
		return;

	int sample_size = Mod_SampleSizeForFace( surf );
	smax = ( surf->info->lightextents[0] / sample_size ) + 1;
	tmax = ( surf->info->lightextents[1] / sample_size ) + 1;

	// upload the lightmap
	if( surf->samples != NULL && FBitSet( surf->flags, SURF_LM_UPDATE ))
	{
		GL_Bind( GL_TEXTURE0, tr.lightmaps[esrf->lightmaptexturenum].lightmap );

		// write lightmaps into page
		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != LS_NONE; map++ )
		{
			R_BuildLightMapForStyle( surf, buf, map );
			pglTexSubImage2D( GL_TEXTURE_2D, 0, esrf->light_s[map], esrf->light_t[map], smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, buf );
		}
	}

	ClearBits( surf->flags, SURF_LM_UPDATE );

	// upload the deluxemap
	if( esrf->normals != NULL && FBitSet( surf->flags, SURF_DM_UPDATE ))
	{
		GL_Bind( GL_TEXTURE0, tr.lightmaps[esrf->lightmaptexturenum].deluxmap );

		// write lightmaps into page
		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != LS_NONE; map++ )
		{
			R_BuildDeluxMapForStyle( surf, buf, map );
			pglTexSubImage2D( GL_TEXTURE_2D, 0, esrf->light_s[map], esrf->light_t[map], smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, buf );
		}
	}

	ClearBits( surf->flags, SURF_DM_UPDATE );
}

/*
=================
R_UpdateLightMap

Combine and scale multiple lightmaps into the floating
format in r_blocklights
=================
*/
static void R_UpdateLightMap( mstudiosurface_t *surf )
{
	static byte	buf[512*512*4];
	int		map, smax, tmax;

	smax = surf->lightextents[0] + 1;
	tmax = surf->lightextents[1] + 1;

	// upload the lightmap
	if( surf->samples != NULL && FBitSet( surf->flags, SURF_LM_UPDATE ))
	{
		GL_Bind( GL_TEXTURE0, tr.lightmaps[surf->lightmaptexturenum].lightmap );

		// write lightmaps into page
		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != LS_NONE; map++ )
		{
			R_BuildLightMapForStyle( surf, buf, map );
			pglTexSubImage2D( GL_TEXTURE_2D, 0, surf->light_s[map], surf->light_t[map], smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, buf );
		}
	}

	ClearBits( surf->flags, SURF_LM_UPDATE );

	// upload the deluxemap
	if( surf->normals != NULL && FBitSet( surf->flags, SURF_DM_UPDATE ))
	{
		GL_Bind( GL_TEXTURE0, tr.lightmaps[surf->lightmaptexturenum].deluxmap );

		// write lightmaps into page
		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != LS_NONE; map++ )
		{
			R_BuildDeluxMapForStyle( surf, buf, map );
			pglTexSubImage2D( GL_TEXTURE_2D, 0, surf->light_s[map], surf->light_t[map], smax, tmax, GL_RGBA, GL_UNSIGNED_BYTE, buf );
		}
	}

	ClearBits( surf->flags, SURF_DM_UPDATE );
}

/*
========================
R_TextureCoords

fill vec2_t with texture coords
========================
*/
void R_TextureCoords( msurface_t *surf, const Vector &vec, float *out )
{
	float	s, t;

	s = DotProduct( vec, surf->texinfo->vecs[0] ) + surf->texinfo->vecs[0][3];
	s /= surf->texinfo->texture->width;

	t = DotProduct( vec, surf->texinfo->vecs[1] ) + surf->texinfo->vecs[1][3];
	t /= surf->texinfo->texture->height;

	out[0] = s;
	out[1] = t;
}

/*
========================
R_GlobalCoords

fill vec2_t with global coords
========================
*/
void R_GlobalCoords( msurface_t *surf, const Vector &point, float *out )
{
	mfaceinfo_t	*land = surf->texinfo->faceinfo;
	terrain_t		*terra;
	Vector		size;
	indexMap_t	*im;

	if( !land ) return;

	terra = land->terrain;
	if( !terra ) return;

	im = &terra->indexmap;

	for( int i = 0; i < 3; i++ )
		size[i] = land->maxs[i] - land->mins[i];

	out[2] = ( point[0] - land->mins[0] ) / size[0];
	out[3] = ( land->maxs[1] - point[1] ) / size[1];
}

/*
========================
R_GlobalCoords

fill vec2_t with global coords
========================
*/
void R_GlobalCoords( msurface_t *surf, const Vector &point, const Vector &absmin, const Vector &absmax, float scale, float *out )
{
	Vector	size;

	for( int i = 0; i < 3; i++ )
		size[i] = absmax[i] - absmin[i];

	out[2] = (( point[0] - absmin[0] ) / size[0]) * scale;
	out[3] = (( absmax[1] - point[1] ) / size[1]) * scale;
}

/*
========================
R_LightmapCoords

fill Vector4D with lightstyle coords (two styles per array)
========================
*/
void R_LightmapCoords( msurface_t *surf, const Vector &vec, float *coords, int style )
{
	mextrasurf_t	*esrf = surf->info;
	float		sample_size = Mod_SampleSizeForFace( surf );
	float		s, t;

	for( int i = 0; i < 2; i++ )
	{
		if( surf->styles[style+i] == LS_NONE )
			return; // end of styles

		s = DotProduct( vec, surf->info->lmvecs[0] ) + surf->info->lmvecs[0][3];
		s -= surf->info->lightmapmins[0];
		s += esrf->light_s[style+i] * sample_size;
		s += sample_size * 0.5f;
		s /= GL_BLOCK_SIZE * sample_size;

		t = DotProduct( vec, surf->info->lmvecs[1] ) + surf->info->lmvecs[1][3];
		t -= surf->info->lightmapmins[1];
		t += esrf->light_t[style+i] * sample_size;
		t += sample_size * 0.5f;
		t /= GL_BLOCK_SIZE * sample_size;

		coords[i*2+0] = s;
		coords[i*2+1] = t;
	}
}

/*
========================
R_LightmapCoords

fill Vector4D with lightstyle coords (two styles per array)
========================
*/
void R_LightmapCoords( mstudiosurface_t *surf, const Vector &vec, const Vector lmvecs[2], float *coords, int style )
{
	float	s, t;

	for( int i = 0; i < 2; i++ )
	{
		if( surf->styles[style+i] == LS_NONE )
			return; // end of styles

		s = DotProduct( vec, lmvecs[0] ) + surf->light_s[style+i] + 0.5f;
		t = DotProduct( vec, lmvecs[1] ) + surf->light_t[style+i] + 0.5f;
		s /= (float)GL_BLOCK_SIZE;
		t /= (float)GL_BLOCK_SIZE;

		coords[i*2+0] = s;
		coords[i*2+1] = t;
	}
}

/*
=================
R_UpdateSurfaceParams

update some surface params
if this was changed
=================
*/
void R_UpdateSurfaceParams( msurface_t *surf )
{
	mextrasurf_t *esrf = surf->info;
	cl_entity_t	*e = RI->currententity;
	model_t *clmodel = e->model;
	material_t *mat = R_TextureAnimation( surf )->material;

	// check for lightmap modification
	if( FBitSet( surf->flags, SURF_LM_UPDATE|SURF_DM_UPDATE ))
		R_UpdateLightMap( surf );

	if( FBitSet( surf->flags, SURF_MOVIE ))
		R_UpdateCinematic( surf );

	// handle conveyor movement
	if( FBitSet( clmodel->flags, BIT( 0 )) && FBitSet( surf->flags, SURF_CONVEYOR ) || FBitSet( mat->flags, BRUSH_CONVEYOR ))
	{
		float	flRate, flAngle;
		float	flWidth, flConveyorSpeed;
		float	sOffset, sy;
		float	tOffset, cy;

		flConveyorSpeed = (e->curstate.rendercolor.g<<8|e->curstate.rendercolor.b) / 16.0f;
		if( e->curstate.rendercolor.r ) flConveyorSpeed = -flConveyorSpeed;
		flWidth = (float)RENDER_GET_PARM( PARM_TEX_SRC_WIDTH, surf->texinfo->texture->gl_texturenum );

		if( flWidth != 0.0f )
		{
			flRate = abs( flConveyorSpeed ) / flWidth;
			flAngle = ( flConveyorSpeed >= 0.0f ) ? 180.0f : 0.0f;

			SinCos( DEG2RAD( flAngle ), &sy, &cy );
			sOffset = tr.time * cy * flRate;
			tOffset = tr.time * sy * flRate;
	
			// make sure that we are positive
			if( sOffset < 0.0f ) sOffset += 1.0f + -(int)sOffset;
			if( tOffset < 0.0f ) tOffset += 1.0f + -(int)tOffset;

			// make sure that we are in a [0,1] range
			sOffset = sOffset - (int)sOffset;
			tOffset = tOffset - (int)tOffset;

			esrf->texofs[0] = sOffset;
			esrf->texofs[1] = tOffset;
		}
		else
		{
			// no conveyor
 			esrf->texofs[0] = 0.0f;
 			esrf->texofs[1] = 0.0f;
		}
	}
	else
	{
		// no conveyor
 		esrf->texofs[0] = 0.0f;
 		esrf->texofs[1] = 0.0f;
	}
}

/*
=================
R_UpdateSurfaceParams

update some surface params
if this was changed
=================
*/
void R_UpdateSurfaceParams( mstudiosurface_t *surf )
{
	// check for lightmap modification
	if( FBitSet( surf->flags, SURF_LM_UPDATE|SURF_DM_UPDATE ))
		R_UpdateLightMap( surf );
}
