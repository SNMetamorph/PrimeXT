/*
r_lightmap.cpp - lightmaps management
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

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "pm_movevars.h"
#include "xash3d_features.h"
#include "mathlib.h"

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/
static void LM_InitBlock( void )
{
	gl_lightmap_t *lms = &tr.lightmaps[tr.current_lightmap_texture];
	memset( lms->allocated, 0, sizeof( lms->allocated ));
}

static int LM_AllocBlock( unsigned short w, unsigned short h, unsigned short *x, unsigned short *y )
{
	gl_lightmap_t	*lms = &tr.lightmaps[tr.current_lightmap_texture];
	unsigned short	i, j, best, best2;

	best = BLOCK_SIZE;

	for( i = 0; i < BLOCK_SIZE - w; i++ )
	{
		best2 = 0;

		for( j = 0; j < w; j++ )
		{
			if( lms->allocated[i+j] >= best )
				break;
			if( lms->allocated[i+j] > best2 )
				best2 = lms->allocated[i+j];
		}

		if( j == w )
		{	
			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if( best + h > BLOCK_SIZE )
	{
		// current lightmap is full
		lms->state = LM_DONE;
		return false;
          }

	for( i = 0; i < w; i++ )
		lms->allocated[*x + i] = best + h;
	lms->state = LM_USED; // lightmap in use

	return true;
}

static void LM_UploadPages( bool lightmap, bool deluxmap )
{
	char	lmName[16];
	int	i;

	for( i = 0; i < MAX_LIGHTMAPS && tr.lightmaps[i].state != LM_FREE; i++ )
	{
		gl_lightmap_t	*lms = &tr.lightmaps[i];

		if( lightmap && !lms->lightmap )
		{
			Q_snprintf( lmName, sizeof( lmName ), "*diffuse%i", i );
			lms->lightmap = CREATE_TEXTURE( lmName, BLOCK_SIZE, BLOCK_SIZE, NULL, TF_LIGHTMAP ); 
		}

		if( deluxmap && !lms->deluxmap )
		{
			Q_snprintf( lmName, sizeof( lmName ), "*normals%i", i );
			lms->deluxmap = CREATE_TEXTURE( lmName, BLOCK_SIZE, BLOCK_SIZE, NULL, TF_DELUXMAP );
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
	if( !surf->samples && !esrf->deluxemap )
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
	sm = esrf->shadowmap + size * style;

	// put into texture format
	stride = (smax * 4) - (smax << 2);

	for( t = 0; t < tmax; t++, dest += stride )
	{
		for( s = 0; s < smax; s++ )
		{
			dest[0] = TEXTURE_TO_TEXGAMMA( lm->r );
			dest[1] = TEXTURE_TO_TEXGAMMA( lm->g );
			dest[2] = TEXTURE_TO_TEXGAMMA( lm->b );
			if( esrf->shadowmap != NULL )
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
	if( !esrf->deluxemap || surf->styles[style] == LS_NONE )
		return;

	int sample_size = Mod_SampleSizeForFace( surf );
	smax = ( surf->info->lightextents[0] / sample_size ) + 1;
	tmax = ( surf->info->lightextents[1] / sample_size ) + 1;
	size = smax * tmax;

	// jump to specified style
	dm = esrf->deluxemap + size * style;

	// put into texture format
	stride = (smax * 4) - (smax << 2);

	for( t = 0; t < tmax; t++, dest += stride )
	{
		for( s = 0; s < smax; s++ )
		{
			dest[0] = dm->r;
			dest[1] = dm->g;
			dest[2] = dm->b;
			dest[3] = 255;	// unused

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
	byte		buf[132*132*4];
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

		R_ReLightGrass( surf, true );
	}

	ClearBits( surf->flags, SURF_LM_UPDATE );

	// upload the deluxemap
	if( esrf->deluxemap != NULL && FBitSet( surf->flags, SURF_DM_UPDATE ))
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
========================
R_TextureCoords

fill vec2_t with texture coords
========================
*/
void R_TextureCoords( msurface_t *surf, const Vector &vec, float *out )
{
	float	s, t;

	if( FBitSet( surf->flags, SURF_DRAWTURB ))
	{
		s = DotProduct( vec, surf->texinfo->vecs[0] );
		t = DotProduct( vec, surf->texinfo->vecs[1] );
	}
	else
	{
		s = DotProduct( vec, surf->texinfo->vecs[0] ) + surf->texinfo->vecs[0][3];
		s /= surf->texinfo->texture->width;
		t = DotProduct( vec, surf->texinfo->vecs[1] ) + surf->texinfo->vecs[1][3];
		t /= surf->texinfo->texture->height;
	}

	// fill the global coords with same
	out[0] = out[2] = s;
	out[1] = out[3] = t;
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
	Vector		size;

	if( !land || land->groupid == -1 )
		return;

	for( int i = 0; i < 3; i++ )
		size[i] = land->maxs[i] - land->mins[i];

	out[2] = ( point[0] - land->mins[0] ) / size[0];
	out[3] = ( land->maxs[1] - point[1] ) / size[1];
}

