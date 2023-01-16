/*
miptex.cpp - prepare and store miptex into the wad
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "conprint.h"
#include "cmdlib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "imagelib.h"
#include "makewad.h"
#include "miptex.h"
#include "port.h"
#include <math.h>

byte	lbmpalette[256*3];
float	linearpalette[256][3];
float 	d_red, d_green, d_blue;
bool	color_used[256];
float	maxdistortion;
int	colors_used;
byte	pixdata[256];

/*
=============
MIP_AddColor

add unique color and restore original gamma
=============
*/
byte MIP_AddColor( float r, float g, float b )
{
	// one color as reserved for transparent
	for( int i = 0; i < 255; i++ )
	{
		if( !color_used[i] )
		{
			linearpalette[i][0] = r;
			linearpalette[i][1] = g;
			linearpalette[i][2] = b;

			r = bound( 0.0f, r, 1.0f );
			lbmpalette[i*3+0] = (byte)pow( r, INVGAMMA ) * 255;
			g = bound( 0.0f, g, 1.0f );
			lbmpalette[i*3+1] = (byte)pow( g, INVGAMMA ) * 255;
			r = bound( 0.0f, r, 1.0f );
			lbmpalette[i*3+2] = (byte)pow( b, INVGAMMA ) * 255;

			color_used[i] = true;
			colors_used++;

			return i;
		}
	}

	return 0;
}

/*
=============
MIP_AveragePixels

average pixels for mip-mapping
=============
*/
byte MIP_AveragePixels( int count )
{
	float 	bestdistortion, distortion;
	float 	r, g, b, dr, dg, db;
	int	i, pix, bestcolor;

	r = g = b = 0.0f;

	for( i = 0; i < count; i++ )
	{
		pix = pixdata[i];
		r += linearpalette[pix][0];
		g += linearpalette[pix][1];
		b += linearpalette[pix][2];
	}

	r /= count;
	g /= count;
	b /= count;

	r += d_red;
	g += d_green;
	b += d_blue;
	
	// find the best color
	bestdistortion = 3.0;
	bestcolor = -1;

	for( i = 0; i < 255; i++ )
	{
		if( color_used[i] )
		{
			pix = i;
			dr = r - linearpalette[i][0];
			dg = g - linearpalette[i][1];
			db = b - linearpalette[i][2];

			distortion = (dr * dr) + (dg * dg) + (db * db);

			if( distortion < bestdistortion )
			{
				if( !distortion )
				{
					d_red = d_green = d_blue = 0.0f;	// no distortion yet
					return pix;			// perfect match
				}

				bestdistortion = distortion;
				bestcolor = pix;
			}
		}
	}

	if( bestdistortion > 0.001f && colors_used < 255 )
	{
		bestcolor = MIP_AddColor( r, g, b );
		d_red = d_green = d_blue = 0.0f;
		bestdistortion = 0.0f;
	}
	else
	{
		// error diffusion
		d_red = r - linearpalette[bestcolor][0];
		d_green = g - linearpalette[bestcolor][1];
		d_blue = b - linearpalette[bestcolor][2];
	}

	if( bestdistortion > maxdistortion )
		maxdistortion = bestdistortion;

	return bestcolor;
}

