/*
skin.cpp - process studio skins
Copyright (C) 2017 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "port.h"
#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "studio.h"
#include "studiomdl.h"
#include "image_utils.h"
#include "builtin.h"

void Grab_Skin( s_texture_t *ptexture )
{
	bool use_default = false;
	char file1[1024];
	rgbdata_t *pic = nullptr;
	rgbdata_t *alphaMask = nullptr;

	// g-cont. moved here to avoid some bugs
	if( store_uv_coords && !FBitSet( ptexture->flags, STUDIO_NF_CHROME ))
		SetBits( ptexture->flags, STUDIO_NF_UV_COORDS );

	// use internal image
	if( !Q_stricmp( ptexture->name, "#white.bmp" ))
	{
		pic = ImageUtils::LoadImageMemory( ptexture->name, white_bmp, sizeof( white_bmp ));
	}
	else
	{
		if( cdtextureset )
		{
			int	i;

			for( i = 0; i < cdtextureset; i++ )
			{
				Q_snprintf( file1, sizeof( file1 ), "%s/%s", cdtexture[i], ptexture->name );
				if( COM_FileExists( file1 ))
					break;
			}

			if( i == cdtextureset )
				use_default = true;
		}
		else
		{
			Q_snprintf( file1, sizeof( file1 ), "%s/%s", cddir[numdirs], ptexture->name );
			if( !COM_FileExists( file1 ))
				use_default = true;
		}

		pic = ImageUtils::LoadImageFile( file1 );
		if( !pic ) MsgDev( D_ERROR, "unable to load %s\n", ptexture->name );
	}	

	// use emo-texture
	if( !pic ) pic = ImageUtils::LoadImageMemory( "default.bmp", default_bmp, sizeof( default_bmp ));
	if( !pic ) COM_FatalError( "%s not found", file1 ); // ???

	int new_width = Q_min(pic->width, store_uv_coords ? MIP_MAXWIDTH : 512);
	int new_height = Q_min(pic->height, store_uv_coords ? MIP_MAXHEIGHT : 512);
	bool transparent = FBitSet(ptexture->flags, STUDIO_NF_MASKED) && FBitSet(pic->flags, IMAGE_HAS_8BIT_ALPHA);

	// resample to studio limits
	pic = Image_Resample(pic, new_width, new_height);
	if (transparent) {
		alphaMask = Image_ExtractAlphaMask(pic);
	}
	pic = Image_Quantize(pic); // quantize if needs

	if (transparent)
	{
		Image_ApplyAlphaMask(pic, alphaMask, g_alpha_threshold);
		Image_Free(alphaMask);
	}

	ImageUtils::ApplyPaletteGamma(pic); // process gamma
	ptexture->srcwidth = pic->width;
	ptexture->srcheight = pic->height;
	ptexture->psrc = pic;
}

void ResizeTexture( s_texture_t *ptexture )
{
	int	i, j, s, t;
	int	srcadjwidth;
	byte	*pdest;

	// make the width a multiple of 4; some hardware requires this, and it ensures
	// dword alignment for each scan
	ptexture->skintop = ptexture->min_t;
	ptexture->skinleft = ptexture->min_s;
	ptexture->skinwidth = (int)((ptexture->max_s - ptexture->min_s) + 1 + 3) & ~3;
	ptexture->skinheight = (int)(ptexture->max_t - ptexture->min_t) + 1;

	ptexture->size = ptexture->skinwidth * ptexture->skinheight + 256 * 3;

	Msg( "%s [%d %d] (%.0f%%)  %6d bytes\n", ptexture->name, ptexture->skinwidth, ptexture->skinheight, 
		((ptexture->skinwidth * ptexture->skinheight) / (float)(ptexture->srcwidth * ptexture->srcheight)) * 100.0, ptexture->size );
	
	if( ptexture->size > 1024 * 1024 + 256 * 3 )
	{
		MsgDev( D_ERROR, "%.0f %.0f %.0f %.0f\n", ptexture->min_s, ptexture->max_s, ptexture->min_t, ptexture->max_t );
		COM_FatalError( "texture too large\n" );
	}

	pdest = (byte *)Mem_Alloc( ptexture->size );
	ptexture->pdata = pdest;

	// data is saved as a multiple of 4
	// NOTE: g-cont. we don't need to align width: it's already aligned
//	srcadjwidth = (ptexture->srcwidth + 3) & ~3;
	srcadjwidth = ptexture->srcwidth;
	t = ptexture->srcheight - ptexture->skinheight - ptexture->skintop + 10 * ptexture->srcheight;

	// move the picture data to the model area, replicating missing data, deleting unused data.
	for( i = 0; i < ptexture->skinheight; i++, t++ )
	{
		while( t >= ptexture->srcheight )
			t -= ptexture->srcheight;
		while( t < 0 )
			t += ptexture->srcheight;

		for( j = 0, s = ptexture->skinleft + 10 * ptexture->srcwidth; j < ptexture->skinwidth; j++, s++ )
		{
			while( s >= ptexture->srcwidth )
				s -= ptexture->srcwidth;
			*(pdest++) = *(ptexture->psrc->buffer + s + t * srcadjwidth);
		}
	}

	for( i = 0; i < 256; i++ )
	{
		// convert 32-bit palette to 24 bit palette
		pdest[i*3+0] = ptexture->psrc->palette[i*4+0];
		pdest[i*3+1] = ptexture->psrc->palette[i*4+1];
		pdest[i*3+2] = ptexture->psrc->palette[i*4+2];
	}

	Image_Free( ptexture->psrc );
	ptexture->psrc = NULL;
}

float adjust_texcoord( float coord )
{
	if((coord - floor( coord )) > 0.5 )
		return ceil( coord );
	return floor( coord );
}

// called for the base frame
void TextureCoordRanges( s_mesh_t *pmesh, s_texture_t *ptexture )
{
	int	i, j;

	if( FBitSet( ptexture->flags, STUDIO_NF_CHROME ))
	{
		ptexture->skintop = 0;
		ptexture->skinleft = 0;
		ptexture->skinwidth = (ptexture->srcwidth + 3) & ~3;
		ptexture->skinheight = ptexture->srcheight;

		for( i = 0; i < pmesh->numtris; i++ )
		{
			for( j = 0; j < 3; j++ )
			{
				pmesh->triangle[i][j].s = 0;
				pmesh->triangle[i][j].t = 0;
			}

			ptexture->max_s = 63;
			ptexture->min_s = 0;
			ptexture->max_t = 63;
			ptexture->min_t = 0;
		}
		return;
	}

	// check texture coords.
	if( !clip_texcoords && allow_tileing )
	{
		for( i = 0; i < pmesh->numtris; i++ )
		{
			for( j = 0; j < 3; j++ )
			{
				// texcoord U is out of range 0-1 so we can't pack them
				if( pmesh->triangle[i][j].u > 1.0 || pmesh->triangle[i][j].u < 0.0 )
					clip_texcoords = true;

				// texcoord V is out of range 0-1 so we can't pack them
				if( pmesh->triangle[i][j].v > 1.0 || pmesh->triangle[i][j].v < 0.0 )
					clip_texcoords = true;
			}

			// tileing detected
			if( clip_texcoords )
			{
				MsgDev( D_INFO, "UV mapping is out of range 0-1, texture clipping enabled\n" );
				break;
			}
		}
	}

	// pack texture coords
	if( !clip_texcoords )
	{
		int	k, n;

		// prevent to expand texture too much
		for( i = 0; i < pmesh->numtris; i++ )
		{
			for( j = 0; j < 3; j++ )
			{
				if( pmesh->triangle[i][j].u > 4.0f ) pmesh->triangle[i][j].u = 4.0f;
				if( pmesh->triangle[i][j].u < -3.0f ) pmesh->triangle[i][j].u = -3.0f;
				if( pmesh->triangle[i][j].v > 4.0f ) pmesh->triangle[i][j].v = 4.0f;
				if( pmesh->triangle[i][j].v < -3.0f ) pmesh->triangle[i][j].v = -3.0f;
			}
		}

		// clamp U-coord
		while( 1 )
		{
			float min_u = 9999.0f;
			float max_u =-9999.0f;
			float k_max_u = -9999.0f, n_min_u = 9999.0f;
			k = n = -1;

			for( i = 0; i < pmesh->numtris; i++ ) 
			{
				float	local_min, local_max;

				local_min = Q_min( pmesh->triangle[i][0].u, Q_min( pmesh->triangle[i][1].u, pmesh->triangle[i][2].u ));
				local_max = Q_max( pmesh->triangle[i][0].u, Q_max( pmesh->triangle[i][1].u, pmesh->triangle[i][2].u ));

				if( local_min < min_u )
				{
					min_u = local_min;
					k_max_u = local_max;
					k = i;
				}

				if( local_max > max_u )
				{
					max_u = local_max;
					n_min_u = local_min;
					n = i;
				}
			}

			if( k_max_u + 1.0 < max_u )
			{
				for( j = 0; j < 3; j++ )
					pmesh->triangle[k][j].u += 1.0f;
			}
			else if( n_min_u - 1.0 > min_u )
			{
				for( j = 0; j < 3; j++ )
					pmesh->triangle[n][j].u -= 1.0f;
			}
			else
			{
				break;
			}
		}

		// clamp V-coord
		while( 1 )
		{
			float min_v = 9999.0f;
			float max_v =-9999.0f;
			float k_max_v = -9999.0f, n_min_v = 9999.0f;
			k = n = -1;

			for( i = 0; i < pmesh->numtris; i++ ) 
			{
				float	local_min, local_max;

				local_min = Q_min( pmesh->triangle[i][0].v, Q_min( pmesh->triangle[i][1].v, pmesh->triangle[i][2].v ));
				local_max = Q_max( pmesh->triangle[i][0].v, Q_max( pmesh->triangle[i][1].v, pmesh->triangle[i][2].v ));

				if( local_min < min_v )
				{
					min_v = local_min;
					k_max_v = local_max;
					k = i;
				}

				if( local_max > max_v )
				{
					max_v = local_max;
					n_min_v = local_min;
					n = i;
				}
			}

			if( k_max_v + 1.0 < max_v )
			{
				for( j = 0; j < 3; j++ )
					pmesh->triangle[k][j].v += 1.0f;
			}
			else if( n_min_v - 1.0 > min_v )
			{
				for( j = 0; j < 3; j++ )
					pmesh->triangle[n][j].v -= 1.0f;
			}
			else
			{
				break;
			}
		}
	}
	else if( !allow_tileing )
	{
		for( i = 0; i < pmesh->numtris; i++ ) 
		{
			for( j = 0; j < 3; j++ ) 
			{
				if( pmesh->triangle[i][j].u < 0.0f ) pmesh->triangle[i][j].u = 0.0f;
				if( pmesh->triangle[i][j].u > 1.0f ) pmesh->triangle[i][j].u = 1.0f;
				if( pmesh->triangle[i][j].v < 0.0f ) pmesh->triangle[i][j].v = 0.0f;
				if( pmesh->triangle[i][j].v > 1.0f ) pmesh->triangle[i][j].v = 1.0f;
			}
		}
	}

	// convert to pixel coordinates
	for( i = 0; i < pmesh->numtris; i++ )
	{
		for( j = 0; j < 3; j++ )
		{
			// FIXME losing texture coord resultion!
			pmesh->triangle[i][j].s = adjust_texcoord( pmesh->triangle[i][j].u * ptexture->srcwidth );
			pmesh->triangle[i][j].t = adjust_texcoord( pmesh->triangle[i][j].v * ptexture->srcheight );
		}
	}

	// find the range
	if( !clip_texcoords )
	{
		for( i = 0; i < pmesh->numtris; i++ )
		{
			for( j = 0; j < 3; j++ )
			{
				ptexture->max_s = Q_max( pmesh->triangle[i][j].s, ptexture->max_s );
				ptexture->min_s = Q_min( pmesh->triangle[i][j].s, ptexture->min_s );
				ptexture->max_t = Q_max( pmesh->triangle[i][j].t, ptexture->max_t );
				ptexture->min_t = Q_min( pmesh->triangle[i][j].t, ptexture->min_t );
			}
		}
	}
	else
	{
		ptexture->max_s = ptexture->srcwidth - 1;
		ptexture->min_s = 0;
		ptexture->max_t = ptexture->srcheight - 1;
		ptexture->min_t = 0;
	}
}

void ResetTextureCoordRanges( s_mesh_t *pmesh, s_texture_t *ptexture  )
{
	// adjust top, left edge
	for (int i = 0; i < pmesh->numtris; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			pmesh->triangle[i][j].s -= ptexture->min_s;

			// quake wants t inverted
			if (!clip_texcoords)
			{
				pmesh->triangle[i][j].t = (ptexture->max_t - ptexture->min_t) - (pmesh->triangle[i][j].t - ptexture->min_t);
			}
			else
			{
				pmesh->triangle[i][j].t = (ptexture->max_t + 1 - ptexture->min_t) - (pmesh->triangle[i][j].t - ptexture->min_t);
			}
			if (store_uv_coords) {
				pmesh->triangle[i][j].v = 1.0f - pmesh->triangle[i][j].v;
			}
		}
	}
}

void SetSkinValues( void )
{
	int	i, j;

	for( i = 0; i < g_numtextures; i++ )
	{
		Grab_Skin ( &g_texture[i] );

		g_texture[i].max_s = -9999999;
		g_texture[i].min_s = 9999999;
		g_texture[i].max_t = -9999999;
		g_texture[i].min_t = 9999999;
	}

	for( i = 0; i < g_nummodels; i++ )
	{
		for( j = 0; j < g_model[i]->nummesh; j++ ) 
		{
			TextureCoordRanges( g_model[i]->pmesh[j], &g_texture[g_model[i]->pmesh[j]->skinref] );
		}
	}

	for( i = 0; i < g_numtextures; i++ )
	{
		if( g_texture[i].max_s < g_texture[i].min_s )
		{
			// must be a replacement texture
			if( FBitSet( g_texture[i].flags, STUDIO_NF_CHROME ))
			{
				g_texture[i].max_s = 63;
				g_texture[i].min_s = 0;
				g_texture[i].max_t = 63;
				g_texture[i].min_t = 0;
			}
			else
			{
				g_texture[i].max_s = g_texture[g_texture[i].parent].max_s;
				g_texture[i].min_s = g_texture[g_texture[i].parent].min_s;
				g_texture[i].max_t = g_texture[g_texture[i].parent].max_t;
				g_texture[i].min_t = g_texture[g_texture[i].parent].min_t;
			}
		}

		ResizeTexture( &g_texture[i] );
	}

	for( i = 0; i < g_nummodels; i++ )
	{
		for (j = 0; j < g_model[i]->nummesh; j++) 
		{
			ResetTextureCoordRanges( g_model[i]->pmesh[j], &g_texture[g_model[i]->pmesh[j]->skinref] );
		}
	}

	// build texture groups
	for( i = 0; i < MAXSTUDIOSKINS; i++ )
	{
		for( j = 0; j < MAXSTUDIOSKINS; j++ )
		{
			g_skinref[i][j] = j;
		}
	}

	for( i = 0; i < g_numtexturelayers[0]; i++ )
	{
		for( j = 0; j < g_numtexturereps[0]; j++ )
		{
			g_skinref[i][g_texturegroup[0][0][j]] = g_texturegroup[0][i][j];
		}
	}

	if( i != 0 )
	{
		g_numskinfamilies = i;
	}
	else
	{
		g_numskinref = g_numtextures;
		g_numskinfamilies = 1;
	}
}