/*
========================
R_LightmapCoords

fill Vector4D with lightstyle coords (two styles per array)
========================
*/
void R_LightmapCoords( msurface_t *surf, const Vector &vec, float *coords, int style )
{
	float		sample_size = Mod_SampleSizeForFace( surf );
	mextrasurf_t	*esrf = surf->info;
	float		s, t;

	for( int i = 0; i < 2; i++ )
	{
		if( surf->styles[style+i] == LS_NONE )
			return; // end of styles

		s = DotProduct( vec, surf->info->lmvecs[0] ) + surf->info->lmvecs[0][3];
		s -= surf->info->lightmapmins[0];
		s += esrf->light_s[style+i] * sample_size;
		s += sample_size * 0.5f;
		s /= BLOCK_SIZE * sample_size;

		t = DotProduct( vec, surf->info->lmvecs[1] ) + surf->info->lmvecs[1][3];
		t -= surf->info->lightmapmins[1];
		t += esrf->light_t[style+i] * sample_size;
		t += sample_size * 0.5f;
		t /= BLOCK_SIZE * sample_size;

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
	mextrasurf_t	*esrf = surf->info;
	cl_entity_t	*e = RI->currententity;
	model_t		*clmodel = e->model;
	mtexinfo_t	*tx = surf->texinfo;
	mfaceinfo_t	*land = tx->faceinfo;
	texture_t		*tex;

	if( esrf->cached_frame == tr.realframecount )
		return; // already updated?

	// update texture animation if needs
	tex = R_TextureAnimation( surf );

	if( FBitSet( surf->flags, SURF_MOVIE ) && RI->currententity->curstate.body )
	{
		esrf->gl_texturenum = tr.cinTextures[esrf->cintexturenum-1];
	}
	else if( CVAR_TO_BOOL( r_lightmap ) || e->curstate.rendermode == kRenderTransColor )
	{
		esrf->gl_texturenum = tr.whiteTexture;
	}
	else if( FBitSet( surf->flags, SURF_LANDSCAPE ) && land && land->terrain )
	{
		if( land->terrain->layermap.gl_diffuse_id )
			esrf->gl_texturenum = land->terrain->layermap.gl_diffuse_id;
		else if( land->terrain->indexmap.gl_diffuse_id && CVAR_TO_BOOL( r_detailtextures ))
			esrf->gl_texturenum = tr.grayTexture;
		else esrf->gl_texturenum = tex->gl_texturenum;
	}
	else
	{
		esrf->gl_texturenum = tex->gl_texturenum;
	}

	if( land && land->terrain && land->terrain->indexmap.gl_diffuse_id != 0 )
		esrf->dt_texturenum = land->terrain->indexmap.gl_diffuse_id;
	else esrf->dt_texturenum = tex->dt_texturenum;

	// check for lightmap modification
	if( FBitSet( surf->flags, SURF_LM_UPDATE|SURF_DM_UPDATE ))
		R_UpdateLightMap( surf );

	if( FBitSet( surf->flags, SURF_MOVIE ))
		R_UpdateCinematic( surf );

	// handle conveyor movement
	if( FBitSet( clmodel->flags, MODEL_CONVEYOR ) && FBitSet( surf->flags, SURF_CONVEYOR ))
	{
		float	flRate, flAngle;
		float	flWidth, flConveyorSpeed;
		float	flConveyorPos;
		float	sOffset, sy;
		float	tOffset, cy;

		if( !FBitSet( e->curstate.effects, EF_CONVEYOR ))
		{
			flConveyorSpeed = (e->curstate.rendercolor.g<<8|e->curstate.rendercolor.b) / 16.0f;
			if( e->curstate.rendercolor.r ) flConveyorSpeed = -flConveyorSpeed;
		}
		else flConveyorPos = e->curstate.fuser1;
		flWidth = (float)RENDER_GET_PARM( PARM_TEX_SRC_WIDTH, surf->texinfo->texture->gl_texturenum );

		if( flWidth != 0.0f )
		{
			if( !FBitSet( e->curstate.effects, EF_CONVEYOR ))
                              {
				// additive speed not position-based
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
				// receive absolute position, not a speed
				flRate = fabs( flConveyorPos );
				flAngle = ( flConveyorPos >= 0.0f ) ? 180.0f : 0.0f;

				SinCos( DEG2RAD( flAngle ), &sy, &cy );
				esrf->texofs[0] = cy * flRate;
				esrf->texofs[1] = sy * flRate;

				// make sure that we are positive
				if( esrf->texofs[0] < 0.0f ) esrf->texofs[0] += 1.0f + -(int)esrf->texofs[0];
				if( esrf->texofs[1] < 0.0f ) esrf->texofs[1] += 1.0f + -(int)esrf->texofs[1];

				// make sure that we are in a [0,1] range
				esrf->texofs[0] = esrf->texofs[0] - (int)esrf->texofs[0];
				esrf->texofs[1] = esrf->texofs[1] - (int)esrf->texofs[1];
			}
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

	esrf->cached_frame = tr.realframecount;
}