bool MIP_WriteMiptex( const char *lumpname, rgbdata_t *pix )
{
	char	tmpname[64];
	mip_t	*mip;

	// check for all the possible problems
	if (!pix || !FBitSet(pix->flags, IMAGE_QUANTIZED)) {
		Msg(S_ERROR "lump with this name already exists or image not quantized\n");
		return false;
	}

	if ((pix->width & 7) || (pix->height & 7)) {
		Msg(S_ERROR "image width/height not aligned by 16\n");
		return false; // not aligned by 16
	}

	if (pix->width < IMAGE_MINWIDTH || pix->width > IMAGE_MAXWIDTH || pix->height < IMAGE_MINHEIGHT || pix->height > IMAGE_MAXHEIGHT) {
		Msg(S_ERROR "image too small or too large\n");
		return false; // to small or too large
	}

	// calculate gamma corrected linear palette
	for( int i = 0; i < 256; i++ )
	{
		// setup palette
		lbmpalette[i*3+0] = pix->palette[i*4+0];
		lbmpalette[i*3+1] = pix->palette[i*4+1];
		lbmpalette[i*3+2] = pix->palette[i*4+2];

		for( int j = 0; j < 3; j++ )
		{
			float f = lbmpalette[i*3+j] / 255.0f;
			linearpalette[i][j] = pow( f, GAMMA ); // assume textures are done at 2.2, we want to remap them at 1.0
		}
	}

	char hint = W_HintFromSuf( lumpname );
	bool all_colors = false;

	size_t pixels = pix->width * pix->height;
	size_t lumpsize = sizeof( mip_t ) + (( pixels * 85 )>>6) + sizeof( short ) + 768;
	byte *lumpbuffer, *lump_p;

	// all the lumps must be aligned by 4
	// or Wally will stop working properly
	lumpsize = (lumpsize + 3) & ~3;
	maxdistortion = 0.0f;

	if( FBitSet( pix->flags, IMAGE_HAS_1BIT_ALPHA ))
		all_colors = true;

	if( hint == IMG_NORMALMAP )
		all_colors = true;

	if( all_colors )
	{
		// assume palette full for some reasons
		for( int i = 0; i < 256; i++ )
			color_used[i] = true;
		colors_used = 256;
	}
	else
	{
		// figure out what palette entries are actually used
		for( int i = 0; i < 256; i++ )
			color_used[i] = false;
		colors_used = 0;

		for( int x = 0; x < pix->width; x++ )
		{
			for( int y = 0; y < pix->height; y++ )
			{
				int color = pix->buffer[y * pix->width + x];

				if( !color_used[color] )
				{
					color_used[color] = true;
					colors_used++;
				}
			}
		}

		MsgDev( D_REPORT, "%s (colors %i)\n", lumpname, colors_used );
	}

	lumpbuffer = lump_p = (byte *)Mem_Alloc( lumpsize );
	mip = (mip_t *)lumpbuffer;

	// prepare lump name
	Q_strncpy( tmpname, lumpname, sizeof( tmpname ));
	if( hint != IMG_DIFFUSE ) tmpname[Q_strlen( tmpname ) - HINT_NAMELEN] = '\0'; // kill the suffix

	// fill the header
	Q_strncpy( mip->name, tmpname, WAD3_NAMELEN );
	mip->width = pix->width;
	mip->height = pix->height;
	mip->offsets[0] = sizeof( mip_t );
	lump_p += sizeof( mip_t );

	// write miptex
	memcpy( lump_p, pix->buffer, pixels );
	lump_p += pixels;

	// subsample for greater mip levels
	for( int miplevel = 1; miplevel < 4; miplevel++ )
	{
		int mipstep = BIT( miplevel );
		int pixTest = (int)((float)(mipstep * mipstep) * 0.4f ); // 40% of pixels

		mip->offsets[miplevel] = lump_p - (byte *)mip;
		d_red = d_green = d_blue = 0.0f; // no distortion yet

		for( int y = 0; y < pix->height; y += mipstep )
		{
			for( int x = 0; x < pix->width; x += mipstep )
			{
				int count = 0;

				for( int yy = 0; yy < mipstep; yy++ )
				{
					for( int xx = 0; xx < mipstep; xx++ )
					{
						byte testpixel = pix->buffer[(y + yy) * pix->width + x + xx];
						
						// if 255 is not transparent, or this isn't a transparent pixel,
						// add it in to the image filter
						if( !FBitSet( pix->flags, IMAGE_HAS_1BIT_ALPHA ) || testpixel != 255 )
						{
							pixdata[count] = testpixel;
							count++;
						}
					}
				}

				// solid pixels account for < 40% of this pixel, make it transparent
				if( count > pixTest )
					*lump_p++ = MIP_AveragePixels( count );
				else *lump_p++ = 255;
			}	
		}
	}

	// write out palette
	*(unsigned short *)lump_p = 256; // palette size
	lump_p += sizeof( short );

	memcpy( lump_p, lbmpalette, 768 );
	lump_p += 768;

	size_t disksize = (( lump_p - lumpbuffer ) + 3) & ~3;

	if (lumpsize != disksize) {
		MsgDev(D_ERROR, "%s is corrupted (buffer is %s bytes, written %s)\n", lumpname, Q_memprint(lumpsize), Q_memprint(disksize));
	}

	bool result = W_SaveLump( output_wad, lumpname, lumpbuffer, lumpsize, TYP_MIPTEX, ATTR_NONE ) >= 0;

	Mem_Free( lumpbuffer );

	return result;
}

bool MIP_CheckForReplace( dlumpinfo_t *find, rgbdata_t *image, int &width, int &height )
{
	// NOTE: we can replace this lump but this is unsafe
	if( find != NULL )
	{
		size_t lumpsize = ((sizeof( mip_t ) + ((( width * height ) * 85 )>>6) + sizeof( short ) + 768) + 3) & ~3;

		switch( GetReplaceLevel( ))
		{
		case REP_IGNORE:
			Msg(S_ERROR "%s already exists\n", find->name); 
			Image_Free( image );
			return false;
		case REP_NORMAL:
			if( FBitSet( find->attribs, ATTR_READONLY ))
			{
				// g-cont. i left this limitation as a protect of the replacement of compressed lumps
				Msg(S_ERROR "%s is read-only\n", find->name);
				Image_Free( image );
				return false;
			}
			if( lumpsize != find->size )
			{
				Msg(S_ERROR "%s.mip [%s] should be [%s]\n",
				find->name, Q_memprint( lumpsize ), Q_memprint( find->size )); 
				Image_Free( image );
				return false;
			}
			break;
		case REP_FORCE:
			if( lumpsize != find->size )
			{
				size_t oldpos;
				mip_t test;

				oldpos = tell( W_GetHandle( output_wad ) ); // don't forget restore original position

				if( lseek( W_GetHandle( output_wad ), find->filepos, SEEK_SET ) == -1 )
				{
					Msg(S_ERROR "%s is corrupted\n", find->name );
					lseek( W_GetHandle( output_wad ), oldpos, SEEK_SET );
					Image_Free( image );
					return false;
				}

				if( read( W_GetHandle( output_wad ), &test, sizeof( test )) != sizeof( test ))
				{
					Msg(S_ERROR "%s is corrupted\n", find->name );
					lseek( W_GetHandle( output_wad ), oldpos, SEEK_SET );
					Image_Free( image );
					return false;
				}

				// get new sizes from the saved lump to resample current
				lseek( W_GetHandle( output_wad ), oldpos, SEEK_SET );
				height = test.height;
				width = test.width;
			}
			break;
		}
	}

	return true;
}
